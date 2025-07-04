/* mkjson.c - a part of mkjson library
 *
 * Copyright (C) 2018 Jacek Wieczorek
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.	See the LICENSE file for details.
 */

#include "mkjson.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

// Works like asprintf, but it's always there
// I don't want the name to collide with anything
static int allsprintf( char **strp, const char *fmt, ... )
{
	int len;
	va_list ap;
	va_start( ap, fmt );

	#ifdef _GNU_SOURCE
		// Just hand everything to vasprintf, if it's available
		len = vasprintf( strp, fmt, ap );
	#else
		// Or do it the manual way
		char *buf;
		len = vsnprintf( NULL, 0, fmt, ap );
		if ( len >= 0 )
		{
			buf = malloc( ++len );
			if ( buf != NULL )
			{
				// Hopefully, that's the right way to do it
				va_end( ap );
				va_start( ap, fmt );

				// Write and return the data
				len = vsnprintf( buf, len, fmt, ap );
				if ( len >= 0 )
				{
					*strp = buf;
				}
				else
				{
					free( buf );
				}
			}
		}
	#endif

	va_end( ap );
	return len;
}

// Return JSON string built from va_arg arguments
// If no longer needed, should be passed to free() by user
char *mkjson( enum mkjson_container_type otype, int count, ... )
{
	int i, len, goodchunks = 0, failure = 0;
	char *json, *prefix, **chunks, ign;

	// Value - type and data
	enum mkjson_value_type vtype;
	const char *key;
	long long int intval;
	long double dblval;
	const char *strval;

	// Since v0.9 count cannot be a negative value and datatype is indicated by a separate argument
	// Since I'm not sure whether it's right to put assertions in libraries, the next line is commented out
	// assert( count >= 0 && "After v0.9 negative count is prohibited; please use otype argument instead" );
	if ( count < 0 || ( otype != MKJSON_OBJ && otype != MKJSON_ARR ) ) return NULL;

	// Allocate chunk pointer array - on standard platforms each one should be NULL
	chunks = calloc( count, sizeof( char* ) );
	if ( chunks == NULL ) return NULL;

	// This should rather be at the point of no return
	va_list ap;
	va_start( ap, count );

	// Create chunks
	for ( i = 0; i < count && !failure; i++ )
	{
		// Get value type
		vtype = va_arg( ap, enum mkjson_value_type );

		// Get key
		if ( otype == MKJSON_OBJ )
		{
			key = va_arg( ap, char* );
			if ( key == NULL )
			{
				failure = 1;
				break;
			}
		}
		else key = "";

		// Generate prefix
		if ( allsprintf( &prefix, "%s%s%s",
			otype == MKJSON_OBJ ? "\"" : "",            // Quote before key
			key,                                        // Key
			otype == MKJSON_OBJ ? "\": " : "" ) == -1 ) // Quote and colon after key
		{
			failure = 1;
			break;
		}

		// Depending on value type
		ign = 0;
		switch ( vtype )
		{
			// Ignore string / JSON data
			case MKJSON_IGN_STRING:
			case MKJSON_IGN_JSON:
				(void) va_arg( ap, const char* );
				ign = 1;
				break;

			// Ignore string / JSON data and pass the pointer to free
			case MKJSON_IGN_STRING_FREE:
			case MKJSON_IGN_JSON_FREE:
				free( va_arg( ap, char* ) );
				ign = 1;
				break;

			// Ignore int / long long int
			case MKJSON_IGN_INT:
			case MKJSON_IGN_LLINT:
				if ( vtype == MKJSON_IGN_INT )
					(void) va_arg( ap, int );
				else
					(void) va_arg( ap, long long int );
				ign = 1;
				break;

			// Ignore double / long double
			case MKJSON_IGN_DOUBLE:
			case MKJSON_IGN_LDOUBLE:
				if ( vtype == MKJSON_IGN_DOUBLE )
					(void) va_arg( ap, double );
				else
					(void) va_arg( ap, long double );
				ign = 1;
				break;

			// Ignore boolean
			case MKJSON_IGN_BOOL:
				(void) va_arg( ap, int );
				ign = 1;
				break;

			// Ignore null value
			case MKJSON_IGN_NULL:
				ign = 1;
				break;

			// A null-terminated string
			case MKJSON_STRING:
			case MKJSON_STRING_FREE:
				strval = va_arg( ap, const char* );

				// If the pointer points to NULL, the string will be replaced with JSON null value
				if ( strval == NULL )
				{
					if ( allsprintf( chunks + i, "%snull", prefix ) == -1 )
						chunks[i] = NULL;
				}
				else
				{
					if ( allsprintf( chunks + i, "%s\"%s\"", prefix, strval ) == -1 )
						chunks[i] = NULL;
				}

				// Optional free
				if ( vtype == MKJSON_STRING_FREE )
					free( (char*) strval );
				break;

			// Embed JSON data
			case MKJSON_JSON:
			case MKJSON_JSON_FREE:
				strval = va_arg( ap, const char* );

				// If the pointer points to NULL, the JSON data is replaced with null value
				if ( allsprintf( chunks + i, "%s%s", prefix, strval == NULL ? "null" : strval ) == -1 )
					chunks[i] = NULL;

				// Optional free
				if ( vtype == MKJSON_JSON_FREE )
					free( (char*) strval );
				break;

			// int / long long int
			case MKJSON_INT:
			case MKJSON_LLINT:
				if ( vtype == MKJSON_INT )
					intval = va_arg( ap, int );
				else
					intval = va_arg( ap, long long int );

				if ( allsprintf( chunks + i, "%s%lld", prefix, intval ) == -1 ) chunks[i] = NULL;
				break;

			// double / long double
			case MKJSON_DOUBLE:
			case MKJSON_LDOUBLE:
				if ( vtype == MKJSON_DOUBLE )
					dblval = va_arg( ap, double );
				else
					dblval = va_arg( ap, long double );

				if ( allsprintf( chunks + i, "%s%Lf", prefix, dblval ) == -1 ) chunks[i] = NULL;
				break;

			// double / long double
			case MKJSON_SCI_DOUBLE:
			case MKJSON_SCI_LDOUBLE:
				if ( vtype == MKJSON_SCI_DOUBLE )
					dblval = va_arg( ap, double );
				else
					dblval = va_arg( ap, long double );

				if ( allsprintf( chunks + i, "%s%Le", prefix, dblval ) == -1 ) chunks[i] = NULL;
				break;

			// Boolean
			case MKJSON_BOOL:
				intval = va_arg( ap, int );
				if ( allsprintf( chunks + i, "%s%s", prefix, intval ? "true" : "false" ) == -1 ) chunks[i] = NULL;
				break;

			// JSON null
			case MKJSON_NULL:
				if ( allsprintf( chunks + i, "%snull", prefix ) == -1 ) chunks[i] = NULL;
				break;

			// Bad type specifier
			default:
				chunks[i] = NULL;
				break;
		}

		// Free prefix memory
		free( prefix );

		// NULL chunk without ignore flag indicates failure
		if ( !ign && chunks[i] == NULL ) failure = 1;

		// NULL chunk now indicates ignore flag
		if ( ign ) chunks[i] = NULL;
		else goodchunks++;
	}

	// We won't use ap anymore
	va_end( ap );

	// If everything is fine, merge chunks and create full JSON table
	if ( !failure )
	{
		// Get total length (this is without NUL byte)
		len = 0;
		for ( i = 0; i < count; i++ )
			if ( chunks[i] != NULL )
				len += strlen( chunks[i] );

		// Total length = Chunks length + 2 brackets + separators
		if ( goodchunks == 0 ) goodchunks = 1;
		len = len + 2 + ( goodchunks - 1 ) * 2;

		// Allocate memory for the whole thing
		json = calloc( len + 1, sizeof( char ) );
		if ( json != NULL )
		{
			// Merge chunks (and do not overwrite the first bracket)
			for ( i = 0; i < count; i++ )
			{
				// Add separators:
				// - not on the begining
				// - always after valid chunk
				// - between two valid chunks
				// - between valid and ignored chunk if the latter isn't the last one
				if ( i != 0 && chunks[i - 1] != NULL && ( chunks[i] != NULL || ( chunks[i] == NULL && i != count - 1 ) ) )
					strcat( json + 1, ", ");

				if ( chunks[i] != NULL )
					strcat( json + 1, chunks[i] );
			}

			// Add proper brackets
			json[0] = otype == MKJSON_OBJ ? '{' : '[';
			json[len - 1] = otype == MKJSON_OBJ ? '}' : ']';
		}
	}
	else json = NULL;

	// Free chunks
	for ( i = 0; i < count; i++ )
		free( chunks[i] );
	free( chunks );

	return json;
}


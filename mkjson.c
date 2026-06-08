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
#include <stdint.h>

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

// Calculate the escaped length of a string (excluding null terminator)
static size_t json_escaped_len(const char *str)
{
	size_t len = 0;
	
	if (!str) return 4; // "null"
	
	for (const char *p = str; *p; p++) {
		switch (*p) {
			case '"':
			case '\\':
			case '\b':
			case '\f':
			case '\n':
			case '\r':
			case '\t':
				len += 2; // Escaped char: \X
				break;
			default:
				if ((unsigned char)*p < 0x20) {
					len += 6; // Unicode escape: \uXXXX
				} else {
					len += 1; // Regular char
				}
				break;
		}
	}
	
	return len;
}

// Escape a string for JSON. Caller must free the returned string
static char *json_escape_string(const char *str)
{
	if (!str) {
		char *null_str = malloc(5);
		if (null_str) strcpy(null_str, "null");
		return null_str;
	}
	
	size_t escaped_len = json_escaped_len(str);
	char *escaped = malloc(escaped_len + 1);
	if (!escaped) return NULL;
	
	char *dst = escaped;
	for (const char *p = str; *p; p++) {
		switch (*p) {
			case '"':  *dst++ = '\\'; *dst++ = '"'; break;
			case '\\': *dst++ = '\\'; *dst++ = '\\'; break;
			case '\b': *dst++ = '\\'; *dst++ = 'b'; break;
			case '\f': *dst++ = '\\'; *dst++ = 'f'; break;
			case '\n': *dst++ = '\\'; *dst++ = 'n'; break;
			case '\r': *dst++ = '\\'; *dst++ = 'r'; break;
			case '\t': *dst++ = '\\'; *dst++ = 't'; break;
			default:
				if ((unsigned char)*p < 0x20) {
					// Control characters: use \uXXXX
					sprintf(dst, "\\u%04x", (unsigned char)*p);
					dst += 6;
				} else {
					*dst++ = *p;
				}
				break;
		}
	}
	*dst = '\0';
	
	return escaped;
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
		if ( otype == MKJSON_OBJ )
		{
			// Escape the key for object mode
			char *escaped_key = json_escape_string( key );
			if ( !escaped_key )
			{
				failure = 1;
				break;
			}
			int ret = allsprintf( &prefix, "\"%s\": ", escaped_key );
			free( escaped_key );
			if ( ret == -1 )
			{
				failure = 1;
				break;
			}
		}
		else
		{
			// Array mode - no prefix needed
			if ( allsprintf( &prefix, "" ) == -1 )
			{
				failure = 1;
				break;
			}
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
					char *escaped = json_escape_string( strval );
					if ( escaped )
					{
						if ( allsprintf( chunks + i, "%s\"%s\"", prefix, escaped ) == -1 )
							chunks[i] = NULL;
						free( escaped );
					}
					else
					{
						chunks[i] = NULL;
					}
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

// Helper function for pretty printing with indentation
static char *mkjson_array_internal( enum mkjson_container_type otype, const mkjson_arg *args, int indent_size, int current_depth );

// Null-terminated array version of mkjson
char *mkjson_array( enum mkjson_container_type otype, const mkjson_arg *args )
{
	return mkjson_array_internal(otype, args, 0, 0);
}

// Pretty-printed version with indentation
char *mkjson_array_pretty( enum mkjson_container_type otype, const mkjson_arg *args, int indent_size )
{
	return mkjson_array_internal(otype, args, indent_size, 0);
}

// Internal implementation that handles both pretty and compact output
static char *mkjson_array_internal( enum mkjson_container_type otype, const mkjson_arg *args, int indent_size, int current_depth )
{
	// Pretty printing helpers
	int pretty = (indent_size > 0);
	char *indent = NULL;
	char *newline = pretty ? "\n" : "";
	
	// Create indent string for current depth
	if (pretty) {
		int indent_len = indent_size * current_depth;
		indent = malloc(indent_len + 1);
		if (!indent) return NULL;
		memset(indent, ' ', indent_len);
		indent[indent_len] = '\0';
	} else {
		indent = "";
	}
	
	// Create indent for nested content
	char *nested_indent = NULL;
	if (pretty) {
		int nested_len = indent_size * (current_depth + 1);
		nested_indent = malloc(nested_len + 1);
		if (!nested_indent) {
			if (pretty) free(indent);
			return NULL;
		}
		memset(nested_indent, ' ', nested_len);
		nested_indent[nested_len] = '\0';
	} else {
		nested_indent = "";
	}
	
	// Count arguments
	int count = 0;
	if (args != NULL) {
		for (const mkjson_arg *arg = args; arg->type != 0; arg++) {
			count++;
		}
	}
	
	// Allocate space for varargs
	// We need 3 values per argument for objects (type, key, value)
	// or 2 values per argument for arrays (type, value)
	int vararg_count = count * (otype == MKJSON_OBJ ? 3 : 2);
	void **varargs = calloc(vararg_count, sizeof(void*));
	if (varargs == NULL) return NULL;
	
	// Convert array to varargs format
	int idx = 0;
	for (int i = 0; i < count; i++) {
		const mkjson_arg *arg = &args[i];
		
		// Add type
		varargs[idx++] = (void*)(intptr_t)arg->type;
		
		// Add key for objects
		if (otype == MKJSON_OBJ) {
			varargs[idx++] = (void*)arg->key;
		}
		
		// Add value based on type
		switch (arg->type) {
			case MKJSON_STRING:
			case MKJSON_STRING_FREE:
			case MKJSON_JSON:
			case MKJSON_JSON_FREE:
				varargs[idx++] = (void*)arg->value.str_val;
				break;
			case MKJSON_INT:
				varargs[idx++] = (void*)(intptr_t)arg->value.int_val;
				break;
			case MKJSON_LLINT:
				varargs[idx++] = (void*)(intptr_t)arg->value.llint_val;
				break;
			case MKJSON_DOUBLE:
				// For double, we need to pass by value, which is tricky
				// Let's use a different approach
				break;
			case MKJSON_LDOUBLE:
				// Same issue as double
				break;
			case MKJSON_BOOL:
				varargs[idx++] = (void*)(intptr_t)arg->value.bool_val;
				break;
			case MKJSON_NULL:
				// No value needed
				idx--;
				break;
			default:
				// Ignore types
				if (arg->type < 0) {
					// Handle based on the positive type
					switch (-arg->type) {
						case MKJSON_STRING:
						case MKJSON_STRING_FREE:
						case MKJSON_JSON:
						case MKJSON_JSON_FREE:
							varargs[idx++] = (void*)arg->value.str_val;
							break;
						case MKJSON_INT:
							varargs[idx++] = (void*)(intptr_t)arg->value.int_val;
							break;
						case MKJSON_LLINT:
							varargs[idx++] = (void*)(intptr_t)arg->value.llint_val;
							break;
						case MKJSON_BOOL:
							varargs[idx++] = (void*)(intptr_t)arg->value.bool_val;
							break;
						default:
							idx--;
							break;
					}
				} else {
					idx--;
				}
				break;
		}
	}
	
	// Actually, this approach won't work well with varargs
	// Let's use a simpler approach that builds the JSON directly
	free(varargs);
	
	// Build JSON manually to avoid varargs complexity
	char *json = NULL;
	int len = 0;
	int capacity = 256;
	json = malloc(capacity);
	if (!json) {
		if (pretty) { free(indent); free(nested_indent); }
		return NULL;
	}
	
	// Start with opening bracket
	json[0] = otype == MKJSON_OBJ ? '{' : '[';
	len = 1;
	
	// Add newline after opening bracket if pretty printing
	if (pretty && count > 0) {
		if (len + 1 > capacity) {
			capacity *= 2;
			char *new_json = realloc(json, capacity);
			if (!new_json) { 
				free(json);
				if (pretty) { free(indent); free(nested_indent); }
				return NULL; 
			}
			json = new_json;
		}
		json[len++] = '\n';
	}
	
	// Process each argument
	int first = 1;
	for (int i = 0; i < count; i++) {
		const mkjson_arg *arg = &args[i];
		char *chunk = NULL;
		char *key_escaped = NULL;
		char *val_escaped = NULL;
		int chunk_len = 0;
		
		// Skip ignore types
		if (arg->type < 0) continue;
		
		// Add comma if not first
		if (!first) {
			int comma_space = pretty ? (1 + strlen(newline) + strlen(nested_indent)) : 2;
			while (len + comma_space > capacity) {
				capacity *= 2;
				char *new_json = realloc(json, capacity);
				if (!new_json) { 
					free(json);
					if (pretty) { free(indent); free(nested_indent); }
					return NULL; 
				}
				json = new_json;
			}
			json[len++] = ',';
			if (pretty) {
				strcpy(json + len, newline);
				len += strlen(newline);
				strcpy(json + len, nested_indent);
				len += strlen(nested_indent);
			} else {
				json[len++] = ' ';
			}
		} else if (pretty) {
			// First item - add indent
			int indent_space = strlen(nested_indent);
			while (len + indent_space > capacity) {
				capacity *= 2;
				char *new_json = realloc(json, capacity);
				if (!new_json) { 
					free(json);
					if (pretty) { free(indent); free(nested_indent); }
					return NULL; 
				}
				json = new_json;
			}
			strcpy(json + len, nested_indent);
			len += strlen(nested_indent);
		}
		first = 0;
		
		// For objects, add key
		if (otype == MKJSON_OBJ && arg->key) {
			key_escaped = json_escape_string(arg->key);
			if (!key_escaped) { 
				free(json); 
				if (pretty) { free(indent); free(nested_indent); }
				return NULL; 
			}
		}
		
		// Format value based on type
		switch (arg->type) {
			case MKJSON_STRING:
			case MKJSON_STRING_FREE:
				val_escaped = json_escape_string(arg->value.str_val);
				if (!val_escaped) { 
					free(key_escaped); 
					free(json); 
					if (pretty) { free(indent); free(nested_indent); }
					return NULL; 
				}
				if (otype == MKJSON_OBJ) {
					allsprintf(&chunk, "\"%s\":%s\"%s\"", key_escaped, pretty ? " " : "", val_escaped);
				} else {
					allsprintf(&chunk, "\"%s\"", val_escaped);
				}
				free(val_escaped);
				if (arg->type == MKJSON_STRING_FREE) {
					free(arg->value.str_free_val);
				}
				break;
				
			case MKJSON_JSON:
			case MKJSON_JSON_FREE:
				if (otype == MKJSON_OBJ) {
					allsprintf(&chunk, "\"%s\":%s%s", key_escaped, 
					          pretty ? " " : "", arg->value.str_val ? arg->value.str_val : "null");
				} else {
					allsprintf(&chunk, "%s", 
					          arg->value.str_val ? arg->value.str_val : "null");
				}
				if (arg->type == MKJSON_JSON_FREE) {
					free(arg->value.str_free_val);
				}
				break;
				
			case MKJSON_INT:
				if (otype == MKJSON_OBJ) {
					allsprintf(&chunk, "\"%s\":%s%d", key_escaped, pretty ? " " : "", arg->value.int_val);
				} else {
					allsprintf(&chunk, "%d", arg->value.int_val);
				}
				break;
				
			case MKJSON_LLINT:
				if (otype == MKJSON_OBJ) {
					allsprintf(&chunk, "\"%s\":%s%lld", key_escaped, pretty ? " " : "", arg->value.llint_val);
				} else {
					allsprintf(&chunk, "%lld", arg->value.llint_val);
				}
				break;
				
			case MKJSON_DOUBLE:
				if (otype == MKJSON_OBJ) {
					allsprintf(&chunk, "\"%s\":%s%f", key_escaped, pretty ? " " : "", arg->value.dbl_val);
				} else {
					allsprintf(&chunk, "%f", arg->value.dbl_val);
				}
				break;
				
			case MKJSON_LDOUBLE:
				if (otype == MKJSON_OBJ) {
					allsprintf(&chunk, "\"%s\":%s%Lf", key_escaped, pretty ? " " : "", arg->value.ldbl_val);
				} else {
					allsprintf(&chunk, "%Lf", arg->value.ldbl_val);
				}
				break;
				
			case MKJSON_SCI_DOUBLE:
				if (otype == MKJSON_OBJ) {
					allsprintf(&chunk, "\"%s\":%s%e", key_escaped, pretty ? " " : "", arg->value.dbl_val);
				} else {
					allsprintf(&chunk, "%e", arg->value.dbl_val);
				}
				break;
				
			case MKJSON_SCI_LDOUBLE:
				if (otype == MKJSON_OBJ) {
					allsprintf(&chunk, "\"%s\":%s%Le", key_escaped, pretty ? " " : "", arg->value.ldbl_val);
				} else {
					allsprintf(&chunk, "%Le", arg->value.ldbl_val);
				}
				break;
				
			case MKJSON_BOOL:
				if (otype == MKJSON_OBJ) {
					allsprintf(&chunk, "\"%s\":%s%s", key_escaped, pretty ? " " : "",
					          arg->value.bool_val ? "true" : "false");
				} else {
					allsprintf(&chunk, "%s", arg->value.bool_val ? "true" : "false");
				}
				break;
				
			case MKJSON_NULL:
				if (otype == MKJSON_OBJ) {
					allsprintf(&chunk, "\"%s\":%snull", key_escaped, pretty ? " " : "");
				} else {
					allsprintf(&chunk, "null");
				}
				break;
				
			default:
				break;
		}
		
		free(key_escaped);
		
		// Add chunk to json
		if (chunk) {
			chunk_len = strlen(chunk);
			while (len + chunk_len + 2 > capacity) {
				capacity *= 2;
				char *new_json = realloc(json, capacity);
				if (!new_json) { free(json); free(chunk); return NULL; }
				json = new_json;
			}
			strcpy(json + len, chunk);
			len += chunk_len;
			free(chunk);
		}
	}
	
	// Add closing bracket with proper indentation
	if (pretty && count > 0) {
		// Add newline and indent before closing bracket
		int closing_space = strlen(newline) + strlen(indent) + 1;
		while (len + closing_space > capacity) {
			capacity *= 2;
			char *new_json = realloc(json, capacity);
			if (!new_json) { 
				free(json);
				if (pretty) { free(indent); free(nested_indent); }
				return NULL; 
			}
			json = new_json;
		}
		strcpy(json + len, newline);
		len += strlen(newline);
		strcpy(json + len, indent);
		len += strlen(indent);
	}
	
	json[len++] = otype == MKJSON_OBJ ? '}' : ']';
	json[len] = '\0';
	
	// Free allocated indent strings
	if (pretty) {
		free(indent);
		free(nested_indent);
	}
	
	return json;
}


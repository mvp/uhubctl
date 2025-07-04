/* mkjson.h - a part of mkjson library
 *
 * Copyright (C) 2018 Jacek Wieczorek
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.	See the LICENSE file for details.
 */

#ifndef MKJSON_H
#define MKJSON_H

// JSON container types
enum mkjson_container_type
{
	MKJSON_ARR = 0, // An array
	MKJSON_OBJ = 1  // An object (hash or whatever you call it)
};

// JSON data types
enum mkjson_value_type
{
	MKJSON_STRING      = (int)('s'), // const char* - String data
	MKJSON_STRING_FREE = (int)('f'), // char* - String data, but pointer is freed
	MKJSON_JSON        = (int)('r'), // const char* - JSON data (like string, but no quotes)
	MKJSON_JSON_FREE   = (int)('j'), // char* - JSON data, but pointer is freed
	MKJSON_INT         = (int)('i'), // int - An integer
	MKJSON_LLINT       = (int)('I'), // long long int - A long integer
	MKJSON_DOUBLE      = (int)('d'), // double - A double
	MKJSON_LDOUBLE     = (int)('D'), // long double - A long double
	MKJSON_SCI_DOUBLE  = (int)('e'), // double - A double with scientific notation
	MKJSON_SCI_LDOUBLE = (int)('E'), // long double - A long double with scientific notation
	MKJSON_BOOL        = (int)('b'), // int - A boolean value
	MKJSON_NULL        = (int)('n'),  // -- - JSON null value

	// These cause one argument of certain type to be ignored
	MKJSON_IGN_STRING      = (-MKJSON_STRING),
	MKJSON_IGN_STRING_FREE = (-MKJSON_STRING_FREE),
	MKJSON_IGN_JSON        = (-MKJSON_JSON),
	MKJSON_IGN_JSON_FREE   = (-MKJSON_JSON_FREE),
	MKJSON_IGN_INT         = (-MKJSON_INT),
	MKJSON_IGN_LLINT       = (-MKJSON_LLINT),
	MKJSON_IGN_DOUBLE      = (-MKJSON_DOUBLE),
	MKJSON_IGN_LDOUBLE     = (-MKJSON_LDOUBLE),
	MKJSON_IGN_BOOL        = (-MKJSON_BOOL),
	MKJSON_IGN_NULL        = (-MKJSON_NULL)
};

extern char *mkjson( enum mkjson_container_type otype, int count, ... );

#endif

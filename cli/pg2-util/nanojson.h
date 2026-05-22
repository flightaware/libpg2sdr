/*
 *  nanojson.h - pg2-util json output helpers, declarations
 *
 *  Copyright (c) 2026 FlightAware All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *
 *  1. Redistributions of source code must retain the above copyright
 *  notice, this list of conditions and the following disclaimer.
 *
 *  2. Redistributions in binary form must reproduce the above copyright
 *  notice, this list of conditions and the following disclaimer in the
 *  documentation and/or other materials provided with the distribution.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PG2_NANOJSON_H
#define PG2_NANOJSON_H

#include <stdbool.h>
#include <stdio.h>

/* a tiny tiny json output-only implementation
 * (relies on the caller making calls in the right sequence to produce valid json)
 */

/* set where we're going to write to */
void json_set_output(FILE *out);

/* internal: output a comma if the next token must be a comma or close-brace */
void json_comma(void);

/* Start an array. This is a json value. Subsequent calls should emit json values or call json_end_array */
void json_start_array(void);
/* end an array previously started with json_start_array or json_kv_array */
void json_end_array(void);

/* Start an object. This is a json value. Subsequent calls should emit key/value pairs, or call json_end_object */
void json_start_object(void);
/* end an object previously started with json_start_object */
void json_end_object(void);

/* Emit the key part of a key/value pair within an enclosing object. The value should be emitted next. */
void json_key(const char *key);

/* Emit a json string value, where the value is the quoted/escaped version of 'str' */
void json_string(const char *str);
/* Emit a json string value, where the value is the quoted/escaped version of applying printf-like formatting to 'fmt' */
void json_string_fmt(const char *fmt, ...)
    __attribute__((format(printf,1,2)));

/* Emit an unsigned int as a json value  */
void json_number(unsigned i);

/* Emit a boolean as a json value  */
void json_bool(bool x);

/* Emit an arbitary json value, by applying printf-like formatting to 'fmt'. The result is _not_ quoted. */
void json_value_fmt(const char *fmt, ...)
    __attribute__((format(printf,1,2)));

#endif

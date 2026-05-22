/*
 *  nanojson.c - pg2-util json output helpers, implementation
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

#include "nanojson.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>

static FILE *json_file;
static bool json_needs_comma = false;

void json_set_output(FILE *out)
{
    json_file = out;
    json_needs_comma = false;
}

void json_comma(void)
{
    if (json_needs_comma) {
        fprintf(json_file,",");
        json_needs_comma = false;
    }
}

void json_start_array(void)
{
    json_comma();
    fprintf(json_file, "[");
    json_needs_comma = false;
}

void json_end_array(void)
{
    fprintf(json_file, "]");
    json_needs_comma = true;
}

void json_start_object(void)
{
    json_comma();
    fprintf(json_file, "{");
    json_needs_comma = false;
}

void json_end_object(void)
{
    fprintf(json_file, "}");
    json_needs_comma = true;
}

void json_key(const char *key)
{
    json_string(key);
    fprintf(json_file, ":");
    json_needs_comma = false;
}

void json_number(unsigned i)
{
    json_value_fmt("%u", i);
}

void json_bool(bool x)
{
    json_value_fmt("%s", x ? "true" : "false");
}

void json_string_fmt(const char *fmt, ...)
{
    static char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    json_string(buf);
}

void json_value_fmt(const char *fmt, ...)
{
    json_comma();
    va_list ap;
    va_start(ap, fmt);
    vfprintf(json_file, fmt, ap);
    va_end(ap);
    json_needs_comma = true;
}

void json_string(const char *str)
{
    json_comma();
    fprintf(json_file, "\"");
    for (; *str; ++str) {
        char ch = *str;
        switch (ch) {
        case '\\':
            fprintf(json_file, "\\\\");
            break;
        case '"':
            fprintf(json_file, "\\\"");
            break;
        case '\n':
            fprintf(json_file, "\\n");
            break;
        case '\r':
            fprintf(json_file, "\\r");
            break;
        case '\t':
            fprintf(json_file, "\\t");
            break;
        default:
            if (ch < 32 || ch > 127) {
                fprintf(json_file, "\\u%04x", ch);
            } else {
                fprintf(json_file, "%c", ch);
            }
            break;
        }
    }

    fprintf(json_file, "\"");
    json_needs_comma = true;
}

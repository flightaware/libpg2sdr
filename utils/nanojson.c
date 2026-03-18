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

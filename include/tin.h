#pragma once

#include <stdarg.h>
#include "console.h"

extern "C" void* memset(void* dst, int val, size_t n);
extern "C" void* memcpy(void* dst, const void* src, size_t n);
extern "C" size_t mstrlen(const char* s);
extern "C" int strcmp(const char* a, const char* b);
int digits_b10(int arg);
int count_bin_length(int arg, bool ignore_trailing = true);
char* hex_to_str(char* out, int arg);
char* bin_to_str(char* out, int arg, bool ignore_trailing = true);
char hex_digit_to_char(char digit);
// MAX_DIGITS = 11
char* int_to_str(char* out, int arg);
char* float_to_str(char* out, float arg, int precision);
int my_vsnprintf(char* buf, size_t count, const char* fmt, va_list args);
int my_snprintf(char* buf, size_t count, const char* fmt, ...);
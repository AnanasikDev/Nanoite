#pragma once

#include <windows.h>

void print_str(const char* s);
void prints(const char* s = nullptr);
#if USE_CRT
#include <stdio.h>
#endif

template <typename ...Args>
void __print(const char* s, Args... args)
{
#if USE_CRT
    printf(s, args...);
#else
    prints(s);
#endif
}

void printi(int arg);
void printf(float arg, int precision = 6);
void print_hex(int arg);
void print_bin(int arg, bool ignore_trailing = true);
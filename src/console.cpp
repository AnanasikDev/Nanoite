#include "console.h"
#include "tin.h"
#include "nem.hpp"

void print_str(const char* s)
{
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD written;
    int len = 0;
    while (s[len]) len++;
    WriteFile(out, s, len, &written, 0);
}

void prints(const char* s)
{
    if (s == nullptr)
    {
        print_str("\n");
    }
    else
    {
        print_str(s);
    }
}

void printi(int arg)
{
    char s[11];
    int_to_str(s, arg);
    prints(s);
}

void printf(float arg, int precision)
{
    constexpr int whole_digits = 9;
    constexpr int floating_digits = 6;
    if (nem::is_nan(arg)){
        prints("NaN");
        return;
    }
    if (nem::is_infinite(arg)){
        prints("INF");
        return;
    }
    char s[1 + whole_digits + 1 + floating_digits];
    float_to_str(s, arg, precision);
    prints(s);
}

void print_hex(int arg)
{
    char buf[9];
    hex_to_str(buf, arg);
    prints(buf);
}

void print_bin(int arg, bool ignore_trailing)
{
    char buf[33];
    bin_to_str(buf, arg, ignore_trailing);
    prints(buf);
}
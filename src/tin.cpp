#include "tin.h"
#include "console.h"

extern "C" void* memset(void* dst, int val, size_t n)
{
    unsigned char* d = (unsigned char*)dst;
    while (n--) *d++ = (unsigned char)val;
    return dst;
}

extern "C" void* memcpy(void* dst, const void* src, size_t n)
{
    unsigned char* d = (unsigned char*)dst;
    const unsigned char* s = (const unsigned char*)src;
    while (n--) *d++ = *s++;
    return dst;
}

extern "C" size_t strlen(const char* s) {
    const char* p = s;
    while (*p) ++p;
    return p - s;
}

extern "C" int strcmp(const char* a, const char* b) {
    while (*a && *a == *b) { ++a; ++b; }
    return *(unsigned char*)a - *(unsigned char*)b;
}

int digits_b10(int arg)
{
    if (arg < 0) arg = -arg;
    int len = 1;
    while (arg >= 10)
    {
        len += 1;
        arg /= 10;
    }
    return len;
}


int count_bin_length(int arg, bool ignore_trailing)
{
    if (!ignore_trailing)
    {
        return 32;
    }

    bool found_non_zero = false;
    int length = 0;
    for (int chk = 0; chk < 32; ++chk)
    {
        int digit = arg & 1;
        arg >>= 1;

        if (digit == 0)
        {
            ++length;
            continue;
        }

        length = 0;
    }
    if (length == 32)
    {
        length = 31; // 0 has to be represented with 1 digit
    }
    return 32 - length;
}

char hex_digit_to_char(char digit)
{
    if (digit < 10)
    {
        return '0' + digit;
    }
    else
    {
        return 'A' + digit - 10;
    }
}

char* hex_to_str(char* out, int arg)
{
    for (int chk = 0; chk < 32/4; ++chk)
    {
        int digit = arg & (0xF);
        *(out + 8 - chk - 1) = hex_digit_to_char(digit);
        arg >>= 4;
    }
    out[8] = 0;
    return out + 8;
}

char* bin_to_str(char* out, int arg, bool ignore_trailing)
{
    bool found_non_zero = false;
    int length = count_bin_length(arg, ignore_trailing);
    for (int chk = 32 - length; chk < 32; ++chk)
    {
        int digit = arg & 1;
        *(out + 32 - chk - 1) = hex_digit_to_char(digit);
        arg >>= 1;
    }
    out[length] = 0;
    return out + length;
}

char* int_to_str(char* out, int arg)
{
    bool neg = arg < 0;
    if (neg){
        arg = -arg;
    }
    int len = digits_b10(arg) + neg;
    out[len] = 0;
    int i = 0;
    if (neg){
        out[0] = '-';
        --len;
    }
    for (; i < len; ++i)
    {
        out[len - i - 1 + neg] = (arg % 10) + '0';
        arg /= 10;
    }
    return out + len + neg;
}

char* float_to_str(char* out, float arg, int precision)
{
    bool neg = arg < 0;
    if (neg){
        arg = -arg;
    }
    int whole = static_cast<int>(arg);
    int factor = 1;
    for (int i = 0; i < precision; i++){
        factor *= 10;
    }
    double floating_float = (arg - whole);
    int floating_int = static_cast<int>(floating_float * factor);
    int first_zeros = 0;
    {
        double scaled = floating_float * 10; // 0.1 should count as 0 zeros after .
        for (int i = 0; i < precision; ++i){
            double truncated = static_cast<int>(scaled) + 1e-7;
            if (static_cast<int>(truncated) >= 1){
                // found first non-zero
                break;
            }
            ++first_zeros;
            scaled *= 10;
        }
    }

    for (int i = 0; i < precision; i++){
        if (floating_int % 10 == 0){
            floating_int /= 10;
        }
    }

    if (neg){
        *out = '-';
        ++out;
    }
    int_to_str(out, whole);
    out += digits_b10(whole);
    *out = '.';
    out++;
    for (int z = 0; z < first_zeros; ++z){
        *out = '0';
        out++;
    }
    return int_to_str(out, floating_int);
    // 0.000290f, 6 digit precision
    //  .000290f * 10e6 = 290.0
}

int my_vsnprintf(char* buf, size_t count, const char* fmt, va_list args) {
    char* out = buf;
    char* end = buf + count - 1;

    while (*fmt && out < end) {
        if (*fmt != '%') { *out++ = *fmt++; continue; }
        fmt++; // skip %

        // parse width(padding?)/precision flags minimally
        int width = 0, prec = 6;
        bool left = false;
        if (*fmt == '-') { left = true; fmt++; }
        while (*fmt >= '0' && *fmt <= '9') width = width*10 + (*fmt++ - '0');
        if (*fmt == '.') { fmt++; prec = 0; while (*fmt >= '0' && *fmt <= '9') prec = prec*10 + (*fmt++ - '0'); }

        switch (*fmt++) {
        case 's':
        {
            const char* s = va_arg(args, const char*);
            while (*s && out < end)
                *out++ = *s++;
            break;
        }
        case 'd':
        {
            int v = va_arg(args, int);
            out = int_to_str(out, v);
            break;
        }
        case 'u':
        {
            unsigned v = va_arg(args, unsigned);
            out = int_to_str(out, v);
            break;
        }
        case 'f':
        {
            double v = va_arg(args, double);
            out = float_to_str(out, v, prec);
            break;
        }
        case '%':
        {
            *out++ = '%';
            break;
        }
        }
    }
    *out = '\0';
    return (int)(out - buf);
}

int my_snprintf(char* buf, size_t count, const char* fmt, ...)
{
    va_list args;        
    va_start(args, fmt);    
    int result = my_vsnprintf( buf, count, fmt, args);
    va_end(args);
    return result;
}

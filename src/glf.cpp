#include "glf.h"

// Define the function pointers
#define X(ret, name, args) name##_t name = nullptr;
GL_FUNCS
#undef X

// Load them all in one call
void gl_load_functions()
{
#define X(ret, name, args) name = (name##_t)wglGetProcAddress(#name);
    GL_FUNCS
#undef X
}
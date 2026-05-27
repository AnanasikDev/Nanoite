#pragma once
#include <windows.h>
#include <gl/gl.h>

// GL types that gl.h doesn't define
typedef ptrdiff_t GLsizeiptr;
typedef ptrdiff_t GLintptr;
typedef char GLchar;

// GL constants you need (add more as you use them)
#define GL_ARRAY_BUFFER         0x8892
#define GL_STATIC_DRAW          0x88E4
#define GL_DYNAMIC_DRAW         0x88E8
#define GL_FRAGMENT_SHADER      0x8B30
#define GL_VERTEX_SHADER        0x8B31
#define GL_COMPILE_STATUS       0x8B81
#define GL_LINK_STATUS          0x8B82

// Declare function pointer types and globals for each function you use
#define GL_FUNCS \
    X(void,    glGenBuffers,          (GLsizei n, GLuint* buffers)) \
    X(void,    glBindBuffer,          (GLenum target, GLuint buffer)) \
    X(void,    glBufferData,          (GLenum target, GLsizeiptr size, const void* data, GLenum usage)) \
    X(GLuint,  glCreateShader,        (GLenum type)) \
    X(void,    glShaderSource,        (GLuint shader, GLsizei count, const GLchar** string, const GLint* length)) \
    X(void,    glCompileShader,       (GLuint shader)) \
    X(GLuint,  glCreateProgram,       (void)) \
    X(void,    glAttachShader,        (GLuint program, GLuint shader)) \
    X(void,    glLinkProgram,         (GLuint program)) \
    X(void,    glUseProgram,          (GLuint program)) \
    X(void,    glGenVertexArrays,     (GLsizei n, GLuint* arrays)) \
    X(void,    glBindVertexArray,     (GLuint array)) \
    X(void,    glEnableVertexAttribArray, (GLuint index)) \
    X(void,    glVertexAttribPointer, (GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer)) \
    X(GLint,   glGetUniformLocation,  (GLuint program, const GLchar* name)) \
    X(void,    glUniformMatrix4fv,    (GLint location, GLsizei count, GLboolean transpose, const float* value)) \
    X(void,    glUniform1f,           (GLint location, float v0)) \
    X(void,    glUniform3f,           (GLint location, float v0, float v1, float v2)) \

// Declare the function pointers
#define X(ret, name, args) typedef ret (__stdcall *name##_t) args; extern name##_t name;
GL_FUNCS
#undef X

void gl_load_functions();
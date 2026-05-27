#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <gl/gl.h>
#include "glf.h"
#include <mmeapi.h>
#include <mmsystem.h>

extern "C" int _fltused = 0;

// Provide memset/memcpy since the compiler may emit calls to them
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

void prints(const char* s)
{
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD written;
    int len = 0;
    while (s[len]) len++;
    WriteFile(out, s, len, &written, 0);
}

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

inline int digits_b10(int arg)
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

// MAX_DIGITS = 11
void int_to_str(char* out, int arg){
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
}

void printi(int arg)
{
    char s[11];
    int_to_str(s, arg);
    prints(s);
}

void float_to_str(char* out, float arg, int precision)
{
    bool neg = arg < 0;
    if (neg){
        arg = -arg;
    }
    int whole = static_cast<int>(arg);
    int digits = 1;
    for (int i = 0; i < precision; i++){
        digits *= 10;
    }
    int floating = static_cast<int>((arg - whole) * digits);
    if (neg){
        *out = '-';
        ++out;
    }
    int_to_str(out, whole);
    out += digits_b10(whole);
    int_to_str(out, floating);
    out += digits;
}

template <int WholeDigits = 9, int FloatingDigits = 5>
void printf(float arg, int precision = 2)
{
    char s[1 + WholeDigits + 1 + FloatingDigits];
    float_to_str(s, arg, precision);
}

float my_sqrtf(float x)
{
    return __builtin_sqrtf(x);// __builtin_sqrtf(x);
}

LRESULT CALLBACK WinEventLoop(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam){
    switch (msg){
        case WM_CLOSE:
            // if (MessageBox(hwnd, "Really quit?", "My application", MB_OKCANCEL) == IDOK)
            {
                DestroyWindow(hwnd);
            }
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

class Arena
{
    void* start;
};

template <typename T>
T* memreq(int size)
{
    return (T*)VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}

struct File
{
    HANDLE handle;

    constexpr bool is_valid()
    {
        return handle != nullptr;
    }
};

class FileSystem
{   
public:
    File open(const char* const file_name, const DWORD access = GENERIC_WRITE | GENERIC_READ, const DWORD open = OPEN_ALWAYS)
    {
        HANDLE handle = CreateFile(file_name,                // name of the write
                           access,          // open for writing/reading/executing
                           0,                      // do not share
                           NULL,                   // default security
                           open,             // create/open
                           FILE_ATTRIBUTE_NORMAL,  // normal file
                           NULL);
        
        if (handle == nullptr)
        {
            prints("Could not open file, returned handle is nullptr\n");
        }

        return File { handle };
    }

    /// writes the data to the file, returns true if succeded
    bool write(const File file, char* data, DWORD bytes_to_write)
    {
        DWORD bytes_written;
        bool success = WriteFile( 
                    file.handle,           // open file handle
                    data,      // start of data to write
                    bytes_to_write,  // number of bytes to write
                    &bytes_written, // number of bytes that were written
                    NULL);

        if (!success)
        {
            prints("Could not write to file\n");
        }

        return success;
    }

    bool close(File& file)
    {
        bool success = CloseHandle(file.handle);
        file.handle = nullptr;
        return success;
    }
};


#if USE_CRT
int main()
#else
extern "C" void __stdcall _main()
#endif
{
    char DataBuffer[45] = "New world";
    FileSystem fs;

    File file = fs.open("whatevever.txt");
    fs.write(file, DataBuffer, sizeof(DataBuffer));
    fs.close(file);

    volatile float m = 0.2f;
    volatile float sss = my_sqrtf(m);

    printi(-10);
    prints("\n");
    printi(10);
    prints("\n");
    printf(-0.8f);
    prints("\n");
    printf(0.8f);
    prints("\n");
    printf(-1.1f);
    prints("\n");
    printf(1.1f);
    prints("\n");

    int* a = memreq<int>(2 * sizeof(int));
    a[0] = 2;
    a[1] = 4;
    a[1023] = 8;
    // a[1024] = 8; will give error since more than allocated page
    // a[4096] = 9;

    volatile float x = a[1] + a[0];
    volatile float y = (x);
    (void)y;
    //printf("HEllo from CRT\n");
    //print("Hello!\n");
    //print(12344);
    __print("Hello? %.2f\n", sss);
    __print("Digits in 123456 %d", digits_b10(123456));
    prints("\n\n");
    printi(123);
    prints("\n\n");

    // Create window
    WNDCLASSA wc = {};
    wc.lpfnWndProc = WinEventLoop;
    wc.hInstance = GetModuleHandleA(0);
    wc.lpszClassName = "g";
    RegisterClassA(&wc);

    HWND hwnd = CreateWindowExA(
        0, "g", "game", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 1280, 720,
        0, 0, wc.hInstance, 0
    );

    // Set up OpenGL
    HDC dc = GetDC(hwnd);
    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    SetPixelFormat(dc, ChoosePixelFormat(dc, &pfd), &pfd);
    wglMakeCurrent(dc, wglCreateContext(dc));

    gl_load_functions();

    float vertices[] = {
        -sss, -0.5f, 0.0f,
         0.5f, -0.5f, 0.0f,
         0.0f,  0.5f, 0.0f
    };
    unsigned int VBO;
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

    const char* vertexShaderSource = "#version 330 core\n"
        "layout (location = 0) in vec3 aPos;\n"
        "void main()\n"
        "{\n"
        "   gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);\n"
        "}\0";
    unsigned int vertexShader;
    vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);

    const char* fragmentShaderSource = "#version 330 core\n"
        "out vec4 FragColor;\n"
        "void main()\n"
        "{\n"
        "   FragColor = vec4(1.0f, 0.5f, 0.2f, 1.0f);\n"
        "}\n";
    unsigned int fragmentShader;
    fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);

    unsigned int shaderProgram;
    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    glUseProgram(shaderProgram);
    //glDeleteShader(vertexShader);
    //glDeleteShader(fragmentShader);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    unsigned int VAO;
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    int ckey;           // storage for the current keyboard key being pressed
    int notestate = 0;  // keeping track of when the note is on or off
    int velocity = 100; // MIDI note velocity parameter value
    int midiport = 0;   // select which MIDI output port to open
    int flag;           // monitor the status of returning functions
    HMIDIOUT device;    // MIDI device interface for sending MIDI output

    // variable which is both an integer and an array of characters:
    union { unsigned long word; unsigned char data[4]; } message;
    // message.data[0] = command byte of the MIDI message, for example: 0x90
    // message.data[1] = first data byte of the MIDI message, for example: 60
    // message.data[2] = second data byte of the MIDI message, for example 100
    // message.data[3] = not used for any MIDI messages, so set to 0
    message.data[0] = 0x90;  // MIDI note-on message (requires to data bytes)
    message.data[1] = 60;    // MIDI note-on message: Key number (60 = middle C)
    message.data[2] = 100;   // MIDI note-on message: Key velocity (100 = loud)
    message.data[3] = 0;     // Unused parameter

    __print("MIDI output port set to %d.\n", midiport);

    HMIDIOUT midihandle;
    MMRESULT midirsult = midiOutOpen(&midihandle, midiport, 0, 0, CALLBACK_NULL);
    if (midirsult == 0)
    {
        __print("midi initialized\n");
    }
    else
    {
        __print("midi failed\n");
    }

    midiOutShortMsg(midihandle, message.word); //0x209035C0

    // Main loop
    MSG msg;
    bool running = true;
    float time = 0;
    while (running)
    {
        time += 0.01f;
        while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT){
                running = false;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }

        glClearColor(0.2f, 0.1f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glBindVertexArray(VAO);
        vertices[0] = my_sqrtf(time);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

        glUseProgram(shaderProgram);
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        // printf(time);
        // prints("\n");
        
        if (time > 2.0f){
            midiOutShortMsg(midihandle, message.word); //0x209035C0
            time = 0;
        }

        SwapBuffers(dc);
    }

    midiOutReset(midihandle);
    midiOutClose(midihandle);

    ExitProcess(0);
}
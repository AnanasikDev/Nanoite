#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <gl/gl.h>
#include "glf.h"
#include <mmeapi.h>
#include <mmsystem.h>
#include "nem.hpp"
#include "trig.hpp"

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

void print_str(const char* s)
{
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD written;
    int len = 0;
    while (s[len]) len++;
    WriteFile(out, s, len, &written, 0);
}

void prints(const char* s = nullptr)
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
    int_to_str(out, floating_int);
    // 0.000290f, 6 digit precision
    //  .000290f * 10e6 = 290.0 
}

template <int WholeDigits = 9, int FloatingDigits = 6>
void printf(float arg, int precision = 6)
{
    if (nem::_nem_isnan(arg)){
        prints("NaN");
        return;
    }
    if (!nem::_nem_isfinite(arg)){
        prints("INF");
        return;
    }
    char s[1 + WholeDigits + 1 + FloatingDigits];
    float_to_str(s, arg, precision);
    prints(s);
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

void hex_to_str(char* out, int arg)
{
    for (int chk = 0; chk < 32/4; ++chk)
    {
        int digit = arg & (0xF);
        *(out + 8 - chk - 1) = hex_digit_to_char(digit);
        arg >>= 4;
    }
    out[8] = 0;
}

void print_hex(int arg)
{
    char buf[9];
    hex_to_str(buf, arg);
    prints(buf);
}

int count_bin_length(int arg, bool ignore_trailing = true)
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

void bin_to_str(char* out, int arg, bool ignore_trailing = true)
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
}

void print_bin(int arg, bool ignore_trailing = true)
{
    char buf[33];
    bin_to_str(buf, arg, ignore_trailing);
    prints(buf);
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

void* alloc(int size)
{
    return VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}

bool dealloc(void* ptr, int size)
{
    return VirtualFree(ptr, 0, MEM_RELEASE); // 0 because of MEM_RELEASE
}

struct Header
{
    Header* next = nullptr;
    void* ptr = nullptr; // object to destroy
    void (*func)(void*) = nullptr; // destructor function delegate
};

#ifndef __PLACEMENT_NEW_INLINE
#define __PLACEMENT_NEW_INLINE
void* operator new(size_t, void* ptr) noexcept   { return ptr; }
void  operator delete(void*, void*) noexcept    { }
#endif

template<typename T>
void destroy(void* object)
{
    reinterpret_cast<T*>(object)->~T();
}

struct Arena
{
    Header* head = nullptr;
    char* start = nullptr;
    int size = 0;

    void init(char* _start, int _size)
    {
        start = _start;
        size = _size;
        free = _start;
    }

    constexpr bool is_empty() const
    {
        return size == 0;
    }

    char* reserve(int amount)
    {   
        char* new_free = free + amount; // new pointer to the first free byte
        if (new_free >= start + size) // >=?
        {
            return nullptr;
        }
        char* result = free;
        free = new_free;
        return result;
    }

    template <typename T>
    T* get()
    {
        const int size = sizeof(Header) + sizeof(T);
        char* ptr = (char*)reserve(size);
        T* obj_ptr = (T*)(ptr + sizeof(Header));
        Header* header_ptr = (Header*)ptr;
        header_ptr->ptr = obj_ptr;
        header_ptr->func = destroy<T>;
        
        new (obj_ptr) T();
        if (head == nullptr)
        {
            head = header_ptr;
        }
        else
        {
            header_ptr->next = head;
            head = header_ptr;
        }
        return obj_ptr;
    }

    ~Arena()
    {
        while (head != nullptr)
        {
            head->func(head->ptr);
            head = head->next;
        }
        start = nullptr;
        size = 0;
        head = nullptr;
        free = start;
    }

private:
    char* free = start; // pointer to the first free byte
};

void debug_print_region(void* ptr, const char* label)
{
    MEMORY_BASIC_INFORMATION mbi;
    VirtualQuery(ptr, &mbi, sizeof(mbi));

    prints(label);
    prints("  base: ");
    print_hex((size_t)mbi.BaseAddress);
    prints("  size: ");
    printi((int)mbi.RegionSize);
    prints("  state: ");
    if (mbi.State == MEM_COMMIT)       prints("COMMITTED");
    else if (mbi.State == MEM_RESERVE) prints("RESERVED");
    else if (mbi.State == MEM_FREE)    prints("FREE");
    prints("\n");
}

class Memory
{
public:

    static constexpr int INIT_SIZE = 100000;
    static constexpr int INIT_ARENAS_COUNT = 10;

    int capacity = 0;
    int arenas_count = 0;
    
    Arena pool;
    Arena arenas[INIT_ARENAS_COUNT];
    
public:
    static Memory* instance;

    void init()
    {
        instance = this;
        pool.init((char*)alloc(INIT_SIZE), INIT_SIZE);

        capacity = INIT_SIZE;
        arenas_count = INIT_ARENAS_COUNT;
    }

    void clean()
    {
        if (pool.start != nullptr)
        {
            for (int i = 0; i < arenas_count; ++i)
            {
                arenas[i].~Arena();
            }

            dealloc(pool.start, pool.size);
        }
    }
    
    Arena* get_arena(int size)
    {
        char* start = pool.start;
        char* cursor = start;
        Arena* result = nullptr;
        for (int i = 0; i < arenas_count; ++i)
        {
            Arena& found = arenas[i];
            
            if ((cursor - start) + size >= capacity)
            {
                return nullptr;
            }

            if (!found.is_empty())
            {
                cursor = found.start + found.size;
                continue;
            }

            if (i == arenas_count - 1)
            {
                found.init(cursor, size);
                return &found;
            }

            Arena& next = arenas[i + 1];
            if (next.is_empty() || cursor + size <= next.start)
            {
                found.init(cursor, size);
                return &found;
            }
        }
        return nullptr;
    }

    void free_arena(Arena* arena)
    {
        arena->~Arena();
    }
};

Memory* Memory::instance = nullptr;

struct Enemy{
    Enemy(){
        prints("Enemy created\n");
    }

    ~Enemy(){
        prints("Enemy deleted\n");
    }
};

static Memory* mem()
{
    return Memory::instance;
}

class Level{
    Arena* m_arena = nullptr;
    Enemy* m_enemy = nullptr;

public:

    void Init(){
        m_arena = mem()->get_arena(4000);
        m_enemy = m_arena->get<Enemy>();
    }

    void Cleanup(){
        if (m_arena != nullptr){
            mem()->free_arena(m_arena);
        }
    }
};

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

struct Timer
{
    Timer()
    {
        QueryPerformanceFrequency(&Frequency); 
    }

    void reset()
    {
        StartingTime.QuadPart = 0;
        EndingTime.QuadPart = 0;
        QueryPerformanceCounter(&StartingTime);
    }
    
    double elapsed_seconds()
    {
        QueryPerformanceCounter(&EndingTime);
        LONGLONG delta = EndingTime.QuadPart - StartingTime.QuadPart;
        return double(delta) / Frequency.QuadPart;
    }

private:
    LARGE_INTEGER StartingTime;
    LARGE_INTEGER EndingTime;
    LARGE_INTEGER Frequency;
};

struct Clock
{
    Clock()
    {
        timer.reset();
    }

    void init(float target_fps)
    {
        target_delta_time = 1.0f / target_fps;
    }

    double tick()
    {
        timer.reset();
        double delta_time = timer.elapsed_seconds();
        float to_go = target_delta_time - delta_time;
        while (to_go > 0.0)
        {
            if (to_go > 0.001)
            {
                Sleep(1);
            }
            delta_time = timer.elapsed_seconds();
            to_go = target_delta_time - delta_time;
        }
        return delta_time;
    }

private:
    float target_delta_time = 0.015f;
    Timer timer;
};

struct Input
{
    enum class KeyState : unsigned char
    {
        Idle = 1,
        Press = 1 << 1,
        Hold = 1 << 2,
        Release = 1 << 3,
        Down = Press | Hold,
        Up = Release | Idle
    };

    enum class Key : short
    {
        NONE = 0,

        LeftMouseButton     = VK_LBUTTON,
        RightMouseButton    = VK_RBUTTON,
        MiddleMouseButton   = VK_MBUTTON,
        
        Shift = VK_SHIFT,
        Control = VK_CONTROL,
        
        Tab   = VK_TAB,
        Return = VK_RETURN,
        Escape = VK_ESCAPE,
        Space = VK_SPACE,
        End = VK_END,
        Home = VK_HOME,

        ArrowLeft = VK_LEFT,
        ArrowUp = VK_UP,
        ArrowRight = VK_RIGHT,
        ArrowDown = VK_DOWN
    };
    
    static inline constexpr int MAX = VK_F24;
    
    KeyState states[MAX];
    
    inline bool is_key_down(Key key) const
    {
        return static_cast<int>(states[static_cast<int>(key)]) & static_cast<int>(KeyState::Down);
    }

    inline bool is_key_up(Key key) const
    {
        return static_cast<int>(states[static_cast<int>(key)]) & static_cast<int>(KeyState::Up);
    }

    inline bool is_key_pressed(Key key) const
    {
        return static_cast<int>(states[static_cast<int>(key)]) & static_cast<int>(KeyState::Press);
    }

    inline bool is_key_released(Key key) const
    {
        return static_cast<int>(states[static_cast<int>(key)]) & static_cast<int>(KeyState::Release);
    }

    inline bool is_key_held(Key key) const
    {
        return static_cast<int>(states[static_cast<int>(key)]) & static_cast<int>(KeyState::Hold);
    }

    inline bool is_key_idle(Key key) const
    {
        return static_cast<int>(states[static_cast<int>(key)]) & static_cast<int>(KeyState::Idle);
    }

    Input(){
        for (int k = 0; k < MAX; ++k)
        {
            states[k] = KeyState::Idle;
        }
    }

    void tick()
    {
        for (int k = 1; k < MAX; ++k)
        {
            short raw = GetKeyState(k) + 128;
            bool is_down = !(raw & 128);
            Key key = static_cast<Key>(k);
            if (is_key_down(key) && is_down)
            {
                states[k] = KeyState::Hold;
            }
            else if (is_key_down(key) && !is_down)
            {
                states[k] = KeyState::Release;
            }
            else if (is_key_up(key) && is_down)
            {
                states[k] = KeyState::Press;
            }
            else if (is_key_up(key) && !is_down)
            {
                states[k] = KeyState::Idle;
            }
        }
    }
};

constexpr nem::float2 WINDOW_SIZE { 1280, 720 };
constexpr nem::float2 WINDOW_RATIO { 1.0f, WINDOW_SIZE.x / WINDOW_SIZE.y };

#if USE_CRT
int main()
#else
extern "C" void __stdcall _main()
#endif
{
    Memory memory;
    memory.init();
    Level level;
    level.Init();

    Input input;

    debug_print_region(memory.pool.start, "pool");
    level.Cleanup();

    prints("My float: ");
    prints(" 1e-2:  "); printf(1e-2f); prints();
    prints(" 1e-3:  "); printf(1e-3f); prints();
    prints(" 1e-4:  "); printf(1e-4f); prints();
    prints(" 1e-5:  "); printf(1e-5f); prints();
    prints(" 1e-6:  "); printf(1e-6f); prints();


    char DataBuffer[45] = "New world";
    FileSystem fs;

    File file = fs.open("whatevever.txt");
    fs.write(file, DataBuffer, sizeof(DataBuffer));
    fs.close(file);

    // Create window
    WNDCLASSA wc = {};
    wc.lpfnWndProc = WinEventLoop;
    wc.hInstance = GetModuleHandleA(0);
    wc.lpszClassName = "g";
    RegisterClassA(&wc);

    HWND hwnd = CreateWindowExA(
        0, "g", "game", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, (int)WINDOW_SIZE.x, (int)WINDOW_SIZE.y,
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
        -0.4f, -0.5f, 0.0f,
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

   // __print("MIDI output port set to %d.\n", midiport);

    HMIDIOUT midihandle;
    MMRESULT midirsult = midiOutOpen(&midihandle, midiport, 0, 0, CALLBACK_NULL);
    if (midirsult == 0)
    {
        //__print("midi initialized\n");
    }
    else
    {
        //__print("midi failed\n");
    }

    midiOutShortMsg(midihandle, message.word); //0x209035C0

    Clock clock;
    clock.init(60);

    // Main loop
    MSG msg;
    bool running = true;
    float time = 0;
    float delta_time = 0.f;
    float angle = 0.f;
    while (running)
    {
        delta_time = clock.tick();
        input.tick();

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
        float speed = 1.0f * delta_time;
        float scale = 0.4f;
        angle += speed;
        vertices[0]     = scale * nem::sin<float>(angle) * WINDOW_RATIO.x;
        vertices[1]     = scale * nem::cos<float>(angle) * WINDOW_RATIO.y;
        vertices[0 + 3] = scale * nem::sin<float>(angle + nem::PI<float> * 2.f/3.f)  * WINDOW_RATIO.x;
        vertices[1 + 3] = scale * nem::cos<float>(angle + nem::PI<float> * 2.f/3.f)  * WINDOW_RATIO.y;
        vertices[0 + 6] = scale * nem::sin<float>(angle + nem::PI<float> * 4.f/3.f)  * WINDOW_RATIO.x;
        vertices[1 + 6] = scale * nem::cos<float>(angle + nem::PI<float> * 4.f/3.f)  * WINDOW_RATIO.y;

        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

        glUseProgram(shaderProgram);
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        if (input.is_key_pressed(Input::Key::LeftMouseButton)){
            prints("Press!\n");
        }
        
        if (input.is_key_released(Input::Key::LeftMouseButton)){
            prints("Release!\n");
        }

        if (input.is_key_pressed(Input::Key::Space)){
            prints("Jump!\n");
        }

        if (time > nem::TWO_PI<float>){
            midiOutShortMsg(midihandle, message.word); //0x209035C0
            time = 0;
        }

        SwapBuffers(dc);

        //prints("delta time: ");
        //printf(delta_time);
        //prints();
        time += delta_time;
    }

    memory.clean();

    midiOutReset(midihandle);
    midiOutClose(midihandle);

    ExitProcess(0);
}
#include "tin.h"
#include "console.h"
#include "imgui.h"
#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_win32.h"
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

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

extern "C" int _fltused = 0;

static constexpr char PATTERN_CLEAN = 0xCD;
static constexpr char PATTERN_DELETE = 0x1C;
static constexpr int PAGE_SIZE = 4096;

static constexpr int ceil_to_page_size(int value)
{
    const int floored = PAGE_SIZE * (value / PAGE_SIZE);
    return floored + (floored < value) * PAGE_SIZE;
}

// Provide memset/memcpy since the compiler may emit calls to them
// extern "C" void* memset(void* dst, int val, size_t n)
// {
//     unsigned char* d = (unsigned char*)dst;
//     while (n--) *d++ = (unsigned char)val;
//     return dst;
// }

// extern "C" void* memcpy(void* dst, const void* src, size_t n)
// {
//     unsigned char* d = (unsigned char*)dst;
//     const unsigned char* s = (const unsigned char*)src;
//     while (n--) *d++ = *s++;
//     return dst;
// }


void* alloc(int size)
{
    size = ceil_to_page_size(size);
    void* ptr = VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    memset(ptr, PATTERN_CLEAN, size);
    return ptr;
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
    if (object == nullptr)
    {
        return;
    }

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
        header_ptr->next = nullptr;
        
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

    char* begin() const { return start; }
    char* end() const { return start + size; }
    

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

template <typename T>
struct weak_ptr
{
    T* ptr = nullptr;
};

class Memory
{
public:

    static constexpr int INIT_SIZE = 4000;
    static constexpr int INIT_ARENAS_COUNT = 16;

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

    void extend_arena(Arena*& arena, int new_size)
    {
        if (new_size <= arena->size)
        {
            return;
        }

        Arena* new_arena = get_arena(new_size);
        memcpy(new_arena->start, arena->start, arena->size);
        memset(arena->start, PATTERN_CLEAN, arena->size);
        arena = new_arena;
    }

    void free_arena(Arena*& arena)
    {
        arena->~Arena();
        arena = nullptr;
    }
};

Memory* Memory::instance = nullptr;

LRESULT CALLBACK WinEventLoop(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

class Window
{
public:
    nem::int2 current_size;
    nem::float2 aspect_ratio {1.0f, 1.0f};

    static Window* instance;

    Window()
    {
        instance = this;

        // Create window
        WNDCLASSA wc = {};
        wc.lpfnWndProc = WinEventLoop;
        wc.hInstance = GetModuleHandleA(0);
        wc.lpszClassName = "g";
        RegisterClassA(&wc);

        handle = CreateWindowExA(
            0, "g", "game", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
            CW_USEDEFAULT, CW_USEDEFAULT, (int)INITIAL_SIZE.x, (int)INITIAL_SIZE.y,
            0, 0, wc.hInstance, 0
        );

        // Set up OpenGL
        dc = GetDC(handle);
        PIXELFORMATDESCRIPTOR pfd = {};
        pfd.nSize = sizeof(pfd);
        pfd.nVersion = 1;
        pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pfd.cColorBits = 32;
        pfd.cDepthBits = 24;
        SetPixelFormat(dc, ChoosePixelFormat(dc, &pfd), &pfd);
        wglMakeCurrent(dc, wglCreateContext(dc));

        current_size = INITIAL_SIZE;
        aspect_ratio = { 1.0f, static_cast<float>(current_size.x) / static_cast<float>(current_size.y) };
    }

    void begin_frame()
    {
         while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT){
                running = false;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
    }

    void end_frame()
    {
        SwapBuffers(dc);
    }

    inline bool is_running() const
    {
        return running;
    }

private:
    inline static constexpr nem::int2 INITIAL_SIZE = { 1280, 720 };
    HWND handle;
    MSG msg;
    HDC dc;
    bool running = true;
    bool focused = true;

public:
    LRESULT CALLBACK poll_events(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch (msg){
            case WM_CLOSE:
                //if (MessageBox(hwnd, "Really quit?", "My application", MB_OKCANCEL) == IDOK)
                {
                    DestroyWindow(hwnd);
                }
                return 0;
            case WM_DESTROY:
                PostQuitMessage(0);
                return 0;
            case WM_SETFOCUS:
                focused = true;
                break;
            case WM_KILLFOCUS:
                focused = false;
                break;

    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}
};

Window* Window::instance = nullptr;

LRESULT CALLBACK WinEventLoop(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam){
    return Window::instance->poll_events(hwnd, msg, wParam, lParam);
}

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
        m_arena = mem()->get_arena(1000);
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

struct Music
{
    Music(){
        MMRESULT midirsult = midiOutOpen(&midihandle, 0, 0, 0, CALLBACK_NULL);
        if (midirsult == 0)
        {
            prints("midi initialized\n");
        }
        else
        {
            prints("midi failed\n");
        }
        timer.reset();
    }

    enum class Note
    {
        A0 = 21,
        ASharp0, B0, C1, CSharp1, D1, DSharp1, E1, F1, FSharp1, G1, GSharp1, A1,
        ASharp1, B1, C2, CSharp2, D2, DSharp2, E2, F2, FSharp2, G2, GSharp2, A2,
        ASharp2, B2, C3, CSharp3, D3, DSharp3, E3, F3, FSharp3, G3, GSharp3, A3,
        ASharp3, B3, C4, CSharp4, D4, DSharp4, E4, F4, FSharp4, G4, GSharp4, A4,
        ASharp4, B4, C5, CSharp5, D5, DSharp5, E5, F5, FSharp5, G5, GSharp5, A5,
        ASharp5, B5, C6, CSharp6, D6, DSharp6, E6, F6, FSharp6, G6, GSharp6, A6,
        ASharp6, B6, C7, CSharp7, D7, DSharp7, E7, F7, FSharp7, G7, GSharp7, A7,
        ASharp7, B7, C8, CSharp8, D8, DSharp8, E8, F8, FSharp8, G8, GSharp8, A8,
        ASharp8, B8, C9, CSharp9, D9, DSharp9, E9, F9, FSharp9, G9, GSharp9
    };

    void play(Note note, int velocity = 100){
        union { unsigned long word; unsigned char data[4]; } message;
        message.data[0] = 0x90;
        message.data[1] = static_cast<int>(note);
        message.data[2] = velocity;
        message.data[3] = 0;     // Unused parameter
        impl_play_short(message.word);
    }

    void repeat_sequence(Note* notes, int num, float interval_seconds)
    {
        if (timer.elapsed_seconds() < interval_seconds)
        {
            return;
        }
        
        timer.reset();

        play(notes[seq_index]);
        ++seq_index;

        if (seq_index == num - 1){
            seq_index = 0;
        }
    }

    void clean()
    {
        midiOutReset(midihandle);
        midiOutClose(midihandle);
    }

private:
    HMIDIOUT midihandle;
    Timer timer;
    int seq_index = 0;

    void impl_play_short(int message)
    {
        midiOutShortMsg(midihandle, message);
    }
};

template <typename T>
struct list
{
    static constexpr int CAPACITY_BYTES = 256;
    static constexpr int ITEM_SIZE = sizeof(T);

    Arena* arena = nullptr;
    list(int new_capacity = CAPACITY_BYTES)
    {
        arena = mem()->get_arena(new_capacity);
    }

    T* push_back(T item)
    {
        if (arena == nullptr)
        {
            return nullptr;
        }
        // TODO: add arena extending on overflow
        T* ptr = end();
        new (ptr) T(item);
        ++length;
        return ptr;
    }

    void remove_at(int index)
    {
        T* item = get(index);
        if (item == nullptr)
        {
            return; // item is invalid
        }
        item->~T();
        item = nullptr;
        for (int i = index; i < length - 1; ++i)
        {
            (*this)[i] = (*this)[i + 1];
        }
        --length;
        memset(end(), PATTERN_DELETE, ITEM_SIZE);
        //memset(begin(), PATTERN_DELETE, get_size_bytes());
    }

    T* begin() const
    {
        return (T*)arena->begin();
    }

    T* end() const
    {
        return (T*)(arena->begin() + ITEM_SIZE * length);
    }

    T* get(int index) const
    {
        if (index < 0 || index >= length)
        {
            return nullptr;
        }
        return (T*)(arena->begin() + ITEM_SIZE * index);
    }

    T& operator[](int index)
    {
        return *get(index);
    }

    ~list()
    {
        mem()->free_arena(arena);
    }

    constexpr int get_length() const
    {
        return length;
    }
    
    constexpr int get_size_bytes() const
    {
        return length * ITEM_SIZE;
    }

private:
    int length = 0;
};

template <typename _T, size_t _LENGTH>
struct array
{
    using ITEM_TYPE = _T;
    static constexpr size_t ITEM_SIZE = sizeof(ITEM_TYPE);
    static constexpr size_t LENGTH = _LENGTH;
    static constexpr size_t SIZE = ITEM_SIZE * LENGTH;

    ITEM_TYPE data[LENGTH] = {};

    constexpr size_t length() const { return LENGTH; }
    constexpr size_t size_bytes() const { return SIZE; }

    constexpr ITEM_TYPE& operator[](size_t index){ return data[index]; }
};


// struct AssetManager
// {
//     Arena* arena = nullptr;

//     static AssetManager *instance;

//     AssetManager()
//     {
//         arena = Memory::instance->get_arena(8000);
//     }
//     ~AssetManager()
//     {
//         Memory::instance->free_arena(arena);
//     }
// };

// struct Vertex
// {
//     nem::float3 position;
// };

// struct Mesh
// {

// };

#if USE_CRT
int main()
#else
extern "C" void __stdcall _main()
#endif
{
    // ImGui_ImplWin32_EnableDpiAwareness();
    

    Memory memory;
    memory.init();
    Level level;
    level.Init();

    Input input;
    Window window;

    debug_print_region(memory.pool.start, "pool");
    level.Cleanup();

    Music music;
    
    array<int, 10> arr;
    arr[0] = 4;
    arr[9] = 5;
    list<int> lst;
    lst.push_back(0x01ABCDEF);
    lst.push_back(0xFACEDEAD);
    lst.push_back(0xFEED09AB);

    prints("My float: ");

    char str[50];
    my_snprintf(str, 50, "Hello %.2f <%s>\n", nem::PI<float>, "World!");
    str[49] = 0;
    prints(str);

    lst.remove_at(1);
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

    gl_load_functions();

    float mesh[9];
    float scale = 0.4f;
    mesh[0]     = scale * nem::cos<float>(0.f);
    mesh[1]     = scale * nem::sin<float>(0.f);
    mesh[2]     = 0.f;
    mesh[3] = scale * nem::cos<float>(0.f + nem::PI<float> * 2.f/3.f);
    mesh[4] = scale * nem::sin<float>(0.f + nem::PI<float> * 2.f/3.f);
    mesh[5]     = 0.f;
    mesh[6] = scale * nem::cos<float>(0.f + nem::PI<float> * 4.f/3.f);
    mesh[7] = scale * nem::sin<float>(0.f + nem::PI<float> * 4.f/3.f);
    mesh[8]     = 0.f;

    float vertices[] = {
        -0.5f, -0.5f, 0.0f,
         0.5f, -0.5f, 0.0f,
         0.0f,  0.5f, 0.0f
    };

    float rad = 0.f;
    nem::quat rotation = nem::from_axis_angle(nem::normalize(nem::float3(0, 1, 1)), rad);
    nem::quat delta_rot = nem::from_axis_angle(nem::normalize(nem::float3(0, 1, 1)), 0.01f);

    // list<float> l;

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

    Clock clock;
    clock.init(60);

    // Music::Note notes[5] = { Music::Note::A4, Music::Note::E8, Music::Note::G2, Music::Note::F3, Music::Note::DSharp1 };
    Music::Note notes[3] = { Music::Note::C4, Music::Note::D4, Music::Note::E4 };

    // Main loop
    float time = 0;
    float delta_time = 0.f;
    float angle = 0.f;
    while (window.is_running())
    {
        delta_time = clock.tick();
        input.tick();

        window.begin_frame();

        glClearColor(0.2f, 0.1f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glBindVertexArray(VAO);
        float speed = 1.0f * delta_time;
        float scale = 0.4f;
        angle += speed;
        //vertices[0]     = scale * nem::sin<float>(angle) * window.aspect_ratio.x;
        //vertices[1]     = scale * nem::cos<float>(angle) * window.aspect_ratio.y;
        //vertices[0 + 3] = scale * nem::sin<float>(angle + nem::PI<float> * 2.f/3.f)  * window.aspect_ratio.x;
        //vertices[1 + 3] = scale * nem::cos<float>(angle + nem::PI<float> * 2.f/3.f)  * window.aspect_ratio.y;
        //vertices[0 + 6] = scale * nem::sin<float>(angle + nem::PI<float> * 4.f/3.f)  * window.aspect_ratio.x;
        //vertices[1 + 6] = scale * nem::cos<float>(angle + nem::PI<float> * 4.f/3.f)  * window.aspect_ratio.y;

        // rotation = nem::normalize(rotation * delta_rot);
        rad += delta_time;
        nem::quat rotation =  nem::normalize(nem::from_axis_angle(nem::normalize(nem::float3(1, 1, 1)), rad));
        nem::mat4 transform = nem::homogenous(nem::norm_quaterion_to_rotation_matrix(rotation));
        for (int v = 0; v < 3; ++v)
        {
            nem::float4 vertex(mesh[v * 3 + 0], mesh[v * 3 + 1], mesh[v * 3 + 2], 1.0f);
            vertex = transform * vertex;
            vertices[v * 3 + 0] = vertex[0] * window.aspect_ratio.x;
            vertices[v * 3 + 1] = vertex[1] * window.aspect_ratio.y;
            vertices[v * 3 + 2] = vertex[2];
        }

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
            music.play(Music::Note::ASharp1);
        }

        if (time > nem::TWO_PI<float>){
            time = 0;
        }

        music.repeat_sequence(notes, sizeof(notes), 0.4f);

        window.end_frame();

        //prints("delta time: ");
        //printf(delta_time);
        //prints();
        time += delta_time;
    }

    memory.clean();

    music.clean();    

    ExitProcess(0);
}
cmake_minimum_required(VERSION 3.20)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
project(nano LANGUAGES CXX)

add_executable(nano
    src/main.cpp
    src/glf.cpp
    include/tin.h
    src/tin.cpp
    include/console.h
    src/console.cpp
)

if (SHOW_CONSOLE)
    set(SybSystem "/SUBSYSTEM:CONSOLE")
else()
    set(SybSystem "/SUBSYSTEM:WINDOWS")
endif()

include(FetchContent)
FetchContent_Declare(
    nem
    GIT_REPOSITORY https://github.com/AnanasikDev/Not-Enough-Math.git
    GIT_TAG 6ea5c2
)

set(NEM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(NEM_BUILD_BENCH OFF CACHE BOOL "" FORCE)
set(NEM_BUILD_DEMO OFF CACHE BOOL "" FORCE)
set(ANEM_ERR_THROW OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(nem)

add_library(imgui STATIC
    ${CMAKE_SOURCE_DIR}/external/imgui/imgui_draw.cpp
    ${CMAKE_SOURCE_DIR}/external/imgui/imgui_internal.h
    ${CMAKE_SOURCE_DIR}/external/imgui/imgui_tables.cpp
    ${CMAKE_SOURCE_DIR}/external/imgui/imgui_widgets.cpp
    ${CMAKE_SOURCE_DIR}/external/imgui/imgui.cpp
    ${CMAKE_SOURCE_DIR}/external/imgui/imgui.h
    ${CMAKE_SOURCE_DIR}/external/imgui/imconfig.h
    ${CMAKE_SOURCE_DIR}/external/imgui/imstb_rectpack.h
    ${CMAKE_SOURCE_DIR}/external/imgui/imstb_textedit.h
    ${CMAKE_SOURCE_DIR}/external/imgui/imstb_truetype.h
    ${CMAKE_SOURCE_DIR}/external/imgui/backends/imgui_impl_win32.h
    ${CMAKE_SOURCE_DIR}/external/imgui/backends/imgui_impl_win32.cpp
    ${CMAKE_SOURCE_DIR}/external/imgui/backends/imgui_impl_opengl3.h
    ${CMAKE_SOURCE_DIR}/external/imgui/backends/imgui_impl_opengl3.cpp
    ${CMAKE_SOURCE_DIR}/external/imgui/backends/imgui_impl_opengl3_loader.h
)
target_include_directories(nano PRIVATE ${CMAKE_SOURCE_DIR}/external/imgui ${CMAKE_SOURCE_DIR}/external/imgui/backends)
target_compile_definitions(imgui PRIVATE IMGUI_DEFINE_MATH_OPERATORS)
target_compile_options(imgui PRIVATE
/O2 # should be /Os for size
/GS-
/FAs
/clang:-fno-math-errno
/Zc:threadSafeInit-
/EHs-c-
)
target_link_options(imgui PRIVATE
/errorlimit:0
)

target_include_directories(nano PRIVATE ${CMAKE_SOURCE_DIR}/include)
if (USE_CRT)
    target_compile_definitions(nano PRIVATE USE_CRT=1 _HAS_EXCEPTIONS=0 _VCRT_DECLARED_NEW=1)
    target_link_libraries(nano PRIVATE
        kernel32
        user32
        gdi32
        opengl32
        winmm
        dwmapi
    )
else()
    target_compile_definitions(nano PRIVATE USE_CRT=0 _HAS_EXCEPTIONS=0 _VCRT_DECLARED_NEW=1)
    set(CMAKE_CXX_STANDARD_LIBRARIES "")
    set(CMAKE_C_STANDARD_LIBRARIES "")
    target_compile_options(nano PRIVATE
        /Os # should be /Os for size
        /GS-
        /FAs
        /clang:-fno-math-errno
        /Zc:threadSafeInit-
        /EHs-c-
    )
    target_link_options(nano PRIVATE
        /NODEFAULTLIB
        /ENTRY:_main
        ${SybSystem}
        /OPT:REF
        /OPT:ICF
        /MERGE:.rdata=.text
        /MAP:nano.map
        /errorlimit:0
    )
    target_link_libraries(nano PRIVATE
        kernel32
        user32
        gdi32
        opengl32
        winmm
        dwmapi
        nem
        shell32
        imm32
    )
endif()
target_link_libraries(nano PRIVATE imgui)

function(add_build_stats target)
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -DTARGET_NAME=${target}
            -DTARGET_FILE=$<TARGET_FILE:${target}>
            -DBUILD_CONFIG=$<CONFIG>
            -DSTATS_FILE=${CMAKE_SOURCE_DIR}/.buildstats
            -P ${CMAKE_SOURCE_DIR}/cmake/WriteBuildStats.cmake
        COMMENT "Recording build stats for ${target}"
    )
endfunction()

add_build_stats(nano)
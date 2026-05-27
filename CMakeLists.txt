cmake_minimum_required(VERSION 3.20)
project(nano LANGUAGES CXX)

add_executable(nano src/main.cpp src/glf.cpp)

if (SHOW_CONSOLE)
    set(SybSystem "/SUBSYSTEM:CONSOLE")
else()
    set(SybSystem "/SUBSYSTEM:WINDOWS")
endif()

if (USE_CRT)
    target_compile_definitions(nano PRIVATE USE_CRT=1)
    target_link_libraries(nano PRIVATE
        kernel32
        user32
        gdi32
        opengl32
        winmm
    )
else()
    target_compile_definitions(nano PRIVATE USE_CRT=0)
    set(CMAKE_CXX_STANDARD_LIBRARIES "")
    set(CMAKE_C_STANDARD_LIBRARIES "")
    target_compile_options(nano PRIVATE
        /Os # should be /Os for size
        /GS-
        /FAs
        /clang:-fno-math-errno
        /Zc:threadSafeInit-
    )
    target_link_options(nano PRIVATE
        /NODEFAULTLIB
        /ENTRY:_main
        ${SybSystem}
        /OPT:REF
        /OPT:ICF
        /MERGE:.rdata=.text
        /MAP:nano.map
    )
    target_link_libraries(nano PRIVATE
        kernel32
        user32
        gdi32
        opengl32
        winmm
    )
endif()

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
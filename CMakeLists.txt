cmake_minimum_required(VERSION 3.5)
project(STREAMVBYTE VERSION "2.0.0")

set(STREAMVBYTE_LIB_VERSION "2.0.0" CACHE STRING "streamvbyte library version")
set(STREAMVBYTE_LIB_SOVERSION "2" CACHE STRING "streamvbyte library soversion")

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

include(CheckCCompilerFlag)
include(GNUInstallDirs)

option(STREAMVBYTE_WERROR "Treat warnings as errors." OFF)
option(STREAMVBYTE_WALL "Emit all warnings during compilation." ON)

if(STREAMVBYTE_WERROR)
    if(MSVC)
        check_c_compiler_flag("/WX" WERROR_MSVC_SUPPORTED)
        if(WERROR_MSVC_SUPPORTED)
            add_compile_options(/WX)
        endif()
        # Wall + WX may throw errors from the corecrt headers. Workaround:
        if(STREAMVBYTE_WALL)
            check_c_compiler_flag("/W3" W3_MSVC_SUPPORTED)
            if(W3_MSVC_SUPPORTED)
                add_compile_options(/W3)
            endif()
            check_c_compiler_flag("/w34714" SUPPRESS_C34714_MSVC_SUPPORTED)
            if(SUPPRESS_C34714_MSVC_SUPPORTED)
                add_compile_options(/w34714)
            endif()
            check_c_compiler_flag("/wd5045" SUPPRESS_SPECTRE_MSVC_SUPPORTED)
            if(SUPPRESS_SPECTRE_MSVC_SUPPORTED)
                add_compile_options(/wd5045)
            endif()
            check_c_compiler_flag("/sdl" SDL_MSVC_SUPPORTED)
            if(SDL_MSVC_SUPPORTED)
                add_compile_options(/sdl)
            endif()
        endif()
    else()
        check_c_compiler_flag(-Werror WERROR_GNU_SUPPORTED)
        if(WERROR_GNU_SUPPORTED)
            add_compile_options(-Werror)
        endif()
    endif()
endif()

if(STREAMVBYTE_WALL AND NOT(STREAMVBYTE_WERROR))
    if(MSVC)
        check_c_compiler_flag("/Wall" WALL_MSVC_SUPPORTED)
        if(WALL_MSVC_SUPPORTED)
            add_compile_options(/Wall)
        endif()
    else()
        check_c_compiler_flag(-Wall WALL_GNU_SUPPORTED)
        if(WALL_GNU_SUPPORTED)
            add_compile_options(-Wall)
        endif()
    endif()
endif()

option(STREAMVBYTE_SANITIZE "Sanitize addresses" OFF)

if(NOT CMAKE_BUILD_TYPE)
    message(STATUS "No build type selected")
    if(STREAMVBYTE_SANITIZE)
        message(STATUS "Default to Debug")
        set(CMAKE_BUILD_TYPE Debug
            CACHE STRING "Choose the type of build."
            FORCE
        )
    else()
        message(STATUS "Default to Release")
        set(CMAKE_BUILD_TYPE Release
            CACHE STRING "Choose the type of build."
            FORCE
        )
    endif()
endif()

if(STREAMVBYTE_SANITIZE)
    message(STATUS "Enabling memory sanitizer.")
    add_compile_options(
        -fsanitize=address
        -fno-omit-frame-pointer
        -fno-sanitize-recover=all
    )
    add_link_options(
        -fsanitize=address
        -fno-omit-frame-pointer
        -fno-sanitize-recover=all
    )
endif()

if(MSVC)
    add_definitions("-D__restrict__=__restrict")
endif()

set(STREAMVBYTE_SRCS
    ${PROJECT_SOURCE_DIR}/src/streamvbyte_encode.c
    ${PROJECT_SOURCE_DIR}/src/streamvbyte_decode.c
    ${PROJECT_SOURCE_DIR}/src/streamvbyte_zigzag.c
    ${PROJECT_SOURCE_DIR}/src/streamvbytedelta_encode.c
    ${PROJECT_SOURCE_DIR}/src/streamvbytedelta_decode.c
    ${PROJECT_SOURCE_DIR}/src/streamvbyte_0124_encode.c
    ${PROJECT_SOURCE_DIR}/src/streamvbyte_0124_decode.c
)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_library(streamvbyte "${STREAMVBYTE_SRCS}")
target_link_libraries(streamvbyte ${BASE_FLAGS})

set_target_properties(
    streamvbyte
    PROPERTIES
        VERSION "${STREAMVBYTE_LIB_VERSION}"
        SOVERSION "${STREAMVBYTE_LIB_SOVERSION}"
        WINDOWS_EXPORT_ALL_SYMBOLS YES
)

target_include_directories(
    streamvbyte
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/include>
)

install(
    FILES
        ${PROJECT_SOURCE_DIR}/include/streamvbyte.h
        ${PROJECT_SOURCE_DIR}/include/streamvbytedelta.h
        ${PROJECT_SOURCE_DIR}/include/streamvbyte_zigzag.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(
    TARGETS streamvbyte
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

option(STREAMVBYTE_SANITIZE_UNDEFINED "Sanitize undefined behavior" OFF)
if(STREAMVBYTE_SANITIZE_UNDEFINED)
    add_compile_options(-fsanitize=undefined -fno-sanitize-recover=all)
    add_link_options(-fsanitize=undefined -fno-sanitize-recover=all)
endif()

message(STATUS "CMAKE_SYSTEM_PROCESSOR: " ${CMAKE_SYSTEM_PROCESSOR})
message(STATUS "CMAKE_BUILD_TYPE: " ${CMAKE_BUILD_TYPE}) # this tends to be "sticky" so you can remain unknowingly in debug mode
message(STATUS "CMAKE_C_COMPILER: " ${CMAKE_C_COMPILER}) # important to know which compiler is used
message(STATUS "CMAKE_C_FLAGS: " ${CMAKE_C_FLAGS}) # important to know the flags
message(STATUS "CMAKE_C_FLAGS_DEBUG: " ${CMAKE_C_FLAGS_DEBUG})
message(STATUS "CMAKE_C_FLAGS_RELEASE: " ${CMAKE_C_FLAGS_RELEASE})

# build programs

# example
option(STREAMVBYTE_ENABLE_EXAMPLES "Enable examples for streamvbyte" OFF)
if(STREAMVBYTE_ENABLE_EXAMPLES)
    add_executable(example ${PROJECT_SOURCE_DIR}/examples/example.c)
    target_link_libraries(example streamvbyte)
endif()

option(STREAMVBYTE_ENABLE_TESTS "Enable unit tests for streamvbyte" OFF)
if(STREAMVBYTE_ENABLE_TESTS)
    if(NOT MSVC)
        # perf
        add_executable(perf ${PROJECT_SOURCE_DIR}/tests/perf.c)
        target_link_libraries(perf streamvbyte)
        target_link_libraries(perf m)
    endif()

    # writeseq
    add_executable(writeseq ${PROJECT_SOURCE_DIR}/tests/writeseq.c)
    target_link_libraries(writeseq streamvbyte)

    # unit
    add_executable(unit ${PROJECT_SOURCE_DIR}/tests/unit.c)
    target_link_libraries(unit streamvbyte)
    target_include_directories(unit PRIVATE ${PROJECT_SOURCE_DIR}/src)

    enable_testing()

    # add unit tests
    add_test(NAME unit COMMAND unit)
    add_custom_target(check COMMAND ctest --output-on-failure DEPENDS unit)
endif()

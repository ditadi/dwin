cmake_minimum_required(VERSION 3.20)

# force apple clang for proper objective-c framework support
set(CMAKE_C_COMPILER "/usr/bin/clang")
set(CMAKE_OBJC_COMPILER "/usr/bin/clang")

project(dwin C OBJC)

# generate compile_commands.json for clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

# enable objective-c
enable_language(OBJC)
set(CMAKE_OBJC_STANDARD 11)

# macOS specific settings
set(CMAKE_OSX_DEPLOYMENT_TARGET "15.0")
set(CMAKE_OSX_SYSROOT "/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk")

# include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# source files
set(SOURCES)

# tests
option(BUILD_TESTS "Build tests" OFF)
if(BUILD_TESTS)
    enable_testing()

    add_executable(test_config tests/test_config.m)
    target_include_directories(test_config PRIVATE ${CMAKE_SOURCE_DIR}/include)
    target_link_libraries(test_config "-framework Foundation" "-framework Carbon")
    add_test(NAME ConfigTests COMMAND test_config)

endif()

add_executable(dwin main.c)

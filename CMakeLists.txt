cmake_minimum_required(VERSION 3.20)

# force apple clang for proper objective-c framework support
set(CMAKE_C_COMPILER "/usr/bin/clang")
set(CMAKE_OBJC_COMPILER "/usr/bin/clang")

project(dwin VERSION 0.1.0 LANGUAGES C OBJC)

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

# warnings
add_compile_options(-Wall -Wextra -Wpedantic)

# =============================================================================
# Core library (pure C, no macOS deps)
# =============================================================================

add_library(dwin_core STATIC
    src/core/wm_state.c
    src/core/wm_actions.c
    src/core/wm_layout.c
    src/core/wm_config.c
)

target_include_directories(dwin_core PUBLIC src/core)

# =============================================================================
# macOS app bundle
# =============================================================================

add_executable(dwin MACOSX_BUNDLE
    src/app/main.m
    src/platform/macos/AppDelegate.m
    src/platform/macos/mac_status_bar.m
    src/platform/macos/mac_event_tap.m
    src/platform/macos/mac_effects.m
)

target_include_directories(dwin PRIVATE
    src/app
    src/platform/macos
    src/core
)

target_link_libraries(dwin PRIVATE
    dwin_core
    "-framework Cocoa"
    "-framework ApplicationServices"
)

set_target_properties(dwin PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/Info.plist"
    MACOSX_BUNDLE_BUNDLE_NAME "dwin"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.victor.dwin"
    MACOSX_BUNDLE_BUNDLE_VERSION "0.1.0"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "0.1.0"
)

# =============================================================================
# Tests
# =============================================================================

option(BUILD_TESTS "Build tests" OFF)
if(BUILD_TESTS)
    enable_testing()

    # Core tests (pure C)
    add_executable(test_core tests/test_core.c)
    target_link_libraries(test_core PRIVATE dwin_core)
    target_include_directories(test_core PRIVATE src/core)
    add_test(NAME CoreTests COMMAND test_core)
endif()

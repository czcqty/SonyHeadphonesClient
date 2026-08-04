cmake_minimum_required(VERSION 3.31 FATAL_ERROR)
project(
    SonyHeadphonesClient
    VERSION 2.0.3
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(MDR_BUILD_WITH_ASAN "Build with Address Sanitizer enabled" OFF)
if (MDR_BUILD_WITH_ASAN)
    if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        message(WARNING "ASAN not enabled on MSVC by *us*. You're on your own.")
    else ()
        add_compile_options("-fsanitize=address")
        add_link_options("-fsanitize=address")
    endif ()
endif ()

option(MDR_DEBUG "Build with debug flags enabled" OFF)
if (MDR_DEBUG)
    message(STATUS "Debugging features *ON*. Enjoy the logs.")
    add_compile_definitions(MDR_DEBUG)
    add_compile_definitions(_DEBUG)
endif ()

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    add_compile_options("/Zc:preprocessor")
    add_compile_options("/utf-8")
else()
    add_compile_options("-Wall" "-Wno-unused-function" "-Wno-unused-variable")
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}")

include(DetectPlatform.cmake)
MDR_DetectPlatform() # Sets MDR_PLATFORM_OS
include(DetectGitInfo.cmake)
MDR_DetectGitInfo() # Sets MDR_GIT_COMMIT_HASH and MDR_GIT_BRANCH_NAME

add_compile_definitions("MDR_PLATFORM_${MDR_PLATFORM_OS}")

cmake_policy(SET CMP0171 NEW)

include(CTest)

add_subdirectory(contrib)
option(MDR_ENABLE_CODEGEN "Enable codegen targets (requires LLVM)" ON)
if (MDR_ENABLE_CODEGEN)
    add_subdirectory(tooling)
endif()

include(tooling/Codegen.cmake)
add_subdirectory(libmdr)
add_subdirectory(gui)

set(CPACK_PACKAGE_NAME "SonyHeadphonesClient")
set(CPACK_PACKAGE_VENDOR "czcqty")
set(CPACK_PACKAGE_CONTACT "czcqty")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "Cross-platform local controls for supported Sony headphones")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/czcqty/SonyHeadphonesClient")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "SonyHeadphonesClient")

if(WIN32)
    set(CPACK_GENERATOR "NSIS")
    set(CPACK_PACKAGE_FILE_NAME
        "SonyHeadphonesClient-${PROJECT_VERSION}-windows-x64-setup")
    set(CPACK_NSIS_DISPLAY_NAME "SonyHeadphonesClient")
    set(CPACK_NSIS_PACKAGE_NAME "SonyHeadphonesClient")
    set(CPACK_PACKAGE_EXECUTABLES
        "SonyHeadphonesClient" "SonyHeadphonesClient")
    set(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/gui/assets/icons/app-icon.ico")
    set(CPACK_NSIS_MUI_UNIICON "${CMAKE_SOURCE_DIR}/gui/assets/icons/app-icon.ico")
    set(CPACK_NSIS_INSTALLED_ICON_NAME "bin/SonyHeadphonesClient.exe")
    set(CPACK_NSIS_INSTALL_ROOT "$LOCALAPPDATA")
    set(CPACK_NSIS_DEFINES "RequestExecutionLevel user")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_PACKAGE_FILE_NAME
        "SonyHeadphonesClient-${PROJECT_VERSION}-macos-universal")
    set(CPACK_DMG_VOLUME_NAME "SonyHeadphonesClient")
else()
    # Linux releases use the dedicated AppImage packaging step. Keep CPack's
    # local fallback deterministic for developers.
    set(CPACK_GENERATOR "TGZ")
    set(CPACK_PACKAGE_FILE_NAME
        "SonyHeadphonesClient-${PROJECT_VERSION}-linux-x86_64")
endif()

include(CPack)

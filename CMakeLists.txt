cmake_minimum_required(VERSION 3.31 FATAL_ERROR)
project(
    SonyHeadphonesClient
    VERSION 2.0.4
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(MDR_OPTIMIZE_RELEASE_SIZE
    "Optimize release binaries for package size" ON)
if(MDR_OPTIMIZE_RELEASE_SIZE)
    if(CMAKE_CONFIGURATION_TYPES OR CMAKE_BUILD_TYPE MATCHES "^(Release|MinSizeRel)$")
        include(CheckIPOSupported)
        check_ipo_supported(RESULT MDR_IPO_SUPPORTED OUTPUT MDR_IPO_ERROR
            LANGUAGES C CXX)
        if(MDR_IPO_SUPPORTED)
            set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
        else()
            message(WARNING "Release IPO/LTO is unavailable: ${MDR_IPO_ERROR}")
        endif()
    endif()

    if(MSVC)
        add_compile_options(
            "$<$<CONFIG:Release>:/Os>"
            "$<$<CONFIG:Release>:/Gy>"
            "$<$<CONFIG:Release>:/Gw>"
        )
        add_link_options(
            "$<$<CONFIG:Release>:/OPT:REF>"
            "$<$<CONFIG:Release>:/OPT:ICF>"
        )
    else()
        add_compile_options(
            "$<$<CONFIG:Release>:-Os>"
            "$<$<CONFIG:Release>:-ffunction-sections>"
            "$<$<CONFIG:Release>:-fdata-sections>"
        )
        if(APPLE)
            add_link_options("$<$<CONFIG:Release>:-Wl,-dead_strip>")
        else()
            add_link_options("$<$<CONFIG:Release>:-Wl,--gc-sections>")
        endif()
    endif()
endif()

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
    # CPack exposes the desktop-shortcut checkbox on its NSIS install-options
    # page. The page is enabled by CPACK_NSIS_MODIFY_PATH, while this list
    # connects the checkbox to the executable shortcut.
    set(CPACK_CREATE_DESKTOP_LINKS "SonyHeadphonesClient")
    set(CPACK_NSIS_MODIFY_PATH ON)
    set(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/gui/assets/icons/app-icon.ico")
    set(CPACK_NSIS_MUI_UNIICON "${CMAKE_SOURCE_DIR}/gui/assets/icons/app-icon.ico")
    set(CPACK_NSIS_INSTALLED_ICON_NAME "bin/SonyHeadphonesClient.exe")
    set(CPACK_NSIS_INSTALL_ROOT "$LOCALAPPDATA")
    set(CPACK_NSIS_DEFINES "RequestExecutionLevel user")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
    set(_MDR_MACOS_PACKAGE_ARCH "${CMAKE_OSX_ARCHITECTURES}")
    if(NOT _MDR_MACOS_PACKAGE_ARCH)
        set(_MDR_MACOS_PACKAGE_ARCH "${CMAKE_SYSTEM_PROCESSOR}")
    endif()
    if(_MDR_MACOS_PACKAGE_ARCH MATCHES ";")
        set(_MDR_MACOS_PACKAGE_ARCH "universal")
    endif()
    string(REPLACE ";" "-" _MDR_MACOS_PACKAGE_ARCH
        "${_MDR_MACOS_PACKAGE_ARCH}")
    set(CPACK_PACKAGE_FILE_NAME
        "SonyHeadphonesClient-${PROJECT_VERSION}-macos-${_MDR_MACOS_PACKAGE_ARCH}")
    set(CPACK_DMG_VOLUME_NAME "SonyHeadphonesClient")
else()
    # Linux releases use the dedicated AppImage packaging step. Keep CPack's
    # local fallback deterministic for developers.
    set(CPACK_GENERATOR "TGZ")
    set(CPACK_PACKAGE_FILE_NAME
        "SonyHeadphonesClient-${PROJECT_VERSION}-linux-x86_64")
endif()

include(CPack)

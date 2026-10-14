find_package(Qt6 6.5 REQUIRED COMPONENTS Core)
set(CMAKE_AUTOMOC ON)

add_library(device_backend STATIC
    DeviceService.cpp DeviceService.hpp
    ViewModels.cpp ViewModels.hpp
    DeviceProfileRepository.cpp DeviceProfileRepository.hpp
    DeviceCapabilities.cpp DeviceCapabilities.hpp
    MdrSession.cpp MdrSession.hpp
    BackendProtocol.cpp BackendProtocol.hpp
)
target_include_directories(device_backend PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(device_backend PUBLIC Qt6::Core mdr)

add_executable(SoundConnectBackend BackendMain.cpp)
target_link_libraries(SoundConnectBackend PRIVATE device_backend)
set_target_properties(SoundConnectBackend PROPERTIES WIN32_EXECUTABLE FALSE MACOSX_BUNDLE FALSE)

include(GNUInstallDirs)
if(WIN32 AND NOT MDR_BUILD_QT_FRONTEND)
    # A console Qt Core host needs no plugins. vcpkg can supply Qt without
    # windeployqt, so use CMake's PE dependency scanner for its runtime closure.
    install(CODE "if(POLICY CMP0207)\n    cmake_policy(SET CMP0207 NEW)\nendif()")
    install(TARGETS SoundConnectBackend
        RUNTIME_DEPENDENCIES
            DIRECTORIES "$<TARGET_FILE_DIR:Qt6::Core>" "$<TARGET_FILE_DIR:SoundConnectBackend>"
            PRE_EXCLUDE_REGEXES "api-ms-.*" "ext-ms-.*"
            POST_EXCLUDE_REGEXES ".*[\\\\/]Windows[\\\\/].*"
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION ${CMAKE_INSTALL_BINDIR})
    include(InstallRequiredSystemLibraries)
else()
    install(TARGETS SoundConnectBackend RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
install(FILES README.md DESTINATION ${CMAKE_INSTALL_DATADIR}/SoundConnectBackend)
if(NOT MDR_BUILD_QT_FRONTEND)
    install(FILES "${CMAKE_SOURCE_DIR}/LICENSE" DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/SonyHeadphonesClient)
    if(NOT WIN32)
        qt_generate_deploy_app_script(TARGET SoundConnectBackend OUTPUT_SCRIPT backend_deploy
            NO_TRANSLATIONS NO_UNSUPPORTED_PLATFORM_ERROR)
        install(SCRIPT ${backend_deploy})
    endif()
endif()

if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_executable(device_service_tests tests/DeviceServiceTests.cpp)
    target_link_libraries(device_service_tests PRIVATE device_backend Qt6::Test)
    add_test(NAME backend-device-service COMMAND device_service_tests)
    add_executable(backend_protocol_tests tests/ProtocolTests.cpp)
    target_link_libraries(backend_protocol_tests PRIVATE device_backend Qt6::Test)
    add_test(NAME backend-protocol COMMAND backend_protocol_tests)
    add_executable(mdr_regression_tests tests/BackendTests.cpp)
    target_link_libraries(mdr_regression_tests PRIVATE mdr Qt6::Core Qt6::Test)
    add_test(NAME backend-mdr-regressions COMMAND mdr_regression_tests)
    add_test(NAME backend-process-contract COMMAND ${Python3_EXECUTABLE}
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_process.py" "$<TARGET_FILE:SoundConnectBackend>")
    add_test(NAME backend-legacy-client COMMAND ${Python3_EXECUTABLE}
        "${CMAKE_SOURCE_DIR}/clients/common/test_backend.py" "$<TARGET_FILE:SoundConnectBackend>")
    add_test(NAME backend-native-catalog COMMAND ${Python3_EXECUTABLE}
        "${CMAKE_SOURCE_DIR}/clients/common/test_catalog.py" "$<TARGET_FILE:SoundConnectBackend>")
    set_tests_properties(backend-process-contract PROPERTIES TIMEOUT 90)
endif()

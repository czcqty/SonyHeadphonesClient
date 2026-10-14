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
install(TARGETS SoundConnectBackend RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES README.md DESTINATION ${CMAKE_INSTALL_DATADIR}/SoundConnectBackend)
if(NOT MDR_BUILD_QT_FRONTEND)
    install(FILES "${CMAKE_SOURCE_DIR}/LICENSE" DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/SonyHeadphonesClient)
    qt_generate_deploy_app_script(TARGET SoundConnectBackend OUTPUT_SCRIPT backend_deploy
        NO_TRANSLATIONS NO_UNSUPPORTED_PLATFORM_ERROR)
    install(SCRIPT ${backend_deploy})
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

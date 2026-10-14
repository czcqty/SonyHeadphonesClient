find_package(Qt6 REQUIRED COMPONENTS
    LinguistTools
    Qml
    Quick
    QuickControls2
    Widgets
)
qt_standard_project_setup()

set(CMAKE_AUTOMOC ON)

set(GUI_APP_TARGET ${PROJECT_NAME})
set(GUI_VIEWMODEL_TEST_TARGET ${PROJECT_NAME}_viewmodel_tests)
set(GUI_QML_TEST_TARGET ${PROJECT_NAME}_qml_tests)
set(GUI_BACKEND_TEST_TARGET ${PROJECT_NAME}_backend_tests)
set(GUI_VISUAL_DIFF_TARGET ${PROJECT_NAME}_visual_diff)

set(GUI_SOURCES
    quick/main.cpp
    quick/PanelController.cpp
    quick/AppController.cpp
    quick/DeviceProfileRepository.cpp
    quick/MdrSession.cpp
    quick/ViewModels.cpp
    DeviceCapabilities.cpp
)

set(GUI_HEADERS
    quick/PanelController.hpp
    quick/AppController.hpp
    quick/DeviceProfileRepository.hpp
    quick/MdrSession.hpp
    quick/ViewModels.hpp
    DeviceCapabilities.hpp
)

set(GUI_QML_FILES
    qml/Main.qml
    qml/I18n.qml
    qml/Tokens.qml
    qml/AppSlider.qml
    qml/AppSwitch.qml
    qml/AppComboBox.qml
    qml/SegmentButton.qml
    qml/SettingRow.qml
    qml/Toolbar.qml
    qml/ThemeToggle.qml
    qml/LanguageToggle.qml
    qml/ConnectionOverlay.qml
    qml/ShortcutCard.qml
    qml/HomePage.qml
    qml/DeviceOptionsPage.qml
    qml/TouchAssignmentsPage.qml
    qml/VoiceGuidancePage.qml
    qml/NoiseControlPage.qml
    qml/EqualizerPage.qml
    qml/DevicesPage.qml
    qml/ConnectionPage.qml
)

set(GUI_DEVICE_ASSETS
    assets/devices/linkbuds-s.png
    assets/devices/mdr-xb950bt.png
    assets/devices/wf-1000xm4.png
    assets/devices/wf-1000xm5.png
    assets/devices/wf-1000xm6-original.png
    assets/devices/wf-c510-original.png
    assets/devices/wf-l910-original.png
    assets/devices/wf-c700n.png
    assets/devices/wh-1000xm2.png
    assets/devices/wh-1000xm3.png
    assets/devices/wh-1000xm4.png
    assets/devices/wh-1000xm5.png
    assets/devices/wh-1000xm6.png
    assets/devices/wh-ch520.png
    assets/devices/wh-ch720n.png
    assets/devices/wh-xb900n.png
    assets/devices/wh-xb910n.png
)

qt_add_executable(${GUI_APP_TARGET}
    ${GUI_SOURCES}
    ${GUI_HEADERS}
)

set_source_files_properties(qml/Tokens.qml PROPERTIES
    QT_QML_SINGLETON_TYPE TRUE
)
set_source_files_properties(qml/I18n.qml PROPERTIES
    QT_QML_SINGLETON_TYPE TRUE
)
foreach(_qml_file IN LISTS GUI_QML_FILES)
    get_filename_component(_qml_name "${_qml_file}" NAME)
    set_source_files_properties("${_qml_file}" PROPERTIES
        QT_RESOURCE_ALIAS "${_qml_name}")
endforeach()

qt_add_qml_module(${GUI_APP_TARGET}
    URI HeadphonesClient
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml
    QML_FILES ${GUI_QML_FILES}
)

qt_add_resources(${GUI_APP_TARGET} "device_assets"
    PREFIX "/devices"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/assets/devices"
    FILES ${GUI_DEVICE_ASSETS}
)

qt_add_resources(${GUI_APP_TARGET} "application_icon"
    PREFIX "/icons"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons"
    FILES assets/icons/app-icon-512.png
)

target_include_directories(${GUI_APP_TARGET} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/libmdr/include
)
target_link_libraries(${GUI_APP_TARGET} PRIVATE
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Widgets
    mdr
)
target_compile_definitions(${GUI_APP_TARGET} PRIVATE
    MDR_GIT_COMMIT_HASH="${MDR_GIT_COMMIT_HASH}"
    MDR_GIT_BRANCH_NAME="${MDR_GIT_BRANCH_NAME}"
    MDR_VERSION="${PROJECT_VERSION}"
)

if(WIN32)
    target_sources(${GUI_APP_TARGET} PRIVATE assets/icons/app-icon.rc)
    set_target_properties(${GUI_APP_TARGET} PROPERTIES
        WIN32_EXECUTABLE TRUE
        OUTPUT_NAME "SonyHeadphonesClient"
    )

    # vcpkg has no windeployqt target, so keep development builds runnable by
    # deploying only the QML modules and plugin dependencies selected by the
    # application's compile-time Basic style. vcpkg's applocal step supplies
    # the executable's other transitive runtime dependencies.
    if(DEFINED VCPKG_TARGET_TRIPLET)
        get_filename_component(_GUI_QT_PREFIX "${Qt6_DIR}/../.." ABSOLUTE)
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            set(_GUI_QT_RUNTIME_BIN "${_GUI_QT_PREFIX}/debug/bin")
            set(_GUI_QT_RUNTIME_ROOT "${_GUI_QT_PREFIX}/debug/Qt6")
            set(_GUI_QT_DEBUG_POSTFIX "d")
        else()
            set(_GUI_QT_RUNTIME_BIN "${_GUI_QT_PREFIX}/bin")
            set(_GUI_QT_RUNTIME_ROOT "${_GUI_QT_PREFIX}/Qt6")
            set(_GUI_QT_DEBUG_POSTFIX "")
        endif()

        set(_GUI_QT_QML_PLUGINS
            "QtQml|qmlplugin"
            "QtQml/Models|modelsplugin"
            "QtQml/WorkerScript|workerscriptplugin"
            "QtQuick|qtquick2plugin"
            "QtQuick/Controls/Basic|qtquickcontrols2basicstyleplugin"
            "QtQuick/Controls/Basic/impl|qtquickcontrols2basicstyleimplplugin"
            "QtQuick/Controls/impl|qtquickcontrols2implplugin"
            "QtQuick/Layouts|qquicklayoutsplugin"
            "QtQuick/Shapes|qmlshapesplugin"
            "QtQuick/Templates|qtquicktemplates2plugin"
        )
        set(_GUI_QT_QML_RUNTIME_LIBRARIES
            Qt6QuickControls2
            Qt6QuickControls2Basic
            Qt6QuickControls2BasicStyleImpl
            Qt6QuickControls2Impl
            Qt6QuickLayouts
            Qt6QuickShapes
            Qt6QuickTemplates2
        )

        set(_GUI_QT_DEPLOY_COMMANDS
            COMMAND ${CMAKE_COMMAND} -E make_directory
                    "$<TARGET_FILE_DIR:${GUI_APP_TARGET}>/platforms"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_GUI_QT_RUNTIME_ROOT}/plugins/platforms/qwindows${_GUI_QT_DEBUG_POSTFIX}.dll"
                    "$<TARGET_FILE_DIR:${GUI_APP_TARGET}>/platforms/"
        )
        foreach(_qml_plugin IN LISTS _GUI_QT_QML_PLUGINS)
            string(REPLACE "|" ";" _qml_plugin_parts "${_qml_plugin}")
            list(GET _qml_plugin_parts 0 _qml_module)
            list(GET _qml_plugin_parts 1 _qml_plugin_name)
            set(_qml_source "${_GUI_QT_RUNTIME_ROOT}/qml/${_qml_module}")
            set(_qml_destination
                "$<TARGET_FILE_DIR:${GUI_APP_TARGET}>/Qt6/qml/${_qml_module}")
            list(APPEND _GUI_QT_DEPLOY_COMMANDS
                COMMAND ${CMAKE_COMMAND} -E make_directory "${_qml_destination}"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${_qml_source}/qmldir"
                        "${_qml_source}/${_qml_plugin_name}${_GUI_QT_DEBUG_POSTFIX}.dll"
                        "${_qml_destination}/"
            )
        endforeach()
        foreach(_runtime_library IN LISTS _GUI_QT_QML_RUNTIME_LIBRARIES)
            list(APPEND _GUI_QT_DEPLOY_COMMANDS
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${_GUI_QT_RUNTIME_BIN}/${_runtime_library}${_GUI_QT_DEBUG_POSTFIX}.dll"
                        "$<TARGET_FILE_DIR:${GUI_APP_TARGET}>/"
            )
        endforeach()

        add_custom_command(TARGET ${GUI_APP_TARGET} POST_BUILD
            ${_GUI_QT_DEPLOY_COMMANDS}
            COMMENT "Deploying minimal Qt Quick Basic development runtime"
            VERBATIM
        )
    endif()

elseif(APPLE)
    set_source_files_properties(assets/icons/app-icon.icns PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources"
    )
    target_sources(${GUI_APP_TARGET} PRIVATE assets/icons/app-icon.icns)
    set_target_properties(${GUI_APP_TARGET} PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "SonyHeadphonesClient"
        MACOSX_BUNDLE_GUI_IDENTIFIER "io.github.czcqty.SonyHeadphonesClient"
        MACOSX_BUNDLE_ICON_FILE "app-icon.icns"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        OUTPUT_NAME "SonyHeadphonesClient"
    )
else()
    set_target_properties(${GUI_APP_TARGET} PROPERTIES
        OUTPUT_NAME "SonyHeadphonesClient"
    )
endif()

include(GNUInstallDirs)
install(TARGETS ${GUI_APP_TARGET}
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(FILES "${CMAKE_SOURCE_DIR}/LICENSE"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/SonyHeadphonesClient"
)

if(UNIX AND NOT APPLE)
    install(FILES "${CMAKE_SOURCE_DIR}/packaging/io.github.czcqty.SonyHeadphonesClient.desktop"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/applications"
    )
    install(FILES assets/icons/app-icon.svg
        DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps"
        RENAME io.github.czcqty.SonyHeadphonesClient.svg
    )
    install(FILES assets/icons/app-icon-512.png
        DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/512x512/apps"
        RENAME io.github.czcqty.SonyHeadphonesClient.png
    )
endif()

qt_generate_deploy_qml_app_script(
    TARGET ${GUI_APP_TARGET}
    OUTPUT_SCRIPT _GUI_DEPLOY_SCRIPT
    NO_TRANSLATIONS
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${_GUI_DEPLOY_SCRIPT})

if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test QuickTest)
    find_package(Python3 QUIET COMPONENTS Interpreter)

    set(_GUI_QT_PLUGIN_ROOT
        "$<TARGET_FILE_DIR:Qt6::QOffscreenIntegrationPlugin>/..")
    set(_GUI_TEST_FONT_ENV)
    if(WIN32)
        set(_GUI_TEST_FONT_ENV "QT_QPA_FONTDIR=$ENV{WINDIR}/Fonts")
    endif()

    qt_add_executable(${GUI_VIEWMODEL_TEST_TARGET}
        tests/ViewModelTests.cpp
        quick/AppController.cpp
        quick/DeviceProfileRepository.cpp
        quick/MdrSession.cpp
        quick/ViewModels.cpp
        DeviceCapabilities.cpp
    )
    set_target_properties(${GUI_VIEWMODEL_TEST_TARGET}
        PROPERTIES WIN32_EXECUTABLE FALSE)
    target_include_directories(${GUI_VIEWMODEL_TEST_TARGET} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/libmdr/include
    )
    target_link_libraries(${GUI_VIEWMODEL_TEST_TARGET} PRIVATE
        Qt6::Test
        Qt6::Core
        Qt6::Gui
        mdr
    )
    add_test(NAME gui-viewmodels COMMAND ${GUI_VIEWMODEL_TEST_TARGET})

    qt_add_executable(${GUI_BACKEND_TEST_TARGET}
        tests/BackendTests.cpp
    )
    set_target_properties(${GUI_BACKEND_TEST_TARGET}
        PROPERTIES WIN32_EXECUTABLE FALSE)
    target_include_directories(${GUI_BACKEND_TEST_TARGET} PRIVATE
        ${CMAKE_SOURCE_DIR}/libmdr/include
    )
    target_link_libraries(${GUI_BACKEND_TEST_TARGET} PRIVATE
        Qt6::Test
        Qt6::Core
        mdr
    )
    add_test(NAME gui-backend-regressions COMMAND ${GUI_BACKEND_TEST_TARGET})

    qt_add_executable(SonyHeadphonesClient_panel_tests
        tests/PanelTests.cpp quick/PanelController.cpp quick/PanelController.hpp)
    target_link_libraries(SonyHeadphonesClient_panel_tests PRIVATE Qt6::Test Qt6::Widgets)
    add_test(NAME gui-panel-lifecycle COMMAND SonyHeadphonesClient_panel_tests)
    set_tests_properties(gui-panel-lifecycle PROPERTIES ENVIRONMENT
        "QT_QPA_PLATFORM=offscreen;QT_PLUGIN_PATH=${_GUI_QT_PLUGIN_ROOT};${_GUI_TEST_FONT_ENV}")

    if(Python3_Interpreter_FOUND)
        add_test(NAME gui-localization-catalog
            COMMAND ${Python3_EXECUTABLE}
                    "${CMAKE_SOURCE_DIR}/packaging/validate_translations.py")
        add_test(NAME gui-application-icons
            COMMAND ${Python3_EXECUTABLE}
                    "${CMAKE_SOURCE_DIR}/packaging/generate_icons.py"
                    --validate-only)
        add_test(NAME gui-package-metadata
            COMMAND ${Python3_EXECUTABLE}
                    "${CMAKE_SOURCE_DIR}/packaging/validate_package_metadata.py")
    endif()

    qt_add_executable(${GUI_QML_TEST_TARGET}
        tests/QmlQuickTestMain.cpp
        quick/AppController.cpp
        quick/DeviceProfileRepository.cpp
        quick/MdrSession.cpp
        quick/ViewModels.cpp
        DeviceCapabilities.cpp
    )
    set_target_properties(${GUI_QML_TEST_TARGET}
        PROPERTIES WIN32_EXECUTABLE FALSE)
    target_compile_definitions(${GUI_QML_TEST_TARGET} PRIVATE
        QUICK_TEST_SOURCE_DIR="${CMAKE_SOURCE_DIR}/tests/qml"
    )
    target_include_directories(${GUI_QML_TEST_TARGET} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/libmdr/include
    )
    qt_add_qml_module(${GUI_QML_TEST_TARGET}
        URI HeadphonesClient
        VERSION 1.0
        RESOURCE_PREFIX /qt/qml
        OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test-qml/HeadphonesClient"
        QML_FILES ${GUI_QML_FILES}
    )
    qt_add_resources(${GUI_QML_TEST_TARGET} "test_device_assets"
        PREFIX "/devices"
        BASE "${CMAKE_CURRENT_SOURCE_DIR}/assets/devices"
        FILES ${GUI_DEVICE_ASSETS})
    target_link_libraries(${GUI_QML_TEST_TARGET} PRIVATE
        Qt6::QuickTest
        Qt6::Quick
        Qt6::Qml
        Qt6::QuickControls2
        mdr
    )
    add_test(NAME gui-qml-quicktest
        COMMAND ${GUI_QML_TEST_TARGET}
                -input "${CMAKE_SOURCE_DIR}/tests/qml")
    set_tests_properties(gui-qml-quicktest PROPERTIES ENVIRONMENT
        "QT_QPA_PLATFORM=offscreen;QT_PLUGIN_PATH=${_GUI_QT_PLUGIN_ROOT};${_GUI_TEST_FONT_ENV}")
    if(WIN32)
        set_property(TEST gui-qml-quicktest APPEND PROPERTY
            ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::QuickTest>")
    endif()

    qt_add_executable(${GUI_VISUAL_DIFF_TARGET}
        tests/VisualDiff.cpp
    )
    set_target_properties(${GUI_VISUAL_DIFF_TARGET}
        PROPERTIES WIN32_EXECUTABLE FALSE)
    target_link_libraries(${GUI_VISUAL_DIFF_TARGET} PRIVATE Qt6::Gui)

    add_custom_target(run_gui_tests
        COMMAND ${CMAKE_CTEST_COMMAND}
                --test-dir "${CMAKE_BINARY_DIR}"
                --output-on-failure
                -R "^gui-"
        DEPENDS
            ${GUI_VIEWMODEL_TEST_TARGET}
            ${GUI_BACKEND_TEST_TARGET}
            SonyHeadphonesClient_panel_tests
            ${GUI_QML_TEST_TARGET}
        USES_TERMINAL
    )

    if(WIN32)
        add_custom_target(capture_qml_baselines
            COMMAND powershell -NoProfile -ExecutionPolicy Bypass
                    -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/CapturePreviewBaselines.ps1"
                    -Executable "$<TARGET_FILE:${GUI_APP_TARGET}>"
                    -OutputDirectory "${CMAKE_CURRENT_BINARY_DIR}/visual-baselines"
            DEPENDS ${GUI_APP_TARGET}
            USES_TERMINAL
        )
    endif()
endif()

set(_GUI_TRANSLATION_TARGETS ${GUI_APP_TARGET})
if(BUILD_TESTING)
    list(APPEND _GUI_TRANSLATION_TARGETS ${GUI_QML_TEST_TARGET})
endif()
qt_add_translations(
    TARGETS ${_GUI_TRANSLATION_TARGETS}
    IMMEDIATE_CALL
    TS_FILES "${CMAKE_CURRENT_SOURCE_DIR}/translations/SonyHeadphonesClient_zh_CN.ts"
    RESOURCE_PREFIX "/i18n"
    LRELEASE_OPTIONS -nounfinished
)

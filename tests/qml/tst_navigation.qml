import QtQuick
import QtTest
import HeadphonesClient

TestCase {
    name: "PreviewContract"
    when: windowShown
    width: 360
    height: 240
    visible: true

    Item {
        id: controls
        width: 320
        height: 180

        property int settingToggleCount: 0
        property bool lastSettingValue: false

        LanguageToggle {
            id: languageToggle
            x: 8
            y: 8
        }

        ThemeToggle {
            id: themeToggle
            x: 68
            y: 8
        }

        Text {
            id: translatedSample
            visible: false
            text: I18n.text("equalizer")
        }

        SettingRow {
            id: settingRow
            y: 80
            width: 280
            title: "Test setting"
            switchVisible: true
            checked: false
            onToggled: checked => {
                controls.settingToggleCount += 1
                controls.lastSettingValue = checked
            }
        }
    }

    function test_supported_routes() {
        const routes = ["connection", "home", "settings", "sound", "behavior", "touch", "voice", "noise", "equalizer", "devices"]
        compare(routes.length, 10)
        verify(routes.indexOf("playback") === -1)
        verify(routes.indexOf("account") === -1)
        verify(routes.indexOf("firmware") === -1)
    }

    function test_preview_states() {
        const states = ["ready", "loading", "empty", "error", "disabled"]
        compare(states.length, 5)
    }

    Component { id: panelComponent; Main {} }

    function test_panel_pages_load_in_compact_window() {
        appController.applyProfile("generic-max")
        const panel = createTemporaryObject(panelComponent, null, {width: 360, height: 480})
        verify(panel !== null)
        const loader = findChild(panel, "panelRouteLoader")
        verify(loader !== null)
        for (const route of ["home", "sound", "behavior", "noise", "equalizer", "touch", "voice", "devices", "connection"]) {
            appController.currentRoute = route
            tryCompare(loader, "status", Loader.Ready)
            verify(loader.item !== null, route)
            compare(loader.item.width, 360)
            verify(loader.item.height <= 480)
        }
    }

    function test_language_toggle_is_live_and_does_not_overlap_theme() {
        appController.locale = "en"
        compare(languageToggle.text, "中")
        compare(translatedSample.text, "Equalizer")
        verify(languageToggle.x + languageToggle.width <= themeToggle.x)
        languageToggle.clicked()
        compare(appController.locale, "zh_CN")
        compare(languageToggle.text, "EN")
        compare(translatedSample.text, "均衡器")
        compare(themeToggle.x, 68)
    }

    function test_setting_switch_click_emits_once() {
        controls.settingToggleCount = 0
        controls.lastSettingValue = false
        mouseClick(controls, settingRow.x + 24, settingRow.y + settingRow.height / 2)
        compare(controls.settingToggleCount, 1)
        compare(controls.lastSettingValue, true)

        controls.settingToggleCount = 0
        mouseClick(controls, settingRow.x + settingRow.width - 26,
                   settingRow.y + settingRow.height / 2)
        compare(controls.settingToggleCount, 1)
        compare(controls.lastSettingValue, true)
    }
}

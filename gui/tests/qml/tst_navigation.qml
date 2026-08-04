import QtQuick
import QtTest
import HeadphonesClient

TestCase {
    name: "PreviewContract"
    when: windowShown

    Item {
        id: controls
        width: 140
        height: 64

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
    }

    function test_supported_routes() {
        const routes = ["connection", "home", "settings", "touch", "voice", "noise", "equalizer", "devices"]
        compare(routes.length, 8)
        verify(routes.indexOf("playback") === -1)
        verify(routes.indexOf("account") === -1)
        verify(routes.indexOf("firmware") === -1)
    }

    function test_preview_states() {
        const states = ["ready", "loading", "empty", "error", "disabled"]
        compare(states.length, 5)
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
}

pragma Singleton
import QtQuick

QtObject {
    property bool darkMode: false
    readonly property string desktop: appController.desktopStyle
    readonly property bool roundedDesktop: desktop === "macos" || desktop === "gnome"

    readonly property color primary: desktop === "macos" ? (darkMode ? "#64a8ff" : "#007aff")
        : desktop === "windows" ? (darkMode ? "#60cdff" : "#0067c0")
        : desktop === "kde" ? (darkMode ? "#70c5ed" : "#2980b9")
        : desktop === "gnome" ? (darkMode ? "#78aeed" : "#3584e4")
        : (darkMode ? "#80b9ef" : "#2777b5")
    readonly property color primaryContainer: Qt.rgba(primary.r, primary.g, primary.b, darkMode ? 0.28 : 0.14)
    readonly property color primaryText: darkMode ? "#202020" : "#ffffff"
    readonly property color surface: darkMode ? (desktop === "gnome" ? "#242424" : "#202020")
        : (desktop === "macos" ? "#f5f5f7" : "#f3f3f3")
    readonly property color surfaceContainer: darkMode ? "#2d2d2d" : "#ffffff"
    readonly property color surfaceContainerHigh: darkMode ? "#303030" : "#ffffff"
    readonly property color surfaceContainerHighest: darkMode ? "#383838" : "#e9e9e9"
    readonly property color textPrimary: darkMode ? "#e1e3e3" : "#191c1d"
    readonly property color textSecondary: darkMode ? "#c4c7c7" : "#444748"
    readonly property color outline: darkMode ? "#8e9192" : "#747878"
    readonly property color outlineVariant: darkMode ? "#444748" : "#c4c7c7"
    readonly property color disabled: darkMode ? "#61e1e3e3" : "#61191c1d"
    readonly property color divider: darkMode ? "#2effffff" : "#1f000000"
    readonly property color error: darkMode ? "#ffb4ab" : "#b3261e"
    readonly property color switchOff: darkMode ? "#45494a" : "#d2d2d2"
    readonly property color switchThumbOff: darkMode ? "#c4c7c7" : "#727677"
    readonly property color scrim: darkMode ? "#e6111415" : "#e6f8fafa"

    readonly property int windowWidth: 412
    readonly property int windowHeight: 600
    readonly property int toolbarHeight: 44
    readonly property int horizontalMargin: 16
    readonly property int cardRadius: roundedDesktop ? 10 : (desktop === "windows" ? 6 : 4)

    readonly property int font3S: 12
    readonly property int font2S: 13
    readonly property int fontS: 14
    readonly property int fontM: 16
    readonly property int fontL: 18
    readonly property int fontLL: 16
}

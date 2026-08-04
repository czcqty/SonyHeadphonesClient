pragma Singleton
import QtQuick

QtObject {
    property bool darkMode: false

    readonly property color primary: darkMode ? "#b8c4ff" : "#364fa8"
    readonly property color primaryContainer: darkMode ? "#273a80" : "#c5d4ff"
    readonly property color primaryText: darkMode ? "#17265f" : "#ffffff"
    readonly property color surface: darkMode ? "#111415" : "#f8fafa"
    readonly property color surfaceContainer: darkMode ? "#1d2021" : "#eef1f1"
    readonly property color surfaceContainerHigh: darkMode ? "#242829" : "#f2f4f4"
    readonly property color surfaceContainerHighest: darkMode ? "#303435" : "#e1e3e3"
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
    readonly property int windowHeight: 780
    readonly property int toolbarHeight: 56
    readonly property int bigHeaderHeight: 402
    readonly property int horizontalMargin: 16
    readonly property int cardRadius: 13

    readonly property int font3S: 12
    readonly property int font2S: 13
    readonly property int fontS: 16
    readonly property int fontM: 18
    readonly property int fontL: 20
    readonly property int fontLL: 22
}

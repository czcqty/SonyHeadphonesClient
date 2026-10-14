pragma Singleton
import QtQuick

QtObject {
    property bool darkMode: appController.appearance.dark
    readonly property SystemPalette colors: SystemPalette { colorGroup: SystemPalette.Active }
    readonly property SystemPalette disabledColors: SystemPalette { colorGroup: SystemPalette.Disabled }
    readonly property color primary: colors.highlight
    readonly property color primaryContainer: colors.alternateBase
    readonly property color primaryText: colors.highlightedText
    readonly property color surface: colors.window
    readonly property color surfaceContainer: colors.base
    readonly property color surfaceContainerHigh: colors.alternateBase
    readonly property color surfaceContainerHighest: colors.button
    readonly property color textPrimary: colors.windowText
    readonly property color textSecondary: colors.text
    readonly property color outline: colors.mid
    readonly property color outlineVariant: colors.midlight
    readonly property color disabled: disabledColors.windowText
    readonly property color divider: colors.midlight
    readonly property color error: colors.text
    readonly property color scrim: colors.window
    readonly property int duration: 120
    readonly property int windowWidth: 412
    readonly property int windowHeight: 600
    readonly property int toolbarHeight: 44
    readonly property int horizontalMargin: 16
    readonly property int cardRadius: 0
    readonly property int controlHeight: 32
    readonly property string fontFamily: appController.appearance.systemFont.family
    readonly property int fontS: Math.max(13, Math.round(appController.appearance.systemFont.pointSize > 0 ? appController.appearance.systemFont.pointSize * 96 / 72 : 14))
    readonly property int font3S: Math.max(11, fontS - 2)
    readonly property int font2S: fontS
    readonly property int fontM: fontS + 2
    readonly property int fontL: fontS + 4
    readonly property int fontLL: fontS
}

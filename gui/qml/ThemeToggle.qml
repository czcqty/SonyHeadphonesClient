import QtQuick
import QtQuick.Controls.Basic

Button {
    id: control
    width: 48
    height: 48
    padding: 0
    text: appController.darkMode ? "☀" : "☾"
    Accessible.name: appController.darkMode
        ? I18n.text("theme.light") : I18n.text("theme.dark")
    ToolTip.visible: hovered
    ToolTip.text: Accessible.name
    onClicked: appController.toggleDarkMode()

    background: Rectangle {
        radius: width / 2
        color: control.down ? Tokens.primaryContainer : Tokens.surfaceContainerHighest
        border.width: 1
        border.color: Tokens.outlineVariant
    }

    contentItem: Text {
        text: control.text
        color: Tokens.textPrimary
        font.pixelSize: 22
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}

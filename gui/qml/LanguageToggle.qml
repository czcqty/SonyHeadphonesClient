import QtQuick
import QtQuick.Controls.Basic

Button {
    id: control
    width: 48
    height: 48
    padding: 0
    text: appController.locale === "zh_CN" ? "EN" : "中"
    Accessible.name: appController.locale === "zh_CN"
        ? I18n.text("language.english") : I18n.text("language.chinese")
    ToolTip.visible: hovered
    ToolTip.text: Accessible.name
    onClicked: appController.toggleLocale()

    background: Rectangle {
        radius: width / 2
        color: control.down ? Tokens.primaryContainer : Tokens.surfaceContainerHighest
        border.width: 1
        border.color: Tokens.outlineVariant
    }

    contentItem: Text {
        text: control.text
        color: Tokens.textPrimary
        font.pixelSize: 16
        font.bold: true
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}

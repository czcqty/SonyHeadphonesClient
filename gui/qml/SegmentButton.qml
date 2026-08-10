import QtQuick
import QtQuick.Controls

Button {
    id: control
    property bool selected: false

    implicitHeight: 48
    font.pixelSize: Tokens.font2S
    font.bold: selected
    activeFocusOnTab: true
    Accessible.role: Accessible.Button
    Accessible.name: text

    background: Rectangle {
        radius: 12
        color: control.selected ? Tokens.primary : Tokens.surfaceContainerHighest
        opacity: control.enabled ? 1 : 0.38
    }

    contentItem: Text {
        text: control.text
        color: control.selected ? Tokens.primaryText : Tokens.textPrimary
        font: control.font
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}

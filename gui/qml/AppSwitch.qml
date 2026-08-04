import QtQuick
import QtQuick.Controls

Switch {
    id: control
    implicitWidth: 52
    implicitHeight: 32
    padding: 0

    indicator: Rectangle {
        x: 2
        y: (control.height - height) / 2
        width: 48
        height: 28
        radius: 14
        color: control.enabled
            ? (control.checked ? Tokens.primaryContainer : Tokens.switchOff)
            : Tokens.switchOff

        Rectangle {
            x: control.checked ? parent.width - width - 4 : 4
            y: 4
            width: 20
            height: 20
            radius: 10
            color: control.enabled
                ? (control.checked ? Tokens.primary : Tokens.switchThumbOff)
                : Tokens.switchThumbOff
            Behavior on x { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
        }
    }

    contentItem: Item {}
}

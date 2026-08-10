import QtQuick
import QtQuick.Controls

Slider {
    id: control
    activeFocusOnTab: true
    Accessible.role: Accessible.Slider

    background: Rectangle {
        x: control.orientation === Qt.Horizontal
            ? control.leftPadding
            : control.leftPadding + (control.availableWidth - width) / 2
        y: control.orientation === Qt.Horizontal
            ? control.topPadding + (control.availableHeight - height) / 2
            : control.topPadding
        width: control.orientation === Qt.Horizontal ? control.availableWidth : 4
        height: control.orientation === Qt.Horizontal ? 4 : control.availableHeight
        radius: 2
        color: Tokens.outlineVariant

        Rectangle {
            x: 0
            y: control.orientation === Qt.Horizontal
                ? 0
                : parent.height * control.visualPosition
            width: control.orientation === Qt.Horizontal
                ? parent.width * control.visualPosition
                : parent.width
            height: control.orientation === Qt.Horizontal
                ? parent.height
                : parent.height * (1 - control.visualPosition)
            radius: 2
            color: Tokens.primary
        }
    }

    handle: Rectangle {
        x: control.orientation === Qt.Horizontal
            ? control.leftPadding + control.visualPosition * (control.availableWidth - width)
            : control.leftPadding + (control.availableWidth - width) / 2
        y: control.orientation === Qt.Horizontal
            ? control.topPadding + (control.availableHeight - height) / 2
            : control.topPadding + control.visualPosition * (control.availableHeight - height)
        implicitWidth: 20
        implicitHeight: 20
        radius: 10
        color: Tokens.primary
        border.color: Tokens.surface
        border.width: 2
    }
}

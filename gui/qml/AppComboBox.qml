pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic

ComboBox {
    id: control

    implicitHeight: 52
    leftPadding: 16
    rightPadding: 44
    font.pixelSize: Tokens.fontS
    activeFocusOnTab: true

    delegate: ItemDelegate {
        id: delegateItem
        required property int index
        width: control.width - 8
        height: 48
        leftPadding: 12
        rightPadding: 12
        text: control.textAt(index)
        highlighted: control.highlightedIndex === index

        background: Rectangle {
            radius: 10
            color: delegateItem.highlighted ? Tokens.primaryContainer : "transparent"
        }

        contentItem: Text {
            text: delegateItem.text
            color: Tokens.textPrimary
            font.pixelSize: Tokens.fontS
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
    }

    indicator: Text {
        x: control.width - width - 16
        anchors.verticalCenter: parent.verticalCenter
        text: "\u2304"
        color: control.enabled ? Tokens.textSecondary : Tokens.disabled
        font.pixelSize: Tokens.fontM
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    contentItem: Text {
        leftPadding: 0
        rightPadding: 0
        text: control.displayText
        color: control.enabled ? Tokens.textPrimary : Tokens.disabled
        font: control.font
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: Tokens.cardRadius
        color: control.down ? Tokens.surfaceContainerHighest : Tokens.surfaceContainer
        border.width: control.activeFocus ? 2 : 1
        border.color: control.activeFocus ? Tokens.primary : Tokens.outlineVariant
        opacity: control.enabled ? 1 : 0.6
    }

    popup: Popup {
        y: control.height + 4
        width: control.width
        implicitHeight: Math.min(contentItem.implicitHeight + 8, 280)
        padding: 4

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            ScrollIndicator.vertical: ScrollIndicator {}
        }

        background: Rectangle {
            radius: Tokens.cardRadius
            color: Tokens.surfaceContainerHigh
            border.width: 1
            border.color: Tokens.outlineVariant
        }
    }
}

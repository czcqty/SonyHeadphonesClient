import QtQuick
import QtQuick.Controls

Rectangle {
    id: root
    property string title
    property string subtitle
    property string route
    property bool available: true

    width: parent ? parent.width : 0
    height: 120
    radius: Tokens.cardRadius
    color: Tokens.surfaceContainer
    opacity: available ? 1 : 0.38
    activeFocusOnTab: available
    Accessible.role: Accessible.Button
    Accessible.name: title
    Accessible.description: subtitle
    Keys.onSpacePressed: appController.currentRoute = root.route
    Keys.onReturnPressed: appController.currentRoute = root.route

    TapHandler {
        enabled: root.available
        onTapped: appController.currentRoute = root.route
    }

    Column {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 18
        anchors.rightMargin: 18
        anchors.verticalCenter: parent.verticalCenter
        spacing: 10

        Text {
            width: parent.width
            text: root.title
            color: Tokens.textPrimary
            font.pixelSize: Tokens.fontS
            font.bold: true
            elide: Text.ElideRight
        }
        Text {
            width: parent.width
            text: root.subtitle
            color: Tokens.textSecondary
            font.pixelSize: Tokens.font2S
            wrapMode: Text.WordWrap
            maximumLineCount: 2
            elide: Text.ElideRight
        }
    }
}

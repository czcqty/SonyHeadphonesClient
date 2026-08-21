import QtQuick
import QtQuick.Controls.Basic

Rectangle {
    id: root
    property string title
    property bool backVisible: true
    signal backRequested()

    height: Tokens.toolbarHeight
    color: Tokens.surface

    Button {
        id: backButton
        visible: root.backVisible
        anchors.left: parent.left
        anchors.leftMargin: 4
        anchors.verticalCenter: parent.verticalCenter
        width: 64
        height: 48
        text: I18n.text("back")
        flat: true
        font.pixelSize: Tokens.font2S
        onClicked: root.backRequested()
        Accessible.name: I18n.text("back")
    }

    Text {
        anchors.left: backButton.visible ? backButton.right : parent.left
        anchors.right: parent.right
        anchors.leftMargin: backButton.visible ? 4 : 12
        anchors.rightMargin: backButton.visible ? 68 : 12
        anchors.verticalCenter: parent.verticalCenter
        text: root.title
        horizontalAlignment: Text.AlignHCenter
        color: Tokens.textPrimary
        font.pixelSize: Tokens.fontLL
        elide: Text.ElideRight
    }
}

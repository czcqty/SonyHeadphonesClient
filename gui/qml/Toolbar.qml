import QtQuick
import QtQuick.Controls

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
    }

    Text {
        anchors.centerIn: parent
        width: 264
        text: root.title
        horizontalAlignment: Text.AlignHCenter
        color: Tokens.textPrimary
        font.pixelSize: Tokens.fontLL
        elide: Text.ElideRight
    }
}

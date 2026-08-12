import QtQuick

Item {
    id: root
    property string title
    property string subtitle
    property bool switchVisible: false
    property bool checked: false
    property bool rowEnabled: true
    signal activated()
    signal toggled(bool checked)

    width: parent ? parent.width : 0
    height: subtitle.length > 0 ? 76 : 64
    opacity: rowEnabled ? 1 : 0.38
    activeFocusOnTab: rowEnabled
    Accessible.role: switchVisible ? Accessible.CheckBox : Accessible.Button
    Accessible.name: title
    Accessible.description: subtitle
    Accessible.checked: checked
    Keys.onSpacePressed: switchVisible ? toggled(!checked) : activated()
    Keys.onReturnPressed: switchVisible ? toggled(!checked) : activated()

    Column {
        anchors.left: parent.left
        anchors.right: root.switchVisible ? toggle.left : parent.right
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        anchors.verticalCenter: parent.verticalCenter
        spacing: 4

        Text {
            width: parent.width
            text: root.title
            color: Tokens.textPrimary
            font.pixelSize: Tokens.fontS
            elide: Text.ElideRight
        }
        Text {
            width: parent.width
            visible: root.subtitle.length > 0
            text: root.subtitle
            color: Tokens.textSecondary
            font.pixelSize: Tokens.font3S
            elide: Text.ElideRight
        }
    }

    MouseArea {
        anchors.left: parent.left
        anchors.right: root.switchVisible ? toggle.left : parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        enabled: root.rowEnabled
        cursorShape: Qt.PointingHandCursor
        onClicked: root.switchVisible ? root.toggled(!root.checked) : root.activated()
    }

    AppSwitch {
        id: toggle
        visible: root.switchVisible
        enabled: root.rowEnabled
        checked: root.checked
        anchors.right: parent.right
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        onToggled: root.toggled(checked)
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: Tokens.divider
    }
}

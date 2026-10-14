import QtQuick
import QtQuick.Controls

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
    height: Math.max(row.implicitHeight, toggle.implicitHeight)
    enabled: rowEnabled
    ItemDelegate {
        id: row
        anchors.fill: parent
        text: root.title + (root.subtitle.length ? "\n" + root.subtitle : "")
        rightPadding: root.switchVisible ? toggle.width + 24 : 12
        onClicked: root.switchVisible ? root.toggled(!root.checked) : root.activated()
    }
    AppSwitch {
        id: toggle
        visible: root.switchVisible
        checked: root.checked
        anchors.right: parent.right
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        Accessible.name: root.title
        onToggled: root.toggled(checked)
    }
}

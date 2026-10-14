import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ToolBar {
    id: root
    property string title
    property bool backVisible: true
    signal backRequested()
    height: Math.max(44, implicitHeight)
    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        ToolButton {
            visible: root.backVisible
            text: I18n.text("back")
            onClicked: root.backRequested()
        }
        Label {
            Layout.fillWidth: true
            text: root.title
            elide: Text.ElideRight
        }
    }
}

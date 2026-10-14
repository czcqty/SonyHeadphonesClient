import QtQuick
import QtQuick.Controls

ItemDelegate {
    property string title
    property string subtitle
    property string route
    property bool available: true
    width: parent ? parent.width : 0
    text: title + (subtitle.length ? "\n" + subtitle : "")
    enabled: available
    onClicked: appController.currentRoute = route
}

import QtQuick
import QtQuick.Controls

Button {
    property bool accent: false
    property bool destructive: false
    highlighted: accent
    Accessible.name: text
}

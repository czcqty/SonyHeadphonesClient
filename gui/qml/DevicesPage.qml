import QtQuick
import QtQuick.Controls

Page {
    background: Rectangle { color: Tokens.surface }

    Column {
        anchors.fill: parent

        Toolbar {
            width: parent.width
            title: I18n.text("devices")
            onBackRequested: appController.goBack()
        }

        Column {
            width: parent.width
            Repeater {
                model: appController.deviceManager.devices
                delegate: Rectangle {
                    required property var modelData
                    width: parent.width
                    height: 116
                    color: Tokens.surfaceContainer

                    Column {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 6
                        Text {
                            text: modelData.name.length > 0 ? modelData.name : modelData.mac
                            color: Tokens.textPrimary
                            font.pixelSize: Tokens.fontS
                            font.bold: modelData.playback
                        }
                        Text {
                            text: (modelData.connected ? I18n.text("connected") + " · " : "") + modelData.mac
                            color: Tokens.textSecondary
                            font.pixelSize: Tokens.font3S
                        }
                        Row {
                            spacing: 8
                            Button {
                                text: modelData.connected ? qsTr("Disconnect") : qsTr("Connect")
                                onClicked: modelData.connected
                                    ? appController.deviceManager.disconnectDevice(modelData.mac)
                                    : appController.deviceManager.connectDevice(modelData.mac)
                                Accessible.name: text + " " + modelData.name
                            }
                            Button {
                                visible: appController.deviceManager.sourceSwitchSupported
                                         && modelData.connected && !modelData.playback
                                text: qsTr("Use for playback")
                                onClicked: appController.deviceManager.switchDevice(modelData.mac)
                            }
                            Button {
                                text: qsTr("Unpair")
                                onClicked: appController.deviceManager.unpairDevice(modelData.mac)
                            }
                        }
                    }
                }
            }

            SettingRow {
                title: I18n.text("pairing")
                switchVisible: true
                checked: appController.deviceManager.pairingMode
                rowEnabled: appController.deviceManager.supported
                onToggled: checked => appController.deviceManager.setPairingMode(checked)
            }
        }
    }
}

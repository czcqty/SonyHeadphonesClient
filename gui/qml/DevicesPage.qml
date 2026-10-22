import QtQuick
import QtQuick.Controls

Page {
    background: Item {}

    Flickable {
        ScrollBar.vertical: AppScrollBar {}
        anchors.fill: parent
        contentWidth: width
        contentHeight: pageContent.height + 12
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        Column {
            id: pageContent
            width: parent.width

            Toolbar {
                width: parent.width
                title: I18n.text("devices")
                onBackRequested: appController.goBack()
            }

            Column {
                width: parent.width
                SettingRow {
                    visible: appController.settings.multipointSupported
                    title: I18n.text("multipoint.enabled")
                    switchVisible: true
                    checked: appController.settings.multipointEnabled
                    onToggled: checked => appController.settings.setMultipointEnabled(checked)
                }
                Repeater {
                    model: appController.deviceManager.devices
                    delegate: Rectangle {
                        required property var modelData
                        width: parent.width
                        height: deviceContent.implicitHeight + 24
                        color: Tokens.surfaceContainer

                        Column {
                            id: deviceContent
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: 12
                            spacing: 6
                            Text { font.family: Tokens.fontFamily;
                                text: modelData.name.length > 0 ? modelData.name : modelData.mac
                                color: Tokens.textPrimary
                                font.pixelSize: Tokens.fontS
                                font.bold: modelData.playback
                            }
                            Text { font.family: Tokens.fontFamily;
                                text: (modelData.connected ? I18n.text("connected") + " · " : "") + modelData.mac
                                color: Tokens.textSecondary
                                font.pixelSize: Tokens.font3S
                            }
                            Flow {
                                width: parent.width
                                spacing: 8
                                AppButton {
                                    text: modelData.connected ? I18n.text("disconnect") : I18n.text("connect")
                                    onClicked: modelData.connected
                                        ? appController.deviceManager.disconnectDevice(modelData.mac)
                                        : appController.deviceManager.connectDevice(modelData.mac)
                                    Accessible.name: text + " " + modelData.name
                                }
                                AppButton {
                                    visible: appController.deviceManager.sourceSwitchSupported
                                             && modelData.connected && !modelData.playback
                                    text: I18n.text("use.for.playback")
                                    onClicked: appController.deviceManager.switchDevice(modelData.mac)
                                }
                                AppButton {
                                    text: I18n.text("unpair")
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
}

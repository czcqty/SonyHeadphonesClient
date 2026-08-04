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
                delegate: SettingRow {
                    required property var modelData
                    title: I18n.text(modelData.nameId)
                    subtitle: modelData.connected
                        ? I18n.text("connected") + " · " + modelData.mac
                        : modelData.mac
                    onActivated: appController.deviceManager.switchDevice(modelData.mac)
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

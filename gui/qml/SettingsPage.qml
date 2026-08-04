import QtQuick
import QtQuick.Controls

Page {
    background: Rectangle { color: Tokens.surface }

    Column {
        anchors.fill: parent

        Toolbar {
            width: parent.width
            title: I18n.text("all.settings")
            onBackRequested: appController.goBack()
        }

        Flickable {
            width: parent.width
            height: parent.height - Tokens.toolbarHeight
            contentWidth: width
            contentHeight: settingsColumn.height + 32
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            Column {
                id: settingsColumn
                width: parent.width

                Text {
                    width: parent.width
                    height: 48
                    leftPadding: 16
                    verticalAlignment: Text.AlignVCenter
                    text: I18n.text("sound")
                    color: Tokens.primary
                    font.pixelSize: Tokens.font2S
                    font.bold: true
                }

                SettingRow {
                    visible: appController.noiseControl.supported
                    title: I18n.text("noise")
                    subtitle: appController.noiseControl.mode === 1
                        ? I18n.text("noise.canceling") : I18n.text("ambient")
                    onActivated: appController.currentRoute = "noise"
                }
                SettingRow {
                    visible: appController.equalizer.supported
                    title: I18n.text("equalizer")
                    subtitle: I18n.presetName(appController.equalizer.presetId)
                    onActivated: appController.currentRoute = "equalizer"
                }
                SettingRow {
                    visible: appController.settings.dseeSupported
                    title: I18n.text("dsee")
                    switchVisible: true
                    checked: appController.settings.dseeEnabled
                    onToggled: checked => appController.settings.setDseeEnabled(checked)
                }
                SettingRow {
                    visible: appController.settings.connectionQualitySupported
                    title: I18n.text("quality")
                    switchVisible: true
                    checked: appController.settings.soundQualityPrior
                    onToggled: checked => appController.settings.setSoundQualityPrior(checked)
                }
                SettingRow {
                    visible: appController.settings.listeningModeSupported
                    title: I18n.text("listening.mode")
                    switchVisible: true
                    checked: appController.settings.listeningModeEnabled
                    onToggled: checked => appController.settings.setListeningModeEnabled(checked)
                }
                SettingRow {
                    visible: appController.settings.speakToChatSupported
                    title: I18n.text("speak")
                    switchVisible: true
                    checked: appController.settings.speakToChatEnabled
                    onToggled: checked => appController.settings.setSpeakToChatEnabled(checked)
                }

                Text {
                    width: parent.width
                    height: 48
                    leftPadding: 16
                    verticalAlignment: Text.AlignVCenter
                    text: I18n.text("system")
                    color: Tokens.primary
                    font.pixelSize: Tokens.font2S
                    font.bold: true
                }

                SettingRow {
                    visible: appController.settings.autoPauseSupported
                    title: I18n.text("autopause")
                    switchVisible: true
                    checked: appController.settings.autoPauseEnabled
                    onToggled: checked => appController.settings.setAutoPauseEnabled(checked)
                }
                SettingRow {
                    visible: appController.settings.autoPowerOffSupported
                    title: I18n.text("auto.power")
                    switchVisible: true
                    checked: appController.settings.autoPowerOffEnabled
                    onToggled: checked => appController.settings.setAutoPowerOffEnabled(checked)
                }
                SettingRow {
                    visible: appController.settings.headGestureSupported
                    title: I18n.text("head.gesture")
                    switchVisible: true
                    checked: appController.settings.headGestureEnabled
                    onToggled: checked => appController.settings.setHeadGestureEnabled(checked)
                }
                SettingRow {
                    visible: appController.settings.touchSupported
                    title: I18n.text("touch")
                    onActivated: appController.currentRoute = "touch"
                }
                SettingRow {
                    visible: appController.settings.voiceGuidanceSupported
                    title: I18n.text("voice")
                    subtitle: appController.settings.voiceGuidanceEnabled
                        ? I18n.text("on") : I18n.text("off")
                    onActivated: appController.currentRoute = "voice"
                }
                SettingRow {
                    visible: appController.deviceManager.supported
                    title: I18n.text("devices")
                    onActivated: appController.currentRoute = "devices"
                }
            }
        }
    }
}

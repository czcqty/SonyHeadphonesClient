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
                    visible: appController.settings.listeningModeSupported
                    title: qsTr("Cinema mode")
                    switchVisible: true
                    checked: appController.settings.cinemaModeEnabled
                    onToggled: checked => appController.settings.setCinemaModeEnabled(checked)
                }
                ComboBox {
                    visible: appController.settings.listeningModeSupported
                             && appController.settings.listeningModeEnabled
                    width: parent.width - 32
                    x: 16
                    model: [qsTr("Small room"), qsTr("Medium room"), qsTr("Large room")]
                    currentIndex: appController.settings.listeningRoomSize
                    onActivated: index => appController.settings.setListeningRoomSize(index)
                    Accessible.name: qsTr("Listening mode room size")
                }
                SettingRow {
                    visible: appController.settings.speakToChatSupported
                    title: I18n.text("speak")
                    switchVisible: true
                    checked: appController.settings.speakToChatEnabled
                    onToggled: checked => appController.settings.setSpeakToChatEnabled(checked)
                }
                ComboBox {
                    visible: appController.settings.speakToChatSupported
                             && appController.settings.speakToChatEnabled
                    width: parent.width - 32
                    x: 16
                    model: [qsTr("Automatic sensitivity"), qsTr("High sensitivity"), qsTr("Low sensitivity")]
                    currentIndex: appController.settings.speakToChatSensitivity
                    onActivated: index => appController.settings.setSpeakToChatSensitivity(index)
                    Accessible.name: qsTr("Speak-to-Chat sensitivity")
                }
                ComboBox {
                    visible: appController.settings.speakToChatSupported
                             && appController.settings.speakToChatEnabled
                    width: parent.width - 32
                    x: 16
                    model: [qsTr("Fast close"), qsTr("Normal close"), qsTr("Slow close"), qsTr("Do not close")]
                    currentIndex: appController.settings.speakToChatModeOut
                    onActivated: index => appController.settings.setSpeakToChatModeOut(index)
                    Accessible.name: qsTr("Speak-to-Chat close time")
                }
                SettingRow {
                    visible: appController.settings.adaptiveVolumeSupported
                    title: qsTr("Adaptive volume")
                    switchVisible: true
                    checked: appController.settings.adaptiveVolumeEnabled
                    onToggled: checked => appController.settings.setAdaptiveVolumeEnabled(checked)
                }
                SettingRow {
                    visible: appController.settings.safeListeningSupported
                    title: qsTr("Safe Listening")
                    subtitle: qsTr("Reduce prolonged high-volume exposure")
                    switchVisible: true
                    checked: appController.settings.safeListeningEnabled
                    onToggled: checked => appController.settings.setSafeListeningEnabled(checked)
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
                }
                ComboBox {
                    visible: appController.settings.autoPowerOffSupported
                    width: parent.width - 32
                    x: 16
                    property var options: [
                        {name: qsTr("5 minutes"), value: 0x00},
                        {name: qsTr("15 minutes"), value: 0x04},
                        {name: qsTr("30 minutes"), value: 0x01},
                        {name: qsTr("60 minutes"), value: 0x02},
                        {name: qsTr("180 minutes"), value: 0x03},
                        {name: qsTr("When removed"), value: 0x10},
                        {name: I18n.text("off"), value: 0x11}
                    ]
                    model: options.filter(item => item.value !== 0x10
                                          || appController.settings.autoPowerOffWearingDetectionSupported)
                    textRole: "name"
                    function indexForValue() {
                        for (let index = 0; index < model.length; ++index)
                            if (model[index].value === appController.settings.autoPowerOffValue)
                                return index
                        return 0
                    }
                    currentIndex: indexForValue()
                    onActivated: index => appController.settings.setAutoPowerOffValue(model[index].value)
                    Accessible.name: I18n.text("auto.power")
                }
                SettingRow {
                    visible: appController.settings.headGestureSupported
                    title: I18n.text("head.gesture")
                    switchVisible: true
                    checked: appController.settings.headGestureEnabled
                    onToggled: checked => appController.settings.setHeadGestureEnabled(checked)
                }
                SettingRow {
                    visible: appController.settings.captureVoiceSupported
                    title: qsTr("Capture voice during calls")
                    switchVisible: true
                    checked: appController.settings.captureVoiceEnabled
                    onToggled: checked => appController.settings.setCaptureVoiceEnabled(checked)
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

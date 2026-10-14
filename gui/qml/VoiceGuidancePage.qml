import QtQuick
import QtQuick.Controls.Basic

Page {
    background: Rectangle { color: Tokens.surface }

    Flickable {
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
                title: I18n.text("voice")
                onBackRequested: appController.goBack()
            }

            SettingRow {
                    title: I18n.text("voice")
                switchVisible: true
                checked: appController.settings.voiceGuidanceEnabled
                onToggled: checked => appController.settings.setVoiceGuidanceEnabled(checked)
            }

            Item { width: 1; height: 24 }

            Column {
                visible: appController.settings.voiceGuidanceVolumeSupported
                x: 16
                width: parent.width - 32
                spacing: 14

                Text {
                        text: I18n.text("voice.volume")
                    color: Tokens.textPrimary
                    font.pixelSize: Tokens.fontS
                    font.bold: true
                }
                AppSlider {
                    width: parent.width
                    from: -2
                    to: 2
                    stepSize: 1
                    value: appController.settings.voiceGuidanceVolume
                    onMoved: appController.settings.setVoiceGuidanceVolume(Math.round(value))
                }
            }
        }
    }
}

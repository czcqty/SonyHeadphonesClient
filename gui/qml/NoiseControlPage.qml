import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

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
                title: I18n.text("noise")
                onBackRequested: appController.goBack()
            }

            Item { width: 1; height: 28 }

            Column {
                x: 16
                width: parent.width - 32
                spacing: 28

                RowLayout {
                    width: parent.width
                    spacing: 8

                    Repeater {
                        model: [
                            {label: I18n.text("off"), mode: 0, enabled: true},
                            {label: I18n.text("noise.canceling"), mode: 1, enabled: appController.noiseControl.supported},
                            {label: I18n.text("ambient"), mode: 2, enabled: appController.noiseControl.ambientSupported}
                        ]
                        delegate: SegmentButton {
                            required property var modelData
                            Layout.fillWidth: true
                            height: 48
                            enabled: modelData.enabled
                            text: modelData.label
                            selected: appController.noiseControl.mode === modelData.mode
                            onClicked: appController.noiseControl.setMode(modelData.mode)
                        }
                    }
                }

                Rectangle {
                    visible: appController.noiseControl.mode === 2
                    width: parent.width
                    height: 180
                    radius: Tokens.cardRadius
                    color: Tokens.surfaceContainer

                    Column {
                        anchors.fill: parent
                        anchors.margins: 18
                        spacing: 18

                        Text { font.family: Tokens.fontFamily;
                            text: I18n.text("ambient.level")
                            color: Tokens.textPrimary
                            font.pixelSize: Tokens.fontS
                            font.bold: true
                        }
                        AppSlider {
                            width: parent.width
                            from: 1
                            to: 20
                            stepSize: 1
                            value: appController.noiseControl.ambientLevel
                            onMoved: appController.noiseControl.setAmbientLevel(Math.round(value))
                        }
                        Row {
                            width: parent.width
                            spacing: 12
                            Text { font.family: Tokens.fontFamily;
                                width: parent.width - focusSwitch.width - 12
                                anchors.verticalCenter: parent.verticalCenter
                                text: I18n.text("focus.voice")
                                color: Tokens.textPrimary
                                font.pixelSize: Tokens.fontS
                            }
                            AppSwitch {
                                id: focusSwitch
                                checked: appController.noiseControl.focusOnVoice
                                onToggled: appController.noiseControl.setFocusOnVoice(checked)
                            }
                        }
                    }
                }

                Rectangle {
                    visible: appController.noiseControl.adaptiveSupported
                    width: parent.width
                    height: adaptiveColumn.implicitHeight + 36
                    radius: Tokens.cardRadius
                    color: Tokens.surfaceContainer

                    Column {
                        id: adaptiveColumn
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 18
                        spacing: 14

                        Row {
                            width: parent.width
                            Text { font.family: Tokens.fontFamily;
                                width: parent.width - adaptiveSwitch.width
                                text: I18n.text("adaptive.noise")
                                color: Tokens.textPrimary
                                font.pixelSize: Tokens.fontS
                                font.bold: true
                            }
                            AppSwitch {
                                id: adaptiveSwitch
                                checked: appController.noiseControl.adaptiveEnabled
                                onToggled: appController.noiseControl.setAdaptiveEnabled(checked)
                                Accessible.name: I18n.text("adaptive.noise")
                            }
                        }
                        AppComboBox {
                            width: parent.width
                            enabled: appController.noiseControl.adaptiveEnabled
                            model: [I18n.text("standard"), I18n.text("high"), I18n.text("low")]
                            currentIndex: appController.noiseControl.adaptiveSensitivity
                            onActivated: index => appController.noiseControl.setAdaptiveSensitivity(index)
                            Accessible.name: I18n.text("adaptive.sensitivity")
                        }
                    }
                }

                Text { font.family: Tokens.fontFamily;
                    visible: !appController.noiseControl.supported
                    width: parent.width
                    text: I18n.text("unavailable")
                    color: Tokens.textSecondary
                    font.pixelSize: Tokens.fontS
                    wrapMode: Text.WordWrap
                }
            }
        }
    }
}

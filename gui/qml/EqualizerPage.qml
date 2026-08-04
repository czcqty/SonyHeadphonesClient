import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    background: Rectangle { color: Tokens.surface }

    Column {
        anchors.fill: parent

        Toolbar {
            width: parent.width
            title: I18n.text("equalizer")
            onBackRequested: appController.goBack()
        }

        Item { width: 1; height: 20 }

        Column {
            x: 16
            width: parent.width - 32
            spacing: 18

            RowLayout {
                width: parent.width
                SegmentButton {
                    Layout.fillWidth: true
                    text: I18n.text("flat")
                    selected: appController.equalizer.presetId === "flat"
                    onClicked: appController.equalizer.selectPreset("flat")
                }
                SegmentButton {
                    Layout.fillWidth: true
                    text: I18n.text("custom")
                    selected: appController.equalizer.presetId !== "flat"
                    onClicked: appController.equalizer.selectPreset("custom")
                }
            }

            Rectangle {
                width: parent.width
                height: 360
                radius: Tokens.cardRadius
                color: Tokens.surfaceContainer

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 8

                    Repeater {
                        model: ["400", "1k", "2.5k", "6.3k", "16k"]
                        delegate: ColumnLayout {
                            required property int index
                            required property string modelData
                            Layout.fillWidth: true
                            Layout.fillHeight: true

                            Text {
                                Layout.alignment: Qt.AlignHCenter
                                text: appController.equalizer.bands[index]
                                color: Tokens.textPrimary
                                font.pixelSize: Tokens.font2S
                            }
                            AppSlider {
                                Layout.fillHeight: true
                                Layout.alignment: Qt.AlignHCenter
                                orientation: Qt.Vertical
                                from: -10
                                to: 10
                                value: appController.equalizer.bands[index]
                                onMoved: appController.equalizer.setBand(index, Math.round(value))
                            }
                            Text {
                                Layout.alignment: Qt.AlignHCenter
                                text: modelData
                                color: Tokens.textSecondary
                                font.pixelSize: Tokens.font3S
                            }
                        }
                    }
                }
            }

            Text {
                text: I18n.text("clear.bass")
                color: Tokens.textPrimary
                font.pixelSize: Tokens.font2S
                font.bold: true
            }
            AppSlider {
                width: parent.width
                from: -10
                to: 10
                stepSize: 1
                value: appController.equalizer.clearBass
                onMoved: appController.equalizer.setClearBass(Math.round(value))
            }
        }
    }
}

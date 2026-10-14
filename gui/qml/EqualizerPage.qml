import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

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
                title: I18n.text("equalizer")
                onBackRequested: appController.goBack()
            }

            Item { width: 1; height: 20 }

            Column {
                x: 16
                width: parent.width - 32
                spacing: 18

                AppComboBox {
                    width: parent.width
                    model: appController.equalizer.presetOptions.map(option => ({
                        id: option.id,
                        name: I18n.presetName(option.id)
                    }))
                    textRole: "name"
                    function selectedIndex() {
                        for (let index = 0; index < model.length; ++index)
                            if (model[index].id === appController.equalizer.presetId)
                                return index
                        return 0
                    }
                    currentIndex: selectedIndex()
                    onActivated: index => appController.equalizer.selectPreset(model[index].id)
                    Accessible.name: I18n.text("equalizer")
                }

                Rectangle {
                    width: parent.width
                    height: 220
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
}

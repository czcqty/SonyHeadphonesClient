import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    id: root
    background: Item {}

    Flickable {
        ScrollBar.vertical: AppScrollBar {}
        id: scroll
        anchors.fill: parent
        contentWidth: width
        contentHeight: content.height
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        Column {
            id: content
            width: scroll.width

            Item {
                width: parent.width
                height: 104

                Toolbar {
                    z: 2
                    x: 64
                    width: parent.width - 80
                    title: appController.modelName + " · " + I18n.text("connected")
                    backVisible: false
                }

                Image {
                    id: deviceImage
                    x: 16
                    y: 14
                    width: 40
                    height: 40
                    source: appController.deviceImage
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    mipmap: true
                }

                RowLayout {
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: 60
                    spacing: 24

                    Repeater {
                        model: [
                            {label: I18n.text("left"), value: appController.playback.batteryLeft, charging: appController.playback.batteryLeftCharging, show: true},
                            {label: I18n.text("right"), value: appController.playback.batteryRight, charging: appController.playback.batteryRightCharging, show: appController.playback.hasRightBattery},
                            {label: I18n.text("case"), value: appController.playback.batteryCase, charging: appController.playback.batteryCaseCharging, show: appController.playback.hasCaseBattery}
                        ]
                        delegate: Column {
                            required property var modelData
                            visible: modelData.show
                            spacing: 4
                            Text { font.family: Tokens.fontFamily;
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: modelData.label
                                color: Tokens.textSecondary
                                font.pixelSize: Tokens.font3S
                            }
                            Text { font.family: Tokens.fontFamily;
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: modelData.value + "%" + (modelData.charging ? " ⚡" : "")
                                color: Tokens.textPrimary
                                font.pixelSize: Tokens.fontS
                                font.bold: true
                            }
                        }
                    }
                }
            }

            Column {
                x: Tokens.horizontalMargin
                width: parent.width - Tokens.horizontalMargin * 2
                spacing: 12

                Rectangle {
                    width: parent.width
                    height: playbackContent.implicitHeight + 24
                    radius: Tokens.cardRadius
                    color: Tokens.surfaceContainer

                    Column {
                        id: playbackContent
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 12
                        spacing: 12

                        Text { font.family: Tokens.fontFamily;
                            width: parent.width
                            text: I18n.text("playback")
                            color: Tokens.textPrimary
                            font.pixelSize: Tokens.fontS
                            font.bold: true
                        }
                        Text { font.family: Tokens.fontFamily;
                            width: parent.width
                            visible: appController.playback.title.length > 0
                            text: appController.playback.title
                            color: Tokens.textPrimary
                            font.pixelSize: Tokens.fontS
                            font.bold: true
                            elide: Text.ElideRight
                        }
                        Text { font.family: Tokens.fontFamily;
                            width: parent.width
                            visible: appController.playback.artist.length > 0
                            text: appController.playback.artist
                            color: Tokens.textSecondary
                            font.pixelSize: Tokens.font2S
                            elide: Text.ElideRight
                        }
                        Text { font.family: Tokens.fontFamily;
                            visible: appController.playback.album.length > 0
                            width: parent.width
                            text: appController.playback.album
                            color: Tokens.textSecondary
                            font.pixelSize: Tokens.font3S
                            elide: Text.ElideRight
                        }

                        RowLayout {
                            visible: appController.playback.playbackSupported
                            width: parent.width
                            spacing: 8
                            AppButton {
                                Layout.fillWidth: true
                                text: I18n.text("previous")
                                onClicked: appController.playback.previousTrack()
                            }
                            AppButton {
                                Layout.fillWidth: true
                                text: I18n.text("play.pause")
                                onClicked: appController.playback.togglePlayback()
                            }
                            AppButton {
                                Layout.fillWidth: true
                                text: I18n.text("next")
                                onClicked: appController.playback.nextTrack()
                            }
                        }

                        Text { font.family: Tokens.fontFamily;
                            visible: appController.playback.volumeSupported
                            text: I18n.text("volume")
                            color: Tokens.textPrimary
                            font.pixelSize: Tokens.font2S
                            font.bold: true
                        }
                        AppSlider {
                            visible: appController.playback.volumeSupported
                            width: parent.width
                            from: 0
                            to: 30
                            stepSize: 1
                            value: appController.playback.volume
                            onMoved: appController.playback.setVolume(Math.round(value))
                        }


                    }
                }

                Column {
                    width: parent.width
                    spacing: 8
                    visible: appController.noiseControl.supported || appController.noiseControl.ambientSupported
                    Text { font.family: Tokens.fontFamily; text: I18n.text("noise"); color: Tokens.textPrimary; font.bold: true }
                    RowLayout {
                        width: parent.width
                        Repeater {
                            model: [
                                {label: I18n.text("off"), mode: 0, available: true},
                                {label: I18n.text("noise.canceling"), mode: 1, available: appController.noiseControl.supported},
                                {label: I18n.text("ambient"), mode: 2, available: appController.noiseControl.ambientSupported}
                            ]
                            delegate: SegmentButton {
                                required property var modelData
                                Layout.fillWidth: true
                                text: modelData.label
                                enabled: modelData.available
                                selected: appController.noiseControl.mode === modelData.mode
                                onClicked: appController.noiseControl.setMode(modelData.mode)
                            }
                        }
                    }
                    AppSlider {
                        width: parent.width
                        visible: appController.noiseControl.mode === 2
                        from: 1; to: 20; stepSize: 1
                        value: appController.noiseControl.ambientLevel
                        Accessible.name: I18n.text("ambient.level")
                        onMoved: appController.noiseControl.setAmbientLevel(Math.round(value))
                    }
                    ShortcutCard { title: I18n.text("ambient.control"); route: "noise" }
                }
                ShortcutCard {
                    visible: appController.equalizer.supported
                    title: I18n.text("equalizer")
                    subtitle: I18n.presetName(appController.equalizer.presetId)
                    route: "equalizer"
                }
                ShortcutCard { title: I18n.text("sound"); route: "sound" }
                ShortcutCard { title: I18n.text("behavior"); route: "behavior" }
                Rectangle {
                    visible: appController.playback.safeListeningSupported
                    width: parent.width
                    height: 76
                    radius: Tokens.cardRadius
                    color: Tokens.surfaceContainer
                    Text { font.family: Tokens.fontFamily;
                        anchors.left: parent.left
                        anchors.leftMargin: 18
                        anchors.verticalCenter: parent.verticalCenter
                        text: I18n.text("safe.listening") + "\n" + appController.playback.safeListeningPressure + " dB"
                        color: Tokens.textPrimary
                        font.pixelSize: Tokens.fontS
                    }
                    Accessible.role: Accessible.StaticText
                    Accessible.name: I18n.text("safe.listening.pressure")
                    Accessible.description: appController.playback.safeListeningPressure + " dB"
                }

                Text { font.family: Tokens.fontFamily;
                    width: parent.width
                    text: [appController.playback.codec,
                           appController.playback.firmware.length > 0
                               ? I18n.text("firmware") + " " + appController.playback.firmware : ""]
                          .filter(value => value.length > 0).join(" · ")
                    color: Tokens.textSecondary
                    font.pixelSize: Tokens.font3S
                    horizontalAlignment: Text.AlignHCenter
                }

                Text { font.family: Tokens.fontFamily;
                    visible: appController.playback.alertMessage.length > 0
                    width: parent.width
                    text: appController.playback.alertMessage
                                color: powerOffButton.enabled ? Tokens.error : Tokens.disabled
                    font.pixelSize: Tokens.font3S
                    wrapMode: Text.WordWrap
                    Accessible.role: Accessible.AlertMessage
                }

                ShortcutCard {
                    visible: appController.deviceManager.supported
                    title: I18n.text("devices")
                    subtitle: I18n.connectedDevices(appController.deviceManager.devices.length)
                    route: "devices"
                }

                        AppButton {
                            id: powerOffButton
                            visible: appController.playback.powerOffSupported
                            enabled: !appController.playback.powerOffPending
                            width: parent.width
                            height: 48
                            text: appController.playback.powerOffPending
                                ? I18n.text("powering.off") : I18n.text("power.off")
                            onClicked: powerOffDialog.open()

                            destructive: true

                        }
                Item { width: 1; height: 12 }
            }
        }
    }

    Dialog {
        id: powerOffDialog
        anchors.centerIn: parent
        width: Math.min(parent.width - 48, 360)
        modal: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        padding: 20

        background: Rectangle {
            radius: Tokens.cardRadius
            color: Tokens.surfaceContainerHigh
            border.width: 1
            border.color: Tokens.outlineVariant
        }

        contentItem: Column {
            spacing: 12
            Text { font.family: Tokens.fontFamily;
                width: parent.width
                text: I18n.text("power.off.confirm.title")
                color: Tokens.textPrimary
                font.pixelSize: Tokens.fontM
                font.bold: true
                wrapMode: Text.WordWrap
            }
            Text { font.family: Tokens.fontFamily;
                width: parent.width
                text: I18n.text("power.off.confirm.description")
                color: Tokens.textSecondary
                font.pixelSize: Tokens.font2S
                wrapMode: Text.WordWrap
            }
            Row {
                anchors.right: parent.right
                spacing: 8
                AppButton {
                    text: I18n.text("cancel")
                    onClicked: powerOffDialog.close()
                }
                AppButton {
                    text: I18n.text("power.off.action")
                    onClicked: {
                        powerOffDialog.close()
                        appController.playback.powerOff()
                    }
                }
            }
        }
    }
}

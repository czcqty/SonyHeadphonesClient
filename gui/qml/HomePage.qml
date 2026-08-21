import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Page {
    id: root
    background: Rectangle { color: Tokens.surface }

    Flickable {
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
                height: Tokens.bigHeaderHeight

                Toolbar {
                    z: 2
                    width: parent.width
                    title: appController.modelName
                    backVisible: false
                }

                Image {
                    id: deviceImage
                    x: (parent.width - width) / 2
                    y: 72
                    width: 236
                    height: 236
                    source: appController.deviceImage
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    mipmap: true
                }

                RowLayout {
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: 315
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
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: modelData.label
                                color: Tokens.textSecondary
                                font.pixelSize: Tokens.font3S
                            }
                            Text {
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
                    visible: appController.playback.playbackSupported
                    width: parent.width
                    height: 56
                    radius: Tokens.cardRadius
                    color: Tokens.surfaceContainer

                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 16
                        anchors.verticalCenter: parent.verticalCenter
                        text: I18n.text("all.settings")
                        color: Tokens.textSecondary
                        font.pixelSize: Tokens.font2S
                        font.bold: true
                    }
                    TapHandler { onTapped: appController.currentRoute = "settings" }
                }

                Text {
                    topPadding: 12
                    bottomPadding: 0
                    text: I18n.text("shortcuts")
                    color: Tokens.textPrimary
                    font.pixelSize: Tokens.fontM
                    font.bold: true
                }

                ShortcutCard {
                    visible: appController.noiseControl.supported
                    title: I18n.text("noise")
                    subtitle: appController.noiseControl.mode === 1
                        ? I18n.text("noise.canceling")
                        : (appController.noiseControl.mode === 2
                           ? I18n.text("ambient") + " " + appController.noiseControl.ambientLevel
                           : I18n.text("off"))
                    route: "noise"
                }

                ShortcutCard {
                    visible: appController.equalizer.supported
                    title: I18n.text("equalizer")
                    subtitle: I18n.presetName(appController.equalizer.presetId)
                    route: "equalizer"
                }

                Rectangle {
                    width: parent.width
                    height: playbackContent.implicitHeight + 36
                    radius: Tokens.cardRadius
                    color: Tokens.surfaceContainer

                    Column {
                        id: playbackContent
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 18
                        spacing: 12

                        Text {
                            width: parent.width
                            text: I18n.text("playback")
                            color: Tokens.textPrimary
                            font.pixelSize: Tokens.fontS
                            font.bold: true
                        }
                        Text {
                            width: parent.width
                            text: appController.playback.title
                            color: Tokens.textPrimary
                            font.pixelSize: Tokens.fontM
                            font.bold: true
                            elide: Text.ElideRight
                        }
                        Text {
                            width: parent.width
                            text: appController.playback.artist
                            color: Tokens.textSecondary
                            font.pixelSize: Tokens.font2S
                            elide: Text.ElideRight
                        }
                        Text {
                            visible: appController.playback.album.length > 0
                            width: parent.width
                            text: appController.playback.album
                            color: Tokens.textSecondary
                            font.pixelSize: Tokens.font3S
                            elide: Text.ElideRight
                        }

                        RowLayout {
                            width: parent.width
                            spacing: 8
                            Button {
                                Layout.fillWidth: true
                                text: I18n.text("previous")
                                onClicked: appController.playback.previousTrack()
                            }
                            Button {
                                Layout.fillWidth: true
                                text: I18n.text("play.pause")
                                onClicked: appController.playback.togglePlayback()
                            }
                            Button {
                                Layout.fillWidth: true
                                text: I18n.text("next")
                                onClicked: appController.playback.nextTrack()
                            }
                        }

                        Text {
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

                        Button {
                            id: powerOffButton
                            visible: appController.playback.powerOffSupported
                            enabled: !appController.playback.powerOffPending
                            width: parent.width
                            height: 48
                            text: appController.playback.powerOffPending
                                ? I18n.text("powering.off") : I18n.text("power.off")
                            onClicked: powerOffDialog.open()

                            background: Rectangle {
                                radius: Tokens.cardRadius
                                color: "transparent"
                                border.width: 1
                                border.color: Tokens.error
                            }

                            contentItem: Text {
                                text: powerOffButton.text
                                color: powerOffButton.enabled ? Tokens.error : Tokens.disabled
                                font.pixelSize: Tokens.fontS
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                        }
                    }
                }

                Rectangle {
                    visible: appController.playback.safeListeningSupported
                    width: parent.width
                    height: 76
                    radius: Tokens.cardRadius
                    color: Tokens.surfaceContainer
                    Text {
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

                Text {
                    width: parent.width
                    text: [appController.playback.codec,
                           appController.playback.firmware.length > 0
                               ? I18n.text("firmware") + " " + appController.playback.firmware : ""]
                          .filter(value => value.length > 0).join(" · ")
                    color: Tokens.textSecondary
                    font.pixelSize: Tokens.font3S
                    horizontalAlignment: Text.AlignHCenter
                }

                Text {
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

                Item { width: 1; height: 88 }
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
            Text {
                width: parent.width
                text: I18n.text("power.off.confirm.title")
                color: Tokens.textPrimary
                font.pixelSize: Tokens.fontM
                font.bold: true
                wrapMode: Text.WordWrap
            }
            Text {
                width: parent.width
                text: I18n.text("power.off.confirm.description")
                color: Tokens.textSecondary
                font.pixelSize: Tokens.font2S
                wrapMode: Text.WordWrap
            }
            Row {
                anchors.right: parent.right
                spacing: 8
                Button {
                    text: I18n.text("cancel")
                    onClicked: powerOffDialog.close()
                }
                Button {
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

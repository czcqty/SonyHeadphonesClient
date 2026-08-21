import QtQuick
import QtQuick.Controls.Basic

Page {
    background: Rectangle { color: Tokens.surface }

    Column {
        anchors.fill: parent
        Toolbar {
            width: parent.width
            title: "SonyHeadphonesClient"
            backVisible: false
        }

        Item {
            width: parent.width
            height: parent.height - Tokens.toolbarHeight

            Column {
                visible: appController.connection.state !== "scanning"
                    && appController.connection.state !== "connecting"
                    && appController.connection.state !== "initializing"
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 24
                spacing: 12

                Repeater {
                    model: appController.connection.devices
                    delegate: Rectangle {
                        required property var modelData
                        width: parent.width
                        height: 72
                        radius: Tokens.cardRadius
                        color: Tokens.surfaceContainer

                        Column {
                            anchors.left: parent.left
                            anchors.leftMargin: 16
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 4
                            Text {
                                text: modelData.name
                                color: Tokens.textPrimary
                                font.pixelSize: Tokens.fontS
                            }
                            Text {
                                text: modelData.mac
                                color: Tokens.textSecondary
                                font.pixelSize: Tokens.font3S
                            }
                        }
                        Button {
                            anchors.right: parent.right
                            anchors.rightMargin: 8
                            anchors.verticalCenter: parent.verticalCenter
                            text: I18n.text("connect")
                            flat: true
                            onClicked: appController.connectDevice(modelData.name, modelData.mac)
                        }
                    }
                }

                Button {
                    width: parent.width
                    height: 48
                    text: I18n.text("scan")
                    onClicked: appController.refreshDevices()
                }
            }

            Column {
                visible: appController.connection.state === "scanning"
                anchors.centerIn: parent
                spacing: 12
                BusyIndicator {
                    anchors.horizontalCenter: parent.horizontalCenter
                    running: parent.visible
                }
                Text {
                    text: I18n.text("scanning")
                    color: Tokens.textPrimary
                    font.pixelSize: Tokens.fontS
                }
            }

            Text {
                visible: appController.connection.errorCode.length > 0
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: 24
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                text: I18n.errorMessage(appController.connection.errorCode,
                                        appController.connection.errorDetail)
                color: Tokens.error
                font.pixelSize: Tokens.font2S
            }
        }
    }
}

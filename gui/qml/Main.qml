import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: window
    signal dismissRequested()
    Shortcut {
        sequence: "Escape"
        enabled: window.visible && (!window.Overlay.overlay || !window.Overlay.overlay.children.some(child => child.visible))
        onActivated: window.dismissRequested()
    }
    width: Tokens.windowWidth
    height: Tokens.windowHeight
    minimumWidth: 360
    minimumHeight: 480
    visible: appController.previewMode
    title: "SonyHeadphonesClient"
    font: appController.appearance.systemFont

    Loader {
        objectName: "panelRouteLoader"
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        width: Math.min(parent.width, 720)
        sourceComponent: {
            switch (appController.currentRoute) {
            case "connection": return connectionPage
            case "settings": return soundPage
            case "sound": return soundPage
            case "behavior": return behaviorPage
            case "touch": return touchPage
            case "voice": return voicePage
            case "noise": return noisePage
            case "equalizer": return equalizerPage
            case "devices": return devicesPage
            default: return homePage
            }
        }
    }

    Component { id: connectionPage; ConnectionPage {} }
    Component { id: homePage; HomePage {} }
    Component { id: soundPage; DeviceOptionsPage { soundOptions: true } }
    Component { id: behaviorPage; DeviceOptionsPage { soundOptions: false } }
    Component { id: touchPage; TouchAssignmentsPage {} }
    Component { id: voicePage; VoiceGuidancePage {} }
    Component { id: noisePage; NoiseControlPage {} }
    Component { id: equalizerPage; EqualizerPage {} }
    Component { id: devicesPage; DevicesPage {} }

    Rectangle {
        anchors.fill: parent
        visible: appController.previewState !== "ready"
        color: Tokens.scrim
        z: 100

        Column {
            anchors.centerIn: parent
            width: parent.width - 64
            spacing: 18

            AppBusyIndicator {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: appController.previewState === "loading"
                running: visible
            }
            Text { font.family: Tokens.fontFamily;
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                color: Tokens.textPrimary
                font.pixelSize: Tokens.fontS
                text: {
                    switch (appController.previewState) {
                    case "loading": return I18n.text("loading")
                    case "empty": return I18n.text("empty")
                    case "error": return appController.connection.errorCode.length > 0
                        ? I18n.errorMessage(appController.connection.errorCode,
                                            appController.connection.errorDetail)
                        : I18n.text("unavailable")
                    case "disabled": return I18n.text("unavailable")
                    default: return ""
                    }
                }
            }
            AppButton {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: appController.previewState === "empty" || appController.previewState === "error"
                text: I18n.text("retry")
                onClicked: appController.previewState = "ready"
            }
        }
    }

    Rectangle {
        visible: appController.profileReferenceBlocked && appController.previewMode
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 28
        color: Tokens.primaryContainer
        z: 101
        Text { font.family: Tokens.fontFamily;
            anchors.centerIn: parent
            text: I18n.text("reference.blocked")
            color: Tokens.textPrimary
            font.pixelSize: Tokens.font3S
        }
    }

    ConnectionOverlay {
        anchors.fill: parent
        z: 150
    }

    footer: ToolBar {
        height: 48
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            LanguageToggle { Layout.preferredWidth: 36; Layout.preferredHeight: Tokens.controlHeight }
            ThemeToggle { Layout.preferredWidth: 116; Layout.preferredHeight: Tokens.controlHeight }
            Item { Layout.fillWidth: true }
            AppButton {
                text: I18n.text("disconnect")
                visible: appController.connection.state === "connected"
                leftPadding: 6; rightPadding: 6
                implicitWidth: implicitContentWidth + 12
                flat: true
                onClicked: appController.disconnectDevice()
            }
            AppButton {
                text: I18n.text("quit")
                leftPadding: 6; rightPadding: 6
                implicitWidth: implicitContentWidth + 12
                flat: true
                onClicked: Qt.quit()
            }
        }
    }
}

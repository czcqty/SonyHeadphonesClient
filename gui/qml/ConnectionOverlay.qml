import QtQuick
import QtQuick.Controls

Item {
    id: root
    anchors.fill: parent

    property bool transitionRunning: false
    readonly property string connectionState: appController.connection.state
    readonly property bool busy: connectionState === "connecting"
        || connectionState === "initializing"

    visible: busy || transitionRunning
    opacity: 1

    Rectangle {
        anchors.fill: parent
        color: Tokens.surface
    }

    Image {
        id: hero
        x: (root.width - width) / 2
        y: Math.max(32, (root.height - 260) / 2)
        width: 160
        height: 160
        source: appController.connection.selectedDeviceImage
        fillMode: Image.PreserveAspectFit
        smooth: true
        mipmap: true
    }

    Text { font.family: Tokens.fontFamily;
        id: statusText
        anchors.top: hero.bottom
        anchors.topMargin: 18
        anchors.horizontalCenter: parent.horizontalCenter
        color: Tokens.textPrimary
        font.pixelSize: Tokens.fontS
        font.bold: true
            text: I18n.text("connecting")
    }

    AppBusyIndicator {
        anchors.top: statusText.bottom
        anchors.topMargin: 12
        anchors.horizontalCenter: parent.horizontalCenter
        running: root.busy
        visible: running && !root.transitionRunning
    }

    MouseArea { anchors.fill: parent }

    function resetVisuals() {
        root.opacity = 1
        hero.x = (root.width - 160) / 2
        hero.y = Math.max(32, (root.height - 260) / 2)
        hero.width = 160
        hero.height = 160
        statusText.opacity = 1
    }

    function handleState() {
        if (connectionState === "connected" && !transitionRunning) {
            if (appController.currentRoute === "connection") {
                transitionRunning = true
                appController.completeConnectionTransition()
                initialTransition.start()
            }
            return
        }

        if (connectionState === "error" || connectionState === "disconnected") {
            initialTransition.stop()
            transitionRunning = false
            resetVisuals()
        }
    }

    Connections {
        target: appController.connection
        function onChanged() { root.handleState() }
    }

    SequentialAnimation {
        id: initialTransition
        NumberAnimation {
            target: statusText
            property: "opacity"
            from: 1
            to: 0
            duration: 180
            easing.type: Easing.OutCubic
        }
        ParallelAnimation {
            NumberAnimation { target: hero; property: "x"; to: 16; duration: Tokens.reducedMotion ? 0 : 420; easing.type: Easing.OutCubic }
            NumberAnimation { target: hero; property: "y"; to: 14; duration: Tokens.reducedMotion ? 0 : 420; easing.type: Easing.OutCubic }
            NumberAnimation { target: hero; property: "width"; to: 40; duration: Tokens.reducedMotion ? 0 : 420; easing.type: Easing.OutCubic }
            NumberAnimation { target: hero; property: "height"; to: 40; duration: Tokens.reducedMotion ? 0 : 420; easing.type: Easing.OutCubic }
        }
        NumberAnimation { target: root; property: "opacity"; from: 1; to: 0; duration: Tokens.duration }
        ScriptAction {
            script: {
                root.transitionRunning = false
                root.resetVisuals()
            }
        }
    }
}

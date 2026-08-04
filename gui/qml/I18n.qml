pragma Singleton
import QtQuick

QtObject {
    function text(key) {
        switch (key) {
        case "all.settings": return qsTr("All device settings")
        case "shortcuts": return qsTr("Shortcuts")
        case "edit": return qsTr("Edit")
        case "noise": return qsTr("Ambient Sound Control")
        case "noise.canceling": return qsTr("Noise Canceling")
        case "ambient": return qsTr("Ambient Sound")
        case "off": return qsTr("Off")
        case "on": return qsTr("On")
        case "equalizer": return qsTr("Equalizer")
        case "custom": return qsTr("Custom")
        case "playback": return qsTr("Now playing")
        case "previous": return qsTr("Previous")
        case "play.pause": return qsTr("Play / Pause")
        case "next": return qsTr("Next")
        case "power.off": return qsTr("Power off headphones")
        case "volume": return qsTr("Volume")
        case "sound": return qsTr("Sound")
        case "system": return qsTr("System")
        case "devices": return qsTr("Manage connected devices")
        case "dsee": return qsTr("DSEE Extreme")
        case "speak": return qsTr("Speak-to-Chat")
        case "autopause": return qsTr("Pause when headphones are removed")
        case "touch": return qsTr("Change touch sensor function")
        case "ambient.control": return qsTr("Ambient Sound Control")
        case "volume.control": return qsTr("Volume Control")
        case "playback.control": return qsTr("Playback Control")
        case "track.control": return qsTr("Track Control")
        case "no.function": return qsTr("No function")
        case "nc.button": return qsTr("Noise Canceling button cycle")
        case "nc.amb.off": return qsTr("Noise Canceling / Ambient Sound / Off")
        case "nc.amb": return qsTr("Noise Canceling / Ambient Sound")
        case "nc.off": return qsTr("Noise Canceling / Off")
        case "amb.off": return qsTr("Ambient Sound / Off")
        case "voice": return qsTr("Notification & Voice Guide")
        case "voice.volume": return qsTr("Voice guidance volume")
        case "quality": return qsTr("Prioritize sound quality")
        case "auto.power": return qsTr("Automatic power off")
        case "head.gesture": return qsTr("Head gesture")
        case "listening.mode": return qsTr("Background Music Effect")
        case "pairing": return qsTr("Pairing mode")
        case "connected": return qsTr("Connected")
        case "connect": return qsTr("Connect")
        case "scan": return qsTr("Scan for headphones")
        case "scanning": return qsTr("Scanning for headphones…")
        case "connecting": return qsTr("Connecting…")
        case "theme.light": return qsTr("Switch to light mode")
        case "theme.dark": return qsTr("Switch to dark mode")
        case "language.english": return qsTr("Switch to English")
        case "language.chinese": return qsTr("Switch to Simplified Chinese")
        case "empty": return qsTr("No headphones found")
        case "retry": return qsTr("Try again")
        case "loading": return qsTr("Loading device settings…")
        case "unavailable": return qsTr("This feature is not available for this device.")
        case "reference.blocked": return qsTr("Reference state pending capture")
        case "back": return qsTr("Back")
        case "left": return qsTr("L")
        case "right": return qsTr("R")
        case "case": return qsTr("Case")
        case "focus.voice": return qsTr("Focus on Voice")
        case "ambient.level": return qsTr("Ambient sound level")
        case "clear.bass": return qsTr("CLEAR BASS")
        case "flat": return qsTr("Flat")
        case "laptop": return qsTr("Laptop")
        case "phone": return qsTr("Phone")
        default: return key
        }
    }

    function presetName(presetId) {
        return presetId === "flat" ? text("flat") : text("custom")
    }

    function connectedDevices(count) {
        return qsTr("%n device(s) connected", "", count)
    }

    function errorMessage(code, detail) {
        switch (code) {
        case "bluetooth-backend-unavailable":
            return qsTr("No Bluetooth backend available")
        case "bluetooth-scan-failed":
            return detail.length > 0 ? qsTr("Bluetooth scan failed: %1").arg(detail)
                                     : qsTr("Bluetooth scan failed")
        case "connection-timeout":
            return qsTr("Connection timed out")
        case "connection-failed":
            return detail.length > 0 ? qsTr("Connection failed: %1").arg(detail)
                                     : qsTr("Connection failed")
        case "initialization-failed":
            return qsTr("Unable to initialize the headphones")
        case "initialization-timeout":
            return qsTr("Headphone initialization timed out")
        case "protocol-error":
            return qsTr("Protocol error: %1").arg(detail)
        case "unexpected-error":
            return qsTr("Unexpected error: %1").arg(detail)
        default:
            return detail.length > 0 ? detail : text("unavailable")
        }
    }
}

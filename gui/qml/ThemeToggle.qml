import QtQuick

AppComboBox {
    id: control
    width: 116
    height: Tokens.controlHeight
    implicitWidth: 116
    model: [I18n.text("theme.system"), I18n.text("theme.light"), I18n.text("theme.dark")]
    function syncSelection() {
        currentIndex = ["system", "light", "dark"].indexOf(appController.appearance.themeMode)
    }
    Component.onCompleted: syncSelection()
    onModelChanged: Qt.callLater(syncSelection)
    Connections {
        target: appController.appearance
        function onChanged() { control.syncSelection() }
    }
    Accessible.name: I18n.text("theme.choose")
    onActivated: index => appController.appearance.themeMode = ["system", "light", "dark"][index]
}

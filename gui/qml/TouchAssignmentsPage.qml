import QtQuick
import QtQuick.Controls.Basic

Page {
    background: Rectangle { color: Tokens.surface }

    property var assignmentOptions: appController.settings.touchOptions
    property var ncButtonOptions: [
        {text: I18n.text("nc.amb.off"), value: 0x01},
        {text: I18n.text("nc.amb"), value: 0x02},
        {text: I18n.text("nc.off"), value: 0x03},
        {text: I18n.text("amb.off"), value: 0x04}
    ]

    function indexFor(options, value) {
        for (let index = 0; index < options.length; ++index) {
            if (options[index].value === value)
                return index
        }
        return 0
    }

    Column {
        anchors.fill: parent

        Toolbar {
            width: parent.width
            title: I18n.text("touch")
            onBackRequested: appController.goBack()
        }

        Item { width: 1; height: 28 }

        Column {
            x: 16
            width: parent.width - 32
            spacing: 24

            Text {
                text: I18n.text("left")
                color: Tokens.textPrimary
                font.pixelSize: Tokens.fontS
                font.bold: true
            }
            AppComboBox {
                width: parent.width
                model: assignmentOptions
                textRole: "name"
                currentIndex: indexFor(assignmentOptions, appController.settings.touchLeft)
                onActivated: index => appController.settings.setTouchLeft(assignmentOptions[index].value)
            }

            Text {
                text: I18n.text("right")
                color: Tokens.textPrimary
                font.pixelSize: Tokens.fontS
                font.bold: true
            }
            AppComboBox {
                width: parent.width
                model: assignmentOptions
                textRole: "name"
                currentIndex: indexFor(assignmentOptions, appController.settings.touchRight)
                onActivated: index => appController.settings.setTouchRight(assignmentOptions[index].value)
            }

            Text {
                visible: appController.settings.ncButtonSupported
                text: I18n.text("nc.button")
                color: Tokens.textPrimary
                font.pixelSize: Tokens.fontS
                font.bold: true
            }
            AppComboBox {
                visible: appController.settings.ncButtonSupported
                width: parent.width
                model: ncButtonOptions
                textRole: "text"
                currentIndex: indexFor(ncButtonOptions, appController.settings.ncButtonFunction)
                onActivated: index => appController.settings.setNcButtonFunction(ncButtonOptions[index].value)
            }
        }
    }
}

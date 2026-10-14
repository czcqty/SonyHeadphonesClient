import QtQuick

AppButton {
    width: 48
    height: Tokens.controlHeight
    implicitWidth: 36
    leftPadding: 4
    rightPadding: 4
    text: appController.locale === "zh_CN" ? "EN" : "中"
    Accessible.name: appController.locale === "zh_CN" ? I18n.text("language.english") : I18n.text("language.chinese")
    onClicked: appController.toggleLocale()
}

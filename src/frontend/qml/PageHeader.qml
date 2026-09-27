import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TypingTrainerModule

/// Заголовок вложенной страницы с кнопкой «Назад».
RowLayout {
    id: root

    property string title
    default property alias actions: actionsRow.data

    signal back()

    Layout.fillWidth: true
    spacing: 12

    XButton {
        text: "← Назад"
        onClicked: root.back()
    }

    Label {
        Layout.fillWidth: true
        text: root.title
        color: Theme.text
        font.pixelSize: 24
        font.weight: Font.DemiBold
    }

    Row {
        id: actionsRow
        spacing: 8
    }
}

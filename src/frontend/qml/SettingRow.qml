import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TypingTrainerModule

/// Строка настройки: название и пояснение слева, элемент управления справа.
RowLayout {
    id: root

    property string title
    property string description: ""
    default property alias control: slot.data

    Layout.fillWidth: true
    spacing: 16

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 2

        Label {
            Layout.fillWidth: true
            text: root.title
            color: Theme.text
            font.pixelSize: 15
            wrapMode: Text.WordWrap
        }

        Label {
            Layout.fillWidth: true
            visible: root.description.length > 0
            text: root.description
            color: Theme.textMuted
            font.pixelSize: 13
            wrapMode: Text.WordWrap
        }
    }

    Row {
        id: slot
        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
        spacing: 10
    }
}

import QtQuick
import QtQuick.Controls
import TypingTrainerModule

/// Подсказка о клавише: «клавиша» в рамке и пояснение.
Row {
    id: root

    property string key
    property string text

    spacing: 6

    Rectangle {
        anchors.verticalCenter: parent.verticalCenter
        visible: root.key.length > 0
        implicitWidth: keyLabel.implicitWidth + 12
        implicitHeight: 22
        radius: 5
        color: Theme.surface
        border.color: Theme.border

        Label {
            id: keyLabel
            anchors.centerIn: parent
            text: root.key
            color: Theme.text
            font.family: Theme.monoFamily
            font.pixelSize: 12
        }
    }

    Label {
        anchors.verticalCenter: parent.verticalCenter
        text: root.text
        color: Theme.textMuted
        font.pixelSize: 13
    }
}

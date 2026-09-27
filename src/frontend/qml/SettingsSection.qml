import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TypingTrainerModule

/// Карточка с заголовком и группой настроек.
Rectangle {
    id: root

    property string title
    default property alias content: body.data

    implicitHeight: column.implicitHeight + 40
    radius: Theme.radius
    color: Theme.surface
    border.color: Theme.border

    ColumnLayout {
        id: column
        anchors.fill: parent
        anchors.margins: 20
        spacing: 14

        Label {
            text: root.title
            color: Theme.text
            font.pixelSize: 17
            font.weight: Font.DemiBold
        }

        ColumnLayout {
            id: body
            Layout.fillWidth: true
            spacing: 14
        }
    }
}

import QtQuick
import QtQuick.Controls
import TypingTrainerModule

/// Плитка с одной метрикой: подпись, крупное значение и единица измерения.
Rectangle {
    id: root

    property string label
    property string value
    property string unit: ""
    /// Цвет значения (например, для выделения рекорда).
    property color valueColor: Theme.text

    implicitWidth: 140
    implicitHeight: 74
    radius: Theme.radius
    color: Theme.surface
    border.color: Theme.border

    Column {
        anchors.left: parent.left
        anchors.leftMargin: 16
        anchors.verticalCenter: parent.verticalCenter
        spacing: 2

        Label {
            text: root.label
            color: Theme.textMuted
            font.pixelSize: 13
        }

        Row {
            spacing: 5

            Label {
                id: valueLabel
                text: root.value
                color: root.valueColor
                font.pixelSize: 26
                font.weight: Font.DemiBold
            }

            Label {
                anchors.baseline: valueLabel.baseline
                text: root.unit
                visible: text.length > 0 && root.value !== "—"
                color: Theme.textMuted
                font.pixelSize: 13
            }
        }
    }
}

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import TypingTrainerModule

/// Переключатель из нескольких сегментов с подвижным ползунком.
/// Сам currentIndex не меняет: сообщает выбор через activated, а владелец
/// обновляет состояние, к которому привязан currentIndex.
Control {
    id: root

    /// Подписи сегментов.
    property var model: []
    property int currentIndex: 0

    /// Пользователь выбрал сегмент.
    signal activated(int index)

    readonly property Item activeItem: repeater.count > 0 && currentIndex >= 0
                                       && currentIndex < repeater.count ? repeater.itemAt(currentIndex)
                                                                        : null

    implicitWidth: row.implicitWidth + leftPadding + rightPadding
    implicitHeight: 36
    padding: 3
    focusPolicy: Qt.NoFocus
    opacity: enabled ? 1.0 : 0.5

    background: Rectangle {
        radius: height / 2
        color: Theme.surfaceAlt
    }

    contentItem: Item {
        implicitWidth: row.implicitWidth
        implicitHeight: row.implicitHeight

        Rectangle {
            x: root.activeItem ? root.activeItem.x : 0
            width: root.activeItem ? root.activeItem.width : 0
            height: parent.height
            radius: height / 2
            color: Theme.surface
            border.color: Theme.border

            Behavior on x {
                NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
            }
            Behavior on width {
                NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
            }
        }

        Row {
            id: row
            height: parent.height

            Repeater {
                id: repeater
                model: root.model

                delegate: MouseArea {
                    id: segment

                    required property var modelData
                    required property int index
                    readonly property bool selected: root.currentIndex === index

                    width: label.implicitWidth + 28
                    height: row.height
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor

                    onClicked: if (!selected) root.activated(index)

                    Text {
                        id: label
                        anchors.centerIn: parent
                        text: segment.modelData
                        font.pixelSize: 14
                        font.weight: segment.selected ? Font.DemiBold : Font.Normal
                        color: segment.selected || segment.containsMouse ? Theme.text : Theme.textMuted

                        Behavior on color {
                            ColorAnimation { duration: 120 }
                        }
                    }
                }
            }
        }
    }
}

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TypingTrainerModule

/// Итоги завершённой тренировки поверх текста.
Rectangle {
    id: root

    /// Итог из Trainer.lastResult.
    property var result: ({})
    /// Подпись кнопки следующего действия (по Enter).
    property string nextText: "Новый текст"

    signal repeatRequested()
    signal nextRequested()

    readonly property bool personalBest: result.personalBest === true
    readonly property var weakest: result.weakest || []

    function formatTime(seconds) {
        const total = Math.round(seconds || 0)
        const minutes = Math.floor(total / 60)
        const rest = total % 60
        return minutes + ":" + (rest < 10 ? "0" : "") + rest
    }

    radius: Theme.radius
    color: Qt.rgba(Theme.surface.r, Theme.surface.g, Theme.surface.b, 0.97)
    border.color: Theme.border

    // Клики не должны проваливаться к тексту под панелью.
    MouseArea {
        anchors.fill: parent
    }

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - 64, 640)
        spacing: 16

        ColumnLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 4

            Label {
                Layout.alignment: Qt.AlignHCenter
                text: root.personalBest ? "Новый рекорд!" : "Готово!"
                color: root.personalBest ? Theme.success : Theme.text
                font.pixelSize: 26
                font.weight: Font.DemiBold
            }

            Label {
                Layout.alignment: Qt.AlignHCenter
                text: (root.result.length || 0) + " символов за " + root.formatTime(root.result.duration)
                color: Theme.textMuted
                font.pixelSize: 15
            }
        }

        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 48

            Repeater {
                model: [
                    { value: (root.result.wpm || 0).toFixed(0), label: "слов в минуту" },
                    { value: (root.result.accuracy || 0).toFixed(1) + "%", label: "точность" }
                ]

                delegate: ColumnLayout {
                    required property var modelData
                    spacing: 0

                    Label {
                        Layout.alignment: Qt.AlignHCenter
                        text: parent.modelData.value
                        color: Theme.text
                        font.pixelSize: 46
                        font.weight: Font.Bold
                    }
                    Label {
                        Layout.alignment: Qt.AlignHCenter
                        text: parent.modelData.label
                        color: Theme.textMuted
                        font.pixelSize: 14
                    }
                }
            }
        }

        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 32

            Repeater {
                model: [
                    { value: (root.result.cpm || 0).toFixed(0), label: "знаков в минуту" },
                    { value: root.result.consistency > 0 ? root.result.consistency.toFixed(0) + "%" : "—",
                      label: "ритм" },
                    { value: String(root.result.errors || 0), label: "ошибок" }
                ]

                delegate: ColumnLayout {
                    required property var modelData
                    spacing: 0

                    Label {
                        Layout.alignment: Qt.AlignHCenter
                        text: parent.modelData.value
                        color: Theme.text
                        font.pixelSize: 22
                        font.weight: Font.DemiBold
                    }
                    Label {
                        Layout.alignment: Qt.AlignHCenter
                        text: parent.modelData.label
                        color: Theme.textMuted
                        font.pixelSize: 13
                    }
                }
            }
        }

        Column {
            Layout.alignment: Qt.AlignHCenter
            spacing: 10

            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: root.weakest.length > 0 ? "Над чем поработать" : "Заметных заминок не было — отличная работа"
                color: Theme.textMuted
                font.pixelSize: 14
            }

            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 8
                visible: root.weakest.length > 0

                Repeater {
                    model: root.weakest

                    delegate: Rectangle {
                        id: chip

                        required property var modelData

                        implicitWidth: Math.max(chipColumn.implicitWidth + 24, 84)
                        implicitHeight: chipColumn.implicitHeight + 16
                        radius: Theme.smallRadius
                        color: Theme.surfaceAlt

                        Column {
                            id: chipColumn
                            anchors.centerIn: parent
                            spacing: 2

                            Label {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: chip.modelData.gram
                                color: Theme.text
                                font.family: Theme.monoFamily
                                font.pixelSize: 20
                                font.bold: true
                            }

                            Label {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: Math.round(chip.modelData.avgTime) + " мс"
                                color: Theme.textMuted
                                font.pixelSize: 12
                            }

                            Label {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: Math.round(chip.modelData.errorRate * 100) + "% ошибок"
                                color: chip.modelData.errorRate > 0.05 ? Theme.wrong : Theme.textMuted
                                font.pixelSize: 12
                            }
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 10

            XButton {
                text: "Ещё раз  ·  Tab"
                onClicked: root.repeatRequested()
            }

            XButton {
                text: root.nextText + "  ·  Enter"
                highlighted: true
                onClicked: root.nextRequested()
            }
        }

        Label {
            Layout.alignment: Qt.AlignHCenter
            text: "Esc — посмотреть набранный текст"
            color: Theme.textMuted
            font.pixelSize: 13
        }
    }
}

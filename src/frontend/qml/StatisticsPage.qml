pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TypingTrainerModule

/// Накопленная статистика: итоги, скорость по тренировкам и слабые сочетания.
FocusScope {
    id: page

    signal back()

    /// Язык, по которому показываются слабые сочетания.
    property string ngramLanguage: Trainer.language

    readonly property var history: Trainer.history
    readonly property var recent: history.slice(Math.max(0, history.length - 10))
    readonly property var weakNgrams: Trainer.weakNgrams.filter(item => item.language === ngramLanguage)
                                                        .slice(0, 16)

    function average(items, key) {
        if (items.length === 0) return 0
        let sum = 0
        for (const item of items) sum += item[key]
        return sum / items.length
    }

    function best(items, key) {
        let max = 0
        for (const item of items) max = Math.max(max, item[key])
        return max
    }

    function formatDuration(seconds) {
        if (seconds < 60) return "< 1 мин"
        const minutes = Math.round(seconds / 60)
        if (minutes < 60) return minutes + " мин"
        return Math.floor(minutes / 60) + " ч " + (minutes % 60) + " мин"
    }

    objectName: "statisticsPage"
    focus: true
    Keys.onEscapePressed: page.back()
    StackView.onActivated: {
        Trainer.requestStatistics()
        page.forceActiveFocus()
    }

    ScrollView {
        id: scroll
        anchors.fill: parent
        contentWidth: availableWidth

        ColumnLayout {
            width: Math.min(scroll.availableWidth - 56, 960)
            x: (scroll.availableWidth - width) / 2
            spacing: 16

            Item {
                implicitHeight: 12
            }

            PageHeader {
                title: "Статистика"
                onBack: page.back()

                XButton {
                    text: "Сбросить…"
                    enabled: page.history.length > 0 || Trainer.weakNgrams.length > 0
                    onClicked: resetDialog.open()
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                MetricTile {
                    Layout.fillWidth: true
                    label: "Тренировок"
                    value: String(page.history.length)
                }
                MetricTile {
                    Layout.fillWidth: true
                    label: "Лучшая скорость"
                    value: page.history.length > 0 ? page.best(page.history, "wpm").toFixed(0) : "—"
                    unit: "WPM"
                }
                MetricTile {
                    Layout.fillWidth: true
                    label: "Средняя скорость"
                    value: page.recent.length > 0 ? page.average(page.recent, "wpm").toFixed(0) : "—"
                    unit: "WPM"
                }
                MetricTile {
                    Layout.fillWidth: true
                    label: "Средняя точность"
                    value: page.recent.length > 0 ? page.average(page.recent, "accuracy").toFixed(1) : "—"
                    unit: "%"
                }
                MetricTile {
                    Layout.fillWidth: true
                    label: "Время практики"
                    value: page.history.length > 0
                           ? page.formatDuration(page.average(page.history, "duration") * page.history.length)
                           : "—"
                }
            }

            Label {
                Layout.topMargin: -8
                text: "Средние значения — по последним 10 тренировкам."
                color: Theme.textMuted
                font.pixelSize: 12
            }

            SettingsSection {
                Layout.fillWidth: true
                title: "Скорость по тренировкам"

                WpmChart {
                    Layout.fillWidth: true
                    implicitHeight: 240
                    visible: page.history.length > 0
                    sessions: page.history
                }

                Label {
                    Layout.fillWidth: true
                    visible: page.history.length === 0
                    text: "Здесь появится график, когда вы завершите первую тренировку."
                    color: Theme.textMuted
                    font.pixelSize: 14
                    wrapMode: Text.WordWrap
                }
            }

            SettingsSection {
                Layout.fillWidth: true
                title: "Слабые сочетания клавиш"

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 16

                    Label {
                        Layout.fillWidth: true
                        text: "Чем выше сочетание в списке, тем чаще умный режим включает его в тексты. "
                              + "Учитываются в основном последние нажатия, так что прогресс виден быстро."
                        color: Theme.textMuted
                        font.pixelSize: 13
                        wrapMode: Text.WordWrap
                    }

                    XSegmentedControl {
                        model: ["Русский", "English"]
                        currentIndex: page.ngramLanguage === "ru" ? 0 : 1
                        onActivated: (index) => page.ngramLanguage = index === 0 ? "ru" : "en"
                    }
                }

                GridLayout {
                    Layout.fillWidth: true
                    visible: page.weakNgrams.length > 0
                    columns: Math.max(2, Math.floor(width / 170))
                    columnSpacing: 10
                    rowSpacing: 10

                    Repeater {
                        model: page.weakNgrams

                        delegate: Rectangle {
                            id: card

                            required property var modelData
                            required property int index

                            Layout.fillWidth: true
                            implicitHeight: 64
                            radius: Theme.smallRadius
                            color: Theme.surfaceAlt

                            Label {
                                anchors.left: parent.left
                                anchors.leftMargin: 14
                                anchors.verticalCenter: parent.verticalCenter
                                text: card.modelData.gram
                                color: Theme.text
                                font.family: Theme.monoFamily
                                font.pixelSize: 22
                                font.bold: true
                            }

                            Column {
                                anchors.right: parent.right
                                anchors.rightMargin: 14
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 1

                                Label {
                                    anchors.right: parent.right
                                    text: Math.round(card.modelData.avgTime) + " мс"
                                    color: Theme.text
                                    font.pixelSize: 13
                                }
                                Label {
                                    anchors.right: parent.right
                                    text: Math.round(card.modelData.errorRate * 100) + "% ошибок"
                                    color: card.modelData.errorRate > 0.05 ? Theme.wrong : Theme.textMuted
                                    font.pixelSize: 12
                                }
                                Label {
                                    anchors.right: parent.right
                                    text: card.modelData.attempts + " раз"
                                    color: Theme.textMuted
                                    font.pixelSize: 12
                                }
                            }
                        }
                    }
                }

                Label {
                    Layout.fillWidth: true
                    visible: page.weakNgrams.length === 0
                    text: "Пока мало данных. Сочетание попадает сюда, когда встретится хотя бы 5 раз."
                    color: Theme.textMuted
                    font.pixelSize: 14
                    wrapMode: Text.WordWrap
                }
            }

            Item {
                implicitHeight: 16
            }
        }
    }

    Dialog {
        id: resetDialog

        anchors.centerIn: parent
        modal: true
        title: "Сбросить статистику?"

        ColumnLayout {
            spacing: 18

            Label {
                Layout.preferredWidth: 380
                text: "История тренировок и статистика сочетаний будут удалены. "
                      + "Умный режим начнёт подбирать тексты с нуля."
                wrapMode: Text.WordWrap
                color: Theme.textMuted
            }

            RowLayout {
                Layout.alignment: Qt.AlignRight
                spacing: 8

                XButton {
                    text: "Отмена"
                    onClicked: resetDialog.close()
                }

                XButton {
                    text: "Сбросить"
                    highlighted: true
                    onClicked: {
                        Trainer.resetStatistics()
                        resetDialog.close()
                    }
                }
            }
        }
    }
}

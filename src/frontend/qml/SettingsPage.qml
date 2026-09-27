import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TypingTrainerModule

/// Настройки оформления, Smart-режима и набора. Всё сохраняется сразу.
FocusScope {
    id: page

    signal back()

    readonly property var themeIds: ["system", "light", "dark", "black"]

    objectName: "settingsPage"
    focus: true
    Keys.onEscapePressed: page.back()
    StackView.onActivated: page.forceActiveFocus()

    ScrollView {
        id: scroll
        anchors.fill: parent
        contentWidth: availableWidth

        ColumnLayout {
            width: Math.min(scroll.availableWidth - 56, 720)
            x: (scroll.availableWidth - width) / 2
            spacing: 16

            Item {
                implicitHeight: 12
            }

            PageHeader {
                title: "Настройки"
                onBack: page.back()
            }

            SettingsSection {
                Layout.fillWidth: true
                title: "Оформление"

                SettingRow {
                    title: "Тема"

                    XSegmentedControl {
                        model: ["Как в системе", "Светлая", "Тёмная", "Чёрная"]
                        currentIndex: Math.max(0, page.themeIds.indexOf(Theme.currentTheme))
                        onActivated: (index) => Theme.currentTheme = page.themeIds[index]
                    }
                }

                SettingRow {
                    title: "Размер текста"
                    description: "Шрифт набираемого текста"

                    Slider {
                        id: fontSlider
                        width: 200
                        from: 18
                        to: 40
                        stepSize: 2
                        snapMode: Slider.SnapAlways
                        focusPolicy: Qt.NoFocus
                        value: Theme.typingFontSize
                        onMoved: Theme.typingFontSize = value
                    }

                    Label {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 48
                        text: Theme.typingFontSize + " px"
                        color: Theme.textMuted
                        font.pixelSize: 14
                    }
                }
            }

            SettingsSection {
                Layout.fillWidth: true
                title: "Умный режим"

                SettingRow {
                    title: "Сложность"
                    description: "Доля слов с вашими проблемными сочетаниями клавиш. "
                                 + "Остальные слова — обычные, для естественного ритма."

                    Slider {
                        width: 200
                        from: 0
                        to: 1
                        stepSize: 0.05
                        snapMode: Slider.SnapAlways
                        focusPolicy: Qt.NoFocus
                        value: Trainer.difficulty
                        onMoved: Trainer.difficulty = value
                    }

                    Label {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 48
                        text: Math.round(Trainer.difficulty * 100) + "%"
                        color: Theme.textMuted
                        font.pixelSize: 14
                    }
                }

                SettingRow {
                    title: "Длина текста"
                    description: "Сколько символов в одной тренировке"

                    SpinBox {
                        from: 50
                        to: 1000
                        stepSize: 50
                        editable: true
                        value: Trainer.targetLength
                        onValueModified: Trainer.targetLength = value
                    }
                }
            }

            SettingsSection {
                Layout.fillWidth: true
                title: "Набор"

                SettingRow {
                    title: "Не различать заглавные и строчные"
                    description: "Удобно в начале обучения: Shift можно не нажимать"

                    Switch {
                        focusPolicy: Qt.NoFocus
                        checked: Trainer.ignoreCase
                        onToggled: Trainer.ignoreCase = checked
                    }
                }

                SettingRow {
                    title: "Автопауза"
                    description: "Пауза после 5 секунд без нажатий и при переключении на другое окно"

                    Switch {
                        focusPolicy: Qt.NoFocus
                        checked: Trainer.autoPause
                        onToggled: Trainer.autoPause = checked
                    }
                }
            }

            SettingsSection {
                Layout.fillWidth: true
                title: "О программе"

                SettingRow {
                    title: "Typing Trainer " + Application.version
                    description: "Тренажёр слепой печати, который подстраивает упражнения "
                                 + "под ваши слабые сочетания клавиш. Лицензия MIT."

                    XButton {
                        text: "GitHub"
                        onClicked: Qt.openUrlExternally("https://github.com/TihonSotnikov/TypingTrainer")
                    }
                }

                SettingRow {
                    title: "Данные"
                    description: "Статистика и история хранятся локально: " + Trainer.dataLocation

                    XButton {
                        text: "Открыть папку"
                        onClicked: Qt.openUrlExternally(Trainer.dataLocationUrl)
                    }
                }
            }

            Item {
                implicitHeight: 16
            }
        }
    }
}

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TypingTrainerModule

/// Редактор своего текста для свободного режима.
/// Область фокуса: закрывая редактор, владелец снимает с неё фокус целиком.
FocusScope {
    id: root

    /// Пользователь сохранил текст.
    signal saved(string text)
    /// Пользователь отказался от правок.
    signal cancelled()

    /// Подготовить редактор к открытию.
    function open(text) {
        editor.text = text
        editor.forceActiveFocus()
        editor.cursorPosition = editor.length
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.surface
        radius: Theme.radius
        border.color: Theme.border
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 12

        Label {
            Layout.fillWidth: true
            text: "Вставьте или напишите текст для тренировки. Переносы строк, «ёлочки», длинные тире "
                  + "и многоточия будут заменены на то, что есть на клавиатуре."
            wrapMode: Text.WordWrap
            color: Theme.textMuted
            font.pixelSize: 13
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            TextArea {
                id: editor
                textFormat: TextEdit.PlainText
                wrapMode: TextEdit.Wrap
                selectByMouse: true
                color: Theme.text
                font.family: Theme.monoFamily
                font.pixelSize: 17
                // Текст вровень с пояснением над ним: рамки у поля нет.
                leftPadding: 0
                rightPadding: 0
                topPadding: 0
                bottomPadding: 0
                // Рамку рисует панель. Пустой фон, а не null: Material в Qt 6.5 обращается к нему.
                background: Rectangle {
                    color: "transparent"
                }

                Keys.onEscapePressed: root.cancelled()

                // Своя подсказка вместо placeholderText: подсказка Material при фокусе
                // всплывает на рамку поля, а без рамки ложится поверх текста.
                Label {
                    x: editor.leftPadding
                    y: editor.topPadding
                    visible: editor.length === 0 && editor.preeditText.length === 0
                    text: "Например, отрывок из любимой книги"
                    color: Theme.textMuted
                    font: editor.font
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                Layout.fillWidth: true
                text: editor.length + " символов"
                color: Theme.textMuted
                font.pixelSize: 13
            }

            XButton {
                text: "Отмена"
                onClicked: root.cancelled()
            }

            XButton {
                text: "Сохранить"
                highlighted: true
                enabled: editor.text.trim().length > 0
                onClicked: root.saved(editor.text)
            }
        }
    }
}

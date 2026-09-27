import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TypingTrainerModule

/// Редактор своего текста для свободного режима.
Rectangle {
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

    color: Theme.surface
    radius: Theme.radius
    border.color: Theme.border

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
                placeholderText: "Например, отрывок из любимой книги"
                color: Theme.text
                font.family: Theme.monoFamily
                font.pixelSize: 17
                // Рамку рисует панель. Пустой фон, а не null: Material в Qt 6.5 обращается к нему.
                background: Rectangle {
                    color: "transparent"
                }

                Keys.onEscapePressed: root.cancelled()
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

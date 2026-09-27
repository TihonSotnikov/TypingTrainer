import QtQuick
import QtQuick.Controls
import TypingTrainerModule

/// Панель с набираемым текстом: подсветка символов, курсор и автопрокрутка.
Rectangle {
    id: root

    /// Подсказка, когда текста нет.
    property string placeholder: ""

    readonly property bool sessionRunning: Trainer.status === Trainer.Active
                                           || Trainer.status === Trainer.Paused

    color: Theme.surface
    radius: Theme.radius
    border.color: Theme.border

    FontMetrics {
        id: metrics
        font: display.font
    }

    Flickable {
        id: flick

        anchors.fill: parent
        anchors.margins: 28
        clip: true
        interactive: false
        contentWidth: width
        contentHeight: display.contentHeight

        // Строка с курсором держится в верхней трети панели: видно, что впереди.
        contentY: {
            if (display.length === 0 || contentHeight <= height) return 0
            const target = display.cursorRectangle.y - height / 3
            return Math.max(0, Math.min(target, contentHeight - height))
        }

        Behavior on contentY {
            NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
        }

        TextEdit {
            id: display

            width: flick.width
            textFormat: TextEdit.RichText
            text: Trainer.formattedText
            readOnly: true
            selectByMouse: false
            activeFocusOnPress: false
            cursorVisible: false
            wrapMode: TextEdit.Wrap
            color: Theme.pending
            font.family: Theme.monoFamily
            font.pixelSize: Theme.typingFontSize
            // Длинный текст показывается страницами: позиция - относительно начала страницы.
            cursorPosition: Math.max(0, Math.min(Trainer.cursorPosition - Trainer.displayOffset, length))
        }

        Rectangle {
            id: caret

            visible: root.sessionRunning && Trainer.cursorPosition < Trainer.textLength
            x: display.cursorRectangle.x
            y: display.cursorRectangle.y
            width: metrics.averageCharacterWidth
            height: display.cursorRectangle.height
            radius: 4
            color: Theme.accent
            opacity: 0.28

            Behavior on x {
                NumberAnimation { duration: 70; easing.type: Easing.OutQuad }
            }
            Behavior on y {
                NumberAnimation { duration: 120; easing.type: Easing.InOutQuad }
            }
        }
    }

    Label {
        anchors.centerIn: parent
        width: parent.width - 80
        visible: Trainer.textLength === 0 && root.placeholder.length > 0
        text: root.placeholder
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        color: Theme.textMuted
        font.pixelSize: 16
    }
}

import QtQuick
import QtQuick.Controls
import TypingTrainerModule

/// Кнопка приложения. Не забирает фокус клавиатуры: набор текста не прерывается кликом.
/// Оформление своё, без эффектов стиля: одинаково выглядит на любом графическом бэкенде.
Button {
    id: control

    focusPolicy: Qt.NoFocus
    hoverEnabled: true
    implicitHeight: 38
    leftPadding: 16
    rightPadding: 16
    font.pixelSize: 14
    font.weight: Font.Medium

    contentItem: Text {
        text: control.text
        font: control.font
        color: control.highlighted ? Theme.accentText : Theme.text
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        implicitWidth: 64
        radius: 10
        opacity: control.enabled ? 1.0 : 0.45
        color: {
            if (control.highlighted) {
                if (control.down) return Qt.darker(Theme.accent, 1.15)
                return control.hovered ? Qt.lighter(Theme.accent, 1.08) : Theme.accent
            }
            if (control.down) return Theme.border
            return control.hovered ? Theme.surfaceAlt : "transparent"
        }

        Behavior on color {
            ColorAnimation { duration: 100 }
        }
    }

    HoverHandler {
        cursorShape: Qt.PointingHandCursor
    }
}

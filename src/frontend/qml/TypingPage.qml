import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TypingTrainerModule

/// Главный экран: выбор режима, метрики, текст и управление с клавиатуры.
FocusScope {
    id: page

    signal openStatistics()
    signal openSettings()

    /// Открыт редактор своего текста.
    property bool editing: false
    /// Итоги тренировки скрыты клавишей Esc - виден набранный текст.
    property bool resultsDismissed: false

    readonly property bool active: Trainer.status === Trainer.Active
    readonly property bool paused: Trainer.status === Trainer.Paused
    readonly property bool completed: Trainer.status === Trainer.Completed

    /// Начать тренировку на новом тексте.
    function newText() {
        Trainer.start()
    }

    /// Начать заново на том же тексте.
    function restartText() {
        Trainer.restart()
    }

    function setSmartMode(smart) {
        Trainer.smartMode = smart
        newText()
    }

    function setLanguage(code) {
        Trainer.language = code
        newText()
    }

    function startEditing() {
        Trainer.stop()
        editing = true
        editor.open(Trainer.customText)
    }

    function finishEditing(text) {
        if (text !== undefined) Trainer.customText = text
        editing = false
        page.forceActiveFocus()
        newText()
    }

    /// Вернулись из настроек: если набор ещё не начат, текст подбирается заново.
    function applySettings() {
        if (!Trainer.typingStarted || Trainer.status === Trainer.Inactive || completed) newText()
    }

    function formatTime(seconds) {
        const total = Math.floor(seconds)
        const minutes = Math.floor(total / 60)
        const rest = total % 60
        return minutes + ":" + (rest < 10 ? "0" : "") + rest
    }

    function handleKey(event) {
        if (editing) return

        switch (event.key) {
        case Qt.Key_Return:
        case Qt.Key_Enter:
            if (paused) Trainer.resume()
            else if (!active || !Trainer.typingStarted) newText()
            event.accepted = true
            return
        case Qt.Key_Tab:
            if (Trainer.status !== Trainer.Inactive) restartText()
            event.accepted = true
            return
        case Qt.Key_Escape:
            if (active) Trainer.pause()
            else if (paused) Trainer.resume()
            else if (completed) resultsDismissed = !resultsDismissed
            event.accepted = true
            return
        case Qt.Key_Backspace:
            if (active) {
                Trainer.backspace()
                idleTimer.restart()
            }
            event.accepted = true
            return
        }

        // Сочетания с Ctrl/Cmd - это команды, а не набор текста.
        if (event.modifiers & (Qt.ControlModifier | Qt.MetaModifier)) return
        if (event.text.length === 0) return

        if (paused) Trainer.resume() // продолжить можно, просто начав печатать
        if (active || paused) {
            Trainer.typeText(event.text)
            idleTimer.restart()
        }
        event.accepted = true
    }

    focus: true
    Keys.onPressed: (event) => page.handleKey(event)
    StackView.onActivated: page.forceActiveFocus()
    Component.onCompleted: newText()

    // Автопауза: после нескольких секунд без нажатий время перестаёт идти.
    Timer {
        id: idleTimer
        interval: 5000
        onTriggered: {
            if (Trainer.autoPause && page.active && Trainer.typingStarted) Trainer.pause()
        }
    }

    Connections {
        target: Trainer

        function onStatusChanged() {
            if (!page.active) {
                idleTimer.stop()
                layoutBanner.visible = false
            }
            if (page.completed) page.resultsDismissed = false
        }

        function onLayoutMismatch(expected_language) {
            layoutBanner.expectedLanguage = expected_language
            layoutBanner.visible = true
            layoutBannerTimer.restart()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 28
        spacing: 18

        // ----- Режим и навигация -----
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            XSegmentedControl {
                model: ["Умный режим", "Свой текст"]
                currentIndex: Trainer.smartMode ? 0 : 1
                enabled: !page.editing
                onActivated: (index) => page.setSmartMode(index === 0)
            }

            XSegmentedControl {
                visible: Trainer.smartMode
                model: ["Русский", "English"]
                currentIndex: Trainer.language === "ru" ? 0 : 1
                enabled: !page.editing
                onActivated: (index) => page.setLanguage(index === 0 ? "ru" : "en")
            }

            Item {
                Layout.fillWidth: true
            }

            XButton {
                text: "Статистика"
                onClicked: page.openStatistics()
            }

            XButton {
                text: "Настройки"
                onClicked: page.openSettings()
            }
        }

        // ----- Метрики -----
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            MetricTile {
                Layout.fillWidth: true
                label: "Скорость"
                value: Trainer.wpm > 0 ? Trainer.wpm.toFixed(0) : "—"
                unit: "WPM"
            }
            MetricTile {
                Layout.fillWidth: true
                label: "Знаков в минуту"
                value: Trainer.cpm > 0 ? Trainer.cpm.toFixed(0) : "—"
            }
            MetricTile {
                Layout.fillWidth: true
                label: "Точность"
                value: Trainer.typingStarted ? Trainer.accuracy.toFixed(1) : "—"
                unit: "%"
            }
            MetricTile {
                Layout.fillWidth: true
                label: "Ритм"
                value: Trainer.consistency > 0 ? Trainer.consistency.toFixed(0) : "—"
                unit: "%"
            }
            MetricTile {
                Layout.fillWidth: true
                label: "Время"
                value: page.formatTime(Trainer.elapsed)
            }
        }

        // ----- Прогресс -----
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 4
            radius: 2
            color: Theme.surfaceAlt

            Rectangle {
                width: Trainer.textLength > 0 ? parent.width * Trainer.cursorPosition / Trainer.textLength : 0
                height: parent.height
                radius: parent.radius
                color: Theme.accent

                Behavior on width {
                    NumberAnimation { duration: 120 }
                }
            }
        }

        // ----- Текст -----
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            TypingArea {
                id: typingArea
                anchors.fill: parent
                visible: !page.editing
                placeholder: Trainer.smartMode
                             ? "Нажмите Enter — текст подберётся под ваши слабые сочетания клавиш"
                             : "Добавьте свой текст кнопкой «Изменить текст»"
            }

            TextEditorPanel {
                id: editor
                anchors.fill: parent
                visible: page.editing
                onSaved: (text) => page.finishEditing(text)
                onCancelled: page.finishEditing(undefined)
            }

            // Пауза
            Rectangle {
                anchors.fill: typingArea
                visible: page.paused && !page.editing
                radius: Theme.radius
                color: Qt.rgba(Theme.surface.r, Theme.surface.g, Theme.surface.b, 0.92)

                Column {
                    anchors.centerIn: parent
                    spacing: 8

                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "Пауза"
                        color: Theme.text
                        font.pixelSize: 30
                        font.weight: Font.DemiBold
                    }

                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "Продолжайте печатать или нажмите Esc"
                        color: Theme.textMuted
                        font.pixelSize: 15
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: Trainer.resume()
                }
            }

            ResultsPanel {
                anchors.fill: typingArea
                visible: page.completed && !page.resultsDismissed && !page.editing
                result: Trainer.lastResult
                onRepeatRequested: page.restartText()
                onNextRequested: page.newText()
            }

            // Предупреждение о раскладке
            Rectangle {
                id: layoutBanner

                property string expectedLanguage: "ru"

                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 18
                visible: false
                implicitWidth: bannerLabel.implicitWidth + 32
                implicitHeight: 36
                radius: height / 2
                color: Theme.warningBackground
                border.color: Theme.warning

                Label {
                    id: bannerLabel
                    anchors.centerIn: parent
                    text: "Похоже, включена не та раскладка — переключитесь на "
                          + (layoutBanner.expectedLanguage === "ru" ? "русскую" : "английскую")
                    color: Theme.warning
                    font.pixelSize: 14
                    font.weight: Font.Medium
                }

                Timer {
                    id: layoutBannerTimer
                    interval: 3000
                    onTriggered: layoutBanner.visible = false
                }
            }
        }

        // ----- Подсказки и действия -----
        RowLayout {
            Layout.fillWidth: true
            visible: !page.editing
            spacing: 18

            Row {
                spacing: 18

                KeyHint {
                    visible: page.active && !Trainer.typingStarted
                    text: "Начните печатать — время пойдёт с первой буквы"
                }
                KeyHint {
                    visible: page.active && Trainer.typingStarted
                    key: "Esc"
                    text: "пауза"
                }
                KeyHint {
                    visible: page.paused
                    key: "Esc"
                    text: "продолжить"
                }
                KeyHint {
                    visible: Trainer.status !== Trainer.Inactive
                    key: "Tab"
                    text: "заново"
                }
                KeyHint {
                    visible: !page.paused && !(page.active && Trainer.typingStarted)
                    key: "Enter"
                    text: "новый текст"
                }
            }

            Item {
                Layout.fillWidth: true
            }

            XButton {
                visible: !Trainer.smartMode
                text: "Изменить текст"
                onClicked: page.startEditing()
            }

            XButton {
                text: "Заново"
                enabled: Trainer.status !== Trainer.Inactive
                onClicked: page.restartText()
            }

            XButton {
                text: "Новый текст"
                highlighted: true
                onClicked: page.newText()
            }
        }
    }
}

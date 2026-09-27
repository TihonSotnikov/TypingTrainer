pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import TypingTrainerModule

ApplicationWindow {
    id: window

    /// Открыть вложенную страницу; идущий набор при этом ставится на паузу.
    function openPage(component) {
        if (Trainer.status === Trainer.Active && Trainer.typingStarted) Trainer.pause()
        stack.push(component)
    }

    width: 1040
    height: 700
    minimumWidth: 780
    minimumHeight: 560
    visible: true
    title: "Typing Trainer"
    color: Theme.background

    Material.theme: Theme.isDark ? Material.Dark : Material.Light
    Material.accent: Theme.accent
    Material.primary: Theme.accent
    Material.background: Theme.surface
    Material.foreground: Theme.text

    // Подсветка набираемого текста строится в C++ - передаём ей цвета темы.
    Binding { target: Trainer; property: "pendingColor"; value: Theme.pending }
    Binding { target: Trainer; property: "correctColor"; value: Theme.correct }
    Binding { target: Trainer; property: "wrongColor"; value: Theme.wrong }
    Binding { target: Trainer; property: "wrongBackground"; value: Theme.wrongBackground }

    // Ушли в другое окно - время не должно идти.
    Connections {
        target: Application

        function onStateChanged() {
            if (Trainer.autoPause && Application.state !== Qt.ApplicationActive
                    && Trainer.status === Trainer.Active && Trainer.typingStarted)
                Trainer.pause()
        }
    }

    StackView {
        id: stack
        anchors.fill: parent
        initialItem: typingPage

        pushEnter: Transition {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 180 }
        }
        pushExit: Transition {
            NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 180 }
        }
        popEnter: Transition {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 180 }
        }
        popExit: Transition {
            NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 180 }
        }
    }

    TypingPage {
        id: typingPage
        onOpenStatistics: window.openPage(statisticsPage)
        onOpenSettings: window.openPage(settingsPage)
    }

    Component {
        id: statisticsPage

        StatisticsPage {
            onBack: stack.pop()
        }
    }

    Component {
        id: settingsPage

        SettingsPage {
            onBack: {
                stack.pop()
                typingPage.applySettings()
            }
        }
    }
}

pragma Singleton
import QtQuick
import QtCore

/// Оформление приложения: палитра, шрифты и размеры. Выбор темы сохраняется между запусками.
Item {
    id: root

    /// "system" - как в ОС, либо "light", "dark", "black".
    property string currentTheme: "system"
    /// Размер шрифта набираемого текста, px.
    property int typingFontSize: 26

    readonly property bool isDark: currentTheme === "system"
                                   ? Application.styleHints.colorScheme === Qt.ColorScheme.Dark
                                   : currentTheme !== "light"
    readonly property string scheme: currentTheme === "black" ? "black" : (isDark ? "dark" : "light")

    function pick(light, dark, black) {
        return scheme === "black" ? black : (scheme === "dark" ? dark : light)
    }

    // ----- Поверхности -----
    readonly property color background: pick("#F3F4F6", "#1A1B1F", "#000000")
    readonly property color surface: pick("#FFFFFF", "#24252B", "#0F0F11")
    readonly property color surfaceAlt: pick("#E9EBEF", "#2E3036", "#1B1B1E")
    readonly property color border: pick("#E1E4E9", "#34363D", "#232327")

    // ----- Текст -----
    readonly property color text: pick("#1E2227", "#E8E9EC", "#EDEDEF")
    readonly property color textMuted: pick("#6A717C", "#9A9DA6", "#8D8E94")

    // ----- Акценты и состояния -----
    readonly property color accent: pick("#2F6BE0", "#6A96F2", "#7C9CFF")
    readonly property color accentText: "#FFFFFF"
    readonly property color success: pick("#1C8A43", "#5CCB7F", "#5CCB7F")
    readonly property color warning: pick("#A35F00", "#F0B458", "#F0B458")
    readonly property color warningBackground: pick("#FFF3DE", "#3B2F1A", "#2A210F")
    /// Затемнение под модальным окном.
    readonly property color scrim: pick("#66000000", "#99000000", "#B3000000")

    // ----- Набираемый текст -----
    readonly property color pending: pick("#9CA3AE", "#626671", "#55585F")
    readonly property color correct: text
    readonly property color wrong: pick("#D0342C", "#FF6F66", "#FF6F66")
    readonly property color wrongBackground: pick("#FCE4E2", "#4A2626", "#3A1515")

    // ----- Шрифты и размеры -----
    readonly property string monoFamily: monoRegular.status === FontLoader.Ready ? monoRegular.name
                                                                                  : "monospace"
    readonly property int radius: 14
    readonly property int smallRadius: 8

    FontLoader {
        id: monoRegular
        source: "fonts/JetBrainsMono-Regular.ttf"
    }

    FontLoader {
        source: "fonts/JetBrainsMono-Bold.ttf"
    }

    Settings {
        category: "Interface"
        property alias currentTheme: root.currentTheme
        property alias typingFontSize: root.typingFontSize
    }
}

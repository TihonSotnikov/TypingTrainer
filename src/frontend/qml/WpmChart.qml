import QtQuick
import QtQuick.Controls
import TypingTrainerModule

/// Линейный график скорости по тренировкам с подсказкой при наведении.
Item {
    id: root

    /// Записи истории (Trainer.history) в хронологическом порядке.
    property var sessions: []
    /// Сколько последних тренировок показывать.
    property int limit: 50

    readonly property var points: sessions.slice(Math.max(0, sessions.length - limit))
    readonly property real maxWpm: {
        let max = 0
        for (const point of points) max = Math.max(max, point.wpm)
        return max
    }
    /// Шаг сетки - «круглое» число (1, 2, 2.5, 3, 4, 5, 6, 8 × 10ⁿ), не больше четырёх делений.
    readonly property real tickStep: {
        const raw = Math.max(20, maxWpm * 1.1) / 4
        const power = Math.pow(10, Math.floor(Math.log10(raw)))
        for (const factor of [1, 2, 2.5, 3, 4, 5, 6, 8, 10])
            if (factor * power >= raw) return factor * power
        return 10 * power
    }
    readonly property int tickCount: Math.ceil(Math.max(20, maxWpm * 1.1) / tickStep)
    readonly property real scaleMax: tickStep * tickCount

    readonly property real plotLeft: 44
    readonly property real plotRight: width - 12
    readonly property real plotTop: 12
    readonly property real plotBottom: height - 28

    property int hoveredIndex: -1

    function xAt(index) {
        if (points.length <= 1) return (plotLeft + plotRight) / 2
        return plotLeft + (plotRight - plotLeft) * index / (points.length - 1)
    }

    function yAt(wpm) {
        return plotBottom - (plotBottom - plotTop) * wpm / scaleMax
    }

    function formatDate(date) {
        return Qt.formatDateTime(date, "dd.MM.yyyy, hh:mm")
    }

    onPointsChanged: canvas.requestPaint()
    onWidthChanged: canvas.requestPaint()
    onHeightChanged: canvas.requestPaint()

    Connections {
        target: Theme

        function onSchemeChanged() {
            canvas.requestPaint()
        }
    }

    Canvas {
        id: canvas
        anchors.fill: parent

        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()

            // Сетка и подписи шкалы - приглушённые, чтобы не спорить с данными.
            ctx.lineWidth = 1
            ctx.strokeStyle = Theme.border
            ctx.fillStyle = Theme.textMuted
            ctx.font = "12px sans-serif"
            ctx.textAlign = "right"
            ctx.textBaseline = "middle"
            for (let i = 0; i <= root.tickCount; ++i) {
                const value = root.scaleMax * i / root.tickCount
                const y = Math.round(root.yAt(value)) + 0.5
                ctx.beginPath()
                ctx.moveTo(root.plotLeft, y)
                ctx.lineTo(root.plotRight, y)
                ctx.stroke()
                ctx.fillText(String(Math.round(value)), root.plotLeft - 8, y)
            }

            const points = root.points
            if (points.length === 0) return

            // Линия скорости.
            ctx.lineWidth = 2
            ctx.lineJoin = "round"
            ctx.lineCap = "round"
            ctx.strokeStyle = Theme.accent
            ctx.beginPath()
            for (let i = 0; i < points.length; ++i) {
                const x = root.xAt(i)
                const y = root.yAt(points[i].wpm)
                if (i === 0) ctx.moveTo(x, y)
                else ctx.lineTo(x, y)
            }
            ctx.stroke()

            // Точки - только пока их немного, иначе они сливаются в линию.
            if (points.length <= 30) {
                ctx.fillStyle = Theme.accent
                ctx.strokeStyle = Theme.surface
                ctx.lineWidth = 2
                for (let i = 0; i < points.length; ++i) {
                    ctx.beginPath()
                    ctx.arc(root.xAt(i), root.yAt(points[i].wpm), 4, 0, 2 * Math.PI)
                    ctx.fill()
                    ctx.stroke()
                }
            }
        }
    }

    // Подписи оси X: первая и последняя тренировка.
    Label {
        x: root.plotLeft
        y: root.plotBottom + 8
        visible: root.points.length > 1
        text: root.points.length > 0 ? root.formatDate(root.points[0].finishedAt) : ""
        color: Theme.textMuted
        font.pixelSize: 12
    }

    Label {
        x: root.plotRight - implicitWidth
        y: root.plotBottom + 8
        visible: root.points.length > 0
        text: root.points.length > 0 ? root.formatDate(root.points[root.points.length - 1].finishedAt) : ""
        color: Theme.textMuted
        font.pixelSize: 12
    }

    // Наведение: ближайшая тренировка по горизонтали.
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true

        onPositionChanged: (mouse) => {
            if (root.points.length === 0) return
            const span = root.plotRight - root.plotLeft
            const ratio = root.points.length > 1 ? (mouse.x - root.plotLeft) / span : 0
            root.hoveredIndex = Math.max(0, Math.min(root.points.length - 1,
                                                     Math.round(ratio * (root.points.length - 1))))
        }
        onExited: root.hoveredIndex = -1
    }

    Rectangle {
        id: crosshair
        visible: root.hoveredIndex >= 0
        x: root.hoveredIndex >= 0 ? root.xAt(root.hoveredIndex) : 0
        y: root.plotTop
        width: 1
        height: root.plotBottom - root.plotTop
        color: Theme.textMuted
        opacity: 0.5
    }

    Rectangle {
        visible: root.hoveredIndex >= 0
        x: crosshair.x - width / 2
        y: root.hoveredIndex >= 0 ? root.yAt(root.points[root.hoveredIndex].wpm) - height / 2 : 0
        width: 10
        height: 10
        radius: 5
        color: Theme.accent
        border.color: Theme.surface
        border.width: 2
    }

    Rectangle {
        id: tooltip

        readonly property var point: root.hoveredIndex >= 0 ? root.points[root.hoveredIndex] : null

        visible: point !== null
        width: tooltipColumn.implicitWidth + 20
        height: tooltipColumn.implicitHeight + 14
        x: Math.min(Math.max(crosshair.x + 12, 0), root.width - width)
        y: root.plotTop
        radius: Theme.smallRadius
        color: Theme.surface
        border.color: Theme.border

        Column {
            id: tooltipColumn
            anchors.centerIn: parent
            spacing: 2

            Label {
                text: tooltip.point ? Math.round(tooltip.point.wpm) + " WPM · "
                                      + tooltip.point.accuracy.toFixed(1) + "%" : ""
                color: Theme.text
                font.pixelSize: 14
                font.weight: Font.DemiBold
            }

            Label {
                text: tooltip.point ? (tooltip.point.mode === "smart" ? "Умный режим" : "Свой текст")
                                      + " · " + root.formatDate(tooltip.point.finishedAt) : ""
                color: Theme.textMuted
                font.pixelSize: 12
            }
        }
    }
}

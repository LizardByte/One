import QtQuick

// Small, code-native line icons stay crisp at desktop, touch, and TV scales.
Canvas {
    id: icon
    property string name: "computer"
    property color color: Theme.text
    implicitWidth: 22
    implicitHeight: 22
    onNameChanged: requestPaint()
    onColorChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    onPaint: {
        const ctx = getContext("2d");
        ctx.reset();
        ctx.scale(width / 24, height / 24);
        ctx.strokeStyle = color;
        ctx.fillStyle = color;
        ctx.lineWidth = 1.7;
        ctx.lineCap = "round";
        ctx.lineJoin = "round";
        function line(x1, y1, x2, y2) {
            ctx.moveTo(x1, y1);
            ctx.lineTo(x2, y2);
        }
        ctx.beginPath();
        if (name === "computer") {
            ctx.roundedRect(3, 4, 18, 13, 2, 2);
            line(12, 17, 12, 21);
            line(8, 21, 16, 21);
        } else if (name === "settings") {
            line(4, 6, 20, 6);
            line(4, 12, 20, 12);
            line(4, 18, 20, 18);
            ctx.stroke();
            ctx.beginPath();
            ctx.fillStyle = Theme.surface;
            ctx.roundedRect(7, 3, 4, 6, 1, 1);
            ctx.roundedRect(14, 9, 4, 6, 1, 1);
            ctx.roundedRect(6, 15, 4, 6, 1, 1);
            ctx.fill();
        } else if (name === "info") {
            ctx.arc(12, 12, 9, 0, Math.PI * 2);
            line(12, 11, 12, 17);
            line(12, 7, 12, 7.1);
        } else if (name === "plus") {
            line(12, 5, 12, 19);
            line(5, 12, 19, 12);
        } else if (name === "back") {
            line(14, 5, 7, 12);
            line(7, 12, 14, 19);
            line(7, 12, 21, 12);
        } else if (name === "arrow") {
            line(4, 12, 20, 12);
            line(14, 6, 20, 12);
            line(20, 12, 14, 18);
        } else if (name === "play") {
            ctx.moveTo(8, 4);
            ctx.lineTo(20, 12);
            ctx.lineTo(8, 20);
            ctx.closePath();
        } else if (name === "refresh") {
            ctx.arc(12, 12, 8, 0.3, Math.PI * 1.8);
            line(19, 3, 19, 8);
            line(14, 8, 19, 8);
        } else if (name === "check") {
            line(5, 12, 10, 17);
            line(10, 17, 20, 6);
        } else if (name === "close") {
            line(6, 6, 18, 18);
            line(18, 6, 6, 18);
        } else if (name === "sun") {
            ctx.arc(12, 12, 4, 0, Math.PI * 2);
            for (let n = 0; n < 8; ++n) {
                const a = n * Math.PI / 4;
                line(12 + Math.cos(a) * 7, 12 + Math.sin(a) * 7, 12 + Math.cos(a) * 10, 12 + Math.sin(a) * 10);
            }
        } else if (name === "gamepad") {
            ctx.moveTo(7, 6);
            ctx.lineTo(17, 6);
            ctx.bezierCurveTo(21, 6, 23, 19, 19, 19);
            ctx.lineTo(15, 16);
            ctx.lineTo(9, 16);
            ctx.bezierCurveTo(0, 25, 1, 6, 7, 6);
            line(6, 10, 6, 14);
            line(4, 12, 8, 12);
            line(16, 10, 16, 10.1);
            line(19, 13, 19, 13.1);
        }
        ctx.stroke();
    }
}

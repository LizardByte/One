import QtQuick

// Static ambient light: redraw on resize/theme changes only. No perpetual
// particle loop or GPU blur, and no work continues behind a stream.
Canvas {
    id: backdrop
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    Connections {
        target: Theme
        function onDarkChanged() {
            backdrop.requestPaint();
        }
    }
    onPaint: {
        const ctx = getContext("2d");
        ctx.reset();
        const radius = Math.max(width, height) * 0.6;
        const glow = ctx.createRadialGradient(width * 0.58, height * 0.44, 0, width * 0.58, height * 0.44, radius);
        glow.addColorStop(0, Theme.dark ? "#202D3C" : "#E2E9F0");
        glow.addColorStop(1, Theme.background);
        ctx.fillStyle = glow;
        ctx.fillRect(0, 0, width, height);
        ctx.fillStyle = Theme.dark ? "#72849A" : "#94A5B4";
        // A sparse, deterministic texture stays still while hosts are browsed.
        for (let n = 0; n < 36; ++n) {
            const x = ((n * 137 + 43) % 997) / 997 * width;
            const y = ((n * 223 + 79) % 991) / 991 * height;
            ctx.globalAlpha = n % 5 === 0 ? 0.6 : 0.3;
            ctx.beginPath();
            ctx.arc(x, y, n % 5 === 0 ? 1.3 : 0.8, 0, Math.PI * 2);
            ctx.fill();
        }
    }
}

import QtQuick

// Paired online hosts are full moons; unpaired online hosts are half moons.
// Offline hosts are crescents regardless of pairing. Focus only changes the ring.
Item {
    id: avatar
    property string identity: ""
    property bool online: true
    property bool paired: false
    property bool selected: false
    readonly property bool fullMoon: online && paired
    readonly property bool halfMoon: online && !paired
    readonly property color orbitColor: online ? (Theme.dark ? "#F3F4F6" : "#626B78") : selected ? (Theme.dark ? "#AAB0B8" : "#737B85") : (Theme.dark ? "#626D79" : "#8B939D")
    readonly property int phaseDirection: {
        let hash = 0;
        for (let i = 0; i < identity.length; ++i)
            hash = ((hash * 31) + identity.charCodeAt(i)) >>> 0;
        return hash % 2 === 0 ? 1 : -1;
    }
    implicitWidth: 200
    implicitHeight: implicitWidth
    onOrbitColorChanged: celestialBody.requestPaint()
    onSelectedChanged: celestialBody.requestPaint()
    onOnlineChanged: celestialBody.requestPaint()
    onPairedChanged: celestialBody.requestPaint()
    onPhaseDirectionChanged: celestialBody.requestPaint()

    Canvas {
        id: celestialBody
        anchors.fill: parent
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        onPaint: {
            const ctx = getContext("2d");
            ctx.reset();
            const size = Math.min(width, height);
            const x = width / 2, y = height / 2;
            const radius = size * 0.34;
            const tau = Math.PI * 2;

            function disc(r) {
                ctx.beginPath();
                ctx.arc(x, y, r, 0, tau);
            }

            if (avatar.fullMoon) {
                const moonlight = ctx.createRadialGradient(x - radius * 0.3, y - radius * 0.4, 0, x, y, radius * 1.3);
                moonlight.addColorStop(0, "#F0F1F2");
                moonlight.addColorStop(0.6, "#CCD1D6");
                moonlight.addColorStop(1, "#8E9AA7");
                ctx.fillStyle = moonlight;
                disc(radius);
                ctx.fill();
                ctx.save();
                ctx.clip();
                // Soft overlapping maria suggest the lunar surface without
                // adding an image dependency or changing host identity.
                for (const mare of [[-0.42, -0.27, 0.28], [-0.22, -0.48, 0.20], [-0.51, 0.05, 0.20], [0.31, -0.39, 0.18], [0.21, 0.47, 0.12]]) {
                    const mx = x + radius * mare[0], my = y + radius * mare[1], mr = radius * mare[2];
                    const shade = ctx.createRadialGradient(mx, my, mr * 0.3, mx, my, mr);
                    shade.addColorStop(0, "#305F7184");
                    shade.addColorStop(1, "#005F7184");
                    ctx.fillStyle = shade;
                    ctx.beginPath();
                    ctx.arc(mx, my, mr, 0, tau);
                    ctx.fill();
                }
                ctx.restore();
            } else {
                // Earthshine keeps the unlit disk visible in both partial phases.
                const earthshine = ctx.createLinearGradient(x - radius, y - radius, x + radius, y + radius);
                earthshine.addColorStop(0, Theme.dark ? "#26343F" : "#D3DCE4");
                earthshine.addColorStop(1, Theme.dark ? "#172129" : "#E1E7EA");
                ctx.fillStyle = earthshine;
                disc(radius);
                ctx.fill();

                ctx.save();
                ctx.translate(x, y);
                ctx.rotate(-Math.PI / 7);
                ctx.scale(avatar.phaseDirection, 1);
                const moonlight = ctx.createLinearGradient(-radius, -radius, radius, radius);
                moonlight.addColorStop(0, avatar.halfMoon ? "#F0F1F2" : Theme.dark ? "#889AA9" : "#7B8D9D");
                moonlight.addColorStop(1, avatar.halfMoon ? "#8E9AA7" : Theme.dark ? "#445564" : "#ABB9C6");
                ctx.fillStyle = moonlight;
                ctx.beginPath();
                ctx.arc(0, 0, radius, -Math.PI / 2, Math.PI / 2);
                if (!avatar.halfMoon)
                    ctx.bezierCurveTo(radius * 0.88, radius * 0.63, radius * 0.88, -radius * 0.63, 0, -radius);
                ctx.closePath();
                ctx.fill();
                ctx.clip();
                // A few restrained maria give the partial moon a lunar surface.
                ctx.fillStyle = "#10253E54";
                for (const crater of [[0.78, -0.22, 0.17], [0.64, 0.42, 0.13], [0.4, -0.69, 0.09]]) {
                    ctx.beginPath();
                    ctx.arc(radius * crater[0], radius * crater[1], radius * crater[2], 0, tau);
                    ctx.fill();
                }
                ctx.restore();
            }

            // Keep selection legible independently of color and reachability.
            if (avatar.selected || !avatar.online) {
                ctx.strokeStyle = avatar.orbitColor;
                ctx.lineWidth = avatar.selected ? 2 : 1;
                ctx.lineCap = "butt";
                const orbitRadius = size * 0.48;
                ctx.beginPath();
                if (avatar.online) {
                    disc(orbitRadius);
                    ctx.stroke();
                } else {
                    // Equal-angle filled segments avoid both the short-arc
                    // quadrant joins and Qt's thin-pen dash-length rounding.
                    // Four chords per edge keep curvature error below 0.02px
                    // at our largest avatar size, with identical gaps at 360°.
                    const halfStroke = ctx.lineWidth / 2;
                    for (let n = 0; n < 40; ++n) {
                        const start = -Math.PI / 2 + n * tau / 40;
                        for (let step = 0; step <= 4; ++step) {
                            const angle = start + step * tau / 320;
                            const px = x + Math.cos(angle) * (orbitRadius + halfStroke);
                            const py = y + Math.sin(angle) * (orbitRadius + halfStroke);
                            if (step === 0)
                                ctx.moveTo(px, py);
                            else
                                ctx.lineTo(px, py);
                        }
                        for (let step = 4; step >= 0; --step) {
                            const angle = start + step * tau / 320;
                            ctx.lineTo(x + Math.cos(angle) * (orbitRadius - halfStroke), y + Math.sin(angle) * (orbitRadius - halfStroke));
                        }
                        ctx.closePath();
                    }
                    ctx.fillStyle = avatar.orbitColor;
                    ctx.fill();
                }
            }
        }
        Connections {
            target: Theme
            function onDarkChanged() {
                celestialBody.requestPaint();
            }
        }
    }
}

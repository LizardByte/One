import QtQuick
import QtQuick.Controls.Basic

Pane {
    padding: Theme.television ? 28 : 22
    background: Rectangle {
        color: Theme.surface
        radius: Theme.radius
        border.color: Theme.border
    }
}

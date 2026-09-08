import QtQuick
import QtQuick.Controls.Basic

Dialog {
    id: dialog
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(parent ? parent.width - 32 : 440, Theme.television ? 600 : 440)
    padding: 24
    modal: true
    background: Rectangle {
        radius: 14
        color: Theme.surface
        border.color: Theme.border
    }
    header: Label {
        text: dialog.title
        padding: 24
        bottomPadding: 12
        font.pixelSize: Theme.television ? 28 : 22
        font.weight: Font.DemiBold
        color: Theme.text
    }
    footer: DialogButtonBox {
        padding: 24
        topPadding: 12
        spacing: 12
        background: Item {}
        delegate: OneButton {}
    }
    Overlay.modal: Rectangle {
        color: "#AA071210"
    }
}

import QtQuick
import QtQuick.Controls.Basic

// Shared portrait surface for library cards and the selected-title preview.
Rectangle {
    id: poster
    property string source: ""
    property string title: ""
    readonly property bool ready: image.status === Image.Ready
    implicitWidth: 180
    implicitHeight: implicitWidth * 4 / 3
    color: Theme.elevated
    radius: 6
    clip: true
    Image {
        id: image
        objectName: "posterImage"
        anchors.fill: parent
        source: poster.source
        asynchronous: true
        cache: false
        fillMode: Image.PreserveAspectCrop
    }
    Column {
        anchors.centerIn: parent
        width: parent.width - 24
        spacing: 18
        visible: !poster.ready
        OneIcon {
            anchors.horizontalCenter: parent.horizontalCenter
            implicitWidth: Math.min(56, poster.width / 3)
            implicitHeight: implicitWidth
            name: poster.title.toLowerCase() === "desktop" ? "computer" : "gamepad"
            color: Theme.muted
        }
        Label {
            width: parent.width
            text: poster.title
            color: Theme.text
            font.pixelSize: Theme.television ? 20 : 16
            font.weight: Font.DemiBold
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            maximumLineCount: 3
            elide: Text.ElideRight
        }
    }
}

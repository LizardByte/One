import QtQuick

Item {
    id: artwork
    property bool online: true
    implicitWidth: 240
    implicitHeight: 150
    // Abstract scenery, drawn locally: no remote assets or GPU effects required.
    Rectangle {
        anchors.centerIn: parent
        anchors.verticalCenterOffset: -7
        width: Math.min(parent.width * 0.8, 208, (parent.height - 24) / 0.59)
        height: width * 0.59
        radius: 9
        color: Theme.dark ? "#101E20" : "#DDE9E4"
        border.color: artwork.online ? Theme.accent : Theme.muted
        border.width: 2
        Rectangle {
            anchors.fill: parent
            anchors.margins: 7
            radius: 4
            clip: true
            gradient: Gradient {
                GradientStop {
                    position: 0
                    color: artwork.online ? "#264B45" : "#344047"
                }
                GradientStop {
                    position: 1
                    color: "#14282D"
                }
            }
            Rectangle {
                x: parent.width * 0.62
                y: parent.height * 0.18
                width: parent.height * 0.35
                height: width
                radius: width / 2
                color: artwork.online ? "#B2E3BF" : "#879796"
            }
            Rectangle {
                x: -parent.width * 0.14
                y: parent.height * 0.66
                width: parent.width
                height: parent.height
                radius: 20
                rotation: -24
                color: artwork.online ? "#437866" : "#495C5D"
            }
            Rectangle {
                x: parent.width * 0.40
                y: parent.height * 0.73
                width: parent.width
                height: parent.height
                radius: 18
                rotation: 30
                color: artwork.online ? "#224D44" : "#2C4145"
            }
        }
        Rectangle {
            anchors.top: parent.bottom
            anchors.horizontalCenter: parent.horizontalCenter
            width: 3
            height: 11
            color: artwork.online ? Theme.accent : Theme.muted
        }
        Rectangle {
            anchors.top: parent.bottom
            anchors.topMargin: 11
            anchors.horizontalCenter: parent.horizontalCenter
            width: 42
            height: 3
            radius: 1.5
            color: artwork.online ? Theme.accent : Theme.muted
        }
    }
}

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    id: badge
    property string text
    property color tone: Theme.accent
    property bool dot: true
    implicitWidth: contents.implicitWidth + 20
    implicitHeight: contents.implicitHeight + 12
    radius: 4
    color: Qt.alpha(tone, Theme.dark ? 0.12 : 0.09)
    RowLayout {
        id: contents
        anchors.centerIn: parent
        spacing: 7
        Rectangle {
            visible: badge.dot
            width: 6
            height: 6
            radius: 3
            color: badge.tone
        }
        Label {
            text: badge.text
            color: badge.tone
            font.pixelSize: Theme.television ? 16 : 12
            font.weight: Font.DemiBold
        }
    }
}

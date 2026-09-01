import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    id: page

    required property bool televisionMode

    padding: televisionMode ? 48 : 28

    ColumnLayout {
        anchors.fill: parent
        spacing: 16

        Label {
            text: qsTr("About One")
            font.pixelSize: page.televisionMode ? 36 : 30
            font.bold: true
        }

        Label {
            Layout.maximumWidth: 720
            text: qsTr("One is a universal client for streaming games and applications from Sunshine.")
            wrapMode: Text.WordWrap
        }

        Label {
            Layout.maximumWidth: 720
            text: qsTr("This interface is shared by desktop, Android, Android TV, iOS, and Apple TV targets. Platform-native services provide decoding, rendering, audio, input, and application lifecycle integration.")
            wrapMode: Text.WordWrap
            opacity: 0.72
        }

        Item { Layout.fillHeight: true }
    }
}


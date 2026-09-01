import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    id: page

    required property bool televisionMode

    padding: televisionMode ? 48 : 28

    ColumnLayout {
        anchors.fill: parent
        spacing: page.televisionMode ? 24 : 16

        Label {
            text: qsTr("Settings")
            font.pixelSize: page.televisionMode ? 36 : 30
            font.bold: true
        }

        Frame {
            Layout.fillWidth: true

            GridLayout {
                anchors.fill: parent
                columns: page.televisionMode ? 2 : (page.width >= 720 ? 2 : 1)
                columnSpacing: 24
                rowSpacing: 18

                Label { text: qsTr("Resolution") }
                ComboBox {
                    Layout.fillWidth: true
                    model: [qsTr("Native"), qsTr("1920 × 1080"), qsTr("1280 × 720")]
                    focusPolicy: Qt.StrongFocus
                }

                Label { text: qsTr("Frame rate") }
                ComboBox {
                    Layout.fillWidth: true
                    model: [qsTr("60 FPS"), qsTr("120 FPS")]
                    focusPolicy: Qt.StrongFocus
                }

                Label { text: qsTr("Video codec") }
                ComboBox {
                    Layout.fillWidth: true
                    model: [qsTr("Automatic"), qsTr("H.264"), qsTr("HEVC"), qsTr("AV1")]
                    focusPolicy: Qt.StrongFocus
                }
            }
        }

        Item { Layout.fillHeight: true }
    }
}


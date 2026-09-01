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

        RowLayout {
            Layout.fillWidth: true

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                Label {
                    text: qsTr("Computers")
                    font.pixelSize: page.televisionMode ? 36 : 30
                    font.bold: true
                }

                Label {
                    text: qsTr("Connect to a Sunshine host on your network.")
                    opacity: 0.72
                }
            }

            Button {
                id: addComputerButton
                text: qsTr("Add computer")
                focus: true
                focusPolicy: Qt.StrongFocus
            }
        }

        Frame {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ColumnLayout {
                anchors.centerIn: parent
                spacing: 12

                Label {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("No computers yet")
                    font.pixelSize: page.televisionMode ? 28 : 22
                    font.bold: true
                }

                Label {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("Discovered and manually added hosts will appear here.")
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    opacity: 0.72
                }
            }
        }
    }
}


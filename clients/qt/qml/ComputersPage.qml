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
                onClicked: addComputerDialog.open()
            }
        }

        Frame {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: ComputerModel.count === 0

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

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: ComputerModel.count > 0
            clip: true
            spacing: page.televisionMode ? 16 : 10
            model: ComputerModel

            delegate: Frame {
                id: hostCard

                required property string displayName
                required property string address
                required property int port
                required property bool online
                required property bool paired

                width: ListView.view.width
                height: page.televisionMode ? 112 : 88

                RowLayout {
                    anchors.fill: parent
                    spacing: 16

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        Label {
                            text: hostCard.displayName
                            font.pixelSize: page.televisionMode ? 25 : 20
                            font.bold: true
                        }

                        Label {
                            text: qsTr("%1:%2").arg(hostCard.address).arg(hostCard.port)
                            opacity: 0.72
                        }
                    }

                    Label {
                        text: hostCard.online ? qsTr("Online") : qsTr("Not connected")
                        opacity: hostCard.online ? 1 : 0.62
                    }

                    Button {
                        text: hostCard.paired ? qsTr("View games") : qsTr("Pair")
                        enabled: hostCard.online
                        focusPolicy: Qt.StrongFocus
                    }
                }
            }
        }
    }

    Dialog {
        id: addComputerDialog

        anchors.centerIn: parent
        title: qsTr("Add computer")
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        onOpened: {
            standardButton(Dialog.Ok).enabled = hostAddress.text.trim().length > 0
            hostAddress.forceActiveFocus()
        }
        onAccepted: {
            if (ComputerModel.addManualHost(hostAddress.text))
                hostAddress.clear()
        }

        contentItem: ColumnLayout {
            spacing: 10

            Label {
                text: qsTr("Hostname or IP address")
            }

            TextField {
                id: hostAddress

                Layout.preferredWidth: page.televisionMode ? 480 : 340
                placeholderText: qsTr("sunshine.example.com")
                inputMethodHints: Qt.ImhUrlCharactersOnly
                onTextChanged: {
                    if (addComputerDialog.visible)
                        addComputerDialog.standardButton(Dialog.Ok).enabled = text.trim().length > 0
                }
            }
        }
    }
}

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import One

Page {
    id: page

    required property bool televisionMode
    required property int applicationId
    required property string applicationTitle
    required property var streamingController
    signal backRequested
    function focusPrimaryAction() {
        disconnectButton.forceActiveFocus();
    }

    function leaveStream() {
        page.streamingController.stop();
        page.backRequested();
    }

    Component.onCompleted: page.streamingController.startApplication(page.applicationId, page.applicationTitle, videoOutput)
    Component.onDestruction: page.streamingController.stop()

    background: Rectangle {
        color: "black"
    }

    StreamVideoOutput {
        id: videoOutput
        anchors.fill: parent
    }

    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: controls.implicitHeight + 32
        color: "#EB141D20"
        visible: !videoOutput.inputCaptured

        RowLayout {
            id: controls
            anchors.fill: parent
            anchors.margins: 16
            spacing: 12

            OneButton {
                id: disconnectButton
                text: qsTr("Disconnect")
                symbol: "back"
                focusPolicy: Qt.StrongFocus
                onClicked: page.leaveStream()
            }

            Label {
                Layout.fillWidth: true
                text: page.applicationTitle
                color: "#F2F6F4"
                font.pixelSize: page.televisionMode ? 24 : 18
                font.bold: true
                elide: Text.ElideRight
            }

            BusyIndicator {
                visible: page.streamingController.busy && !page.streamingController.streaming
                running: visible
            }

            Label {
                visible: page.width > 720 && page.streamingController.controllerCount > 0
                text: page.streamingController.controllerCount === 1 ? qsTr("1 controller") : qsTr("%1 controllers").arg(page.streamingController.controllerCount)
                color: "#A6B6B4"
            }

            Label {
                visible: page.width > 900
                text: page.streamingController.status
                color: "#A6B6B4"
                elide: Text.ElideRight
                Layout.maximumWidth: 420
            }
        }
    }

    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 24
        width: Math.min(parent.width - 32, inputHint.implicitWidth + 28)
        height: inputHint.implicitHeight + 18
        radius: height / 2
        color: "#C0000000"
        visible: videoOutput.inputEnabled && !videoOutput.inputCaptured

        Label {
            id: inputHint
            anchors.centerIn: parent
            width: parent.width - 28
            text: qsTr("Click the stream to capture input · Ctrl+Alt+Shift+Z releases it")
            color: "white"
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
        }
    }

    Surface {
        anchors.centerIn: parent
        width: Math.min(parent.width - 48, 560)
        visible: page.streamingController.error.length > 0

        ColumnLayout {
            anchors.fill: parent
            spacing: 14

            Label {
                Layout.fillWidth: true
                text: page.streamingController.error
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
            }

            OneButton {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("Back to applications")
                primary: true
                focusPolicy: Qt.StrongFocus
                onClicked: page.leaveStream()
            }
        }
    }

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - 48, 480)
        visible: page.streamingController.busy && !page.streamingController.streaming && page.streamingController.error.length === 0
        spacing: 16
        BusyIndicator {
            Layout.alignment: Qt.AlignHCenter
            running: parent.visible
        }
        Label {
            Layout.fillWidth: true
            text: qsTr("Connecting")
            color: "#F2F6F4"
            font.pixelSize: page.televisionMode ? 28 : 23
            font.weight: Font.DemiBold
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
        }
        Label {
            Layout.fillWidth: true
            text: page.streamingController.status
            color: "#A6B6B4"
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
        }
    }
}

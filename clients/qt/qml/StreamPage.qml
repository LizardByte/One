pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import One

Page {
    id: page

    required property bool televisionMode
    required property int applicationId
    required property string applicationTitle
    required property var streamingController
    signal backRequested()

    function leaveStream() {
        page.streamingController.stop()
        page.backRequested()
    }

    Component.onCompleted: page.streamingController.startApplication(
        page.applicationId,
        page.applicationTitle,
        videoOutput)
    Component.onDestruction: page.streamingController.stop()

    background: Rectangle { color: "black" }

    StreamVideoOutput {
        id: videoOutput
        anchors.fill: parent
    }

    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: controls.implicitHeight + 24
        color: "#B0000000"

        RowLayout {
            id: controls
            anchors.fill: parent
            anchors.margins: 12
            spacing: 12

            Button {
                text: qsTr("Back")
                focusPolicy: Qt.StrongFocus
                onClicked: page.leaveStream()
            }

            Label {
                Layout.fillWidth: true
                text: page.applicationTitle
                color: "white"
                font.pixelSize: page.televisionMode ? 24 : 18
                font.bold: true
                elide: Text.ElideRight
            }

            BusyIndicator {
                visible: page.streamingController.busy && !page.streamingController.streaming
                running: visible
            }

            Label {
                text: page.streamingController.status
                color: "white"
                opacity: 0.82
                elide: Text.ElideRight
                Layout.maximumWidth: 420
            }
        }
    }

    Frame {
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

            Button {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("Back to applications")
                focusPolicy: Qt.StrongFocus
                onClicked: page.leaveStream()
            }
        }
    }
}

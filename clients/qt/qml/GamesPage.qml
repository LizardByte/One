pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    id: page

    required property bool televisionMode
    required property string hostName
    required property var applicationModel
    signal backRequested()

    readonly property int minimumCardWidth: televisionMode ? 260 : 210
    readonly property int columnCount: Math.max(1, Math.floor(gameGrid.width / minimumCardWidth))

    padding: televisionMode ? 48 : 28

    ColumnLayout {
        anchors.fill: parent
        spacing: page.televisionMode ? 24 : 16

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Button {
                text: qsTr("Back")
                focusPolicy: Qt.StrongFocus
                onClicked: page.backRequested()
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Label {
                    text: page.hostName
                    font.pixelSize: page.televisionMode ? 36 : 30
                    font.bold: true
                }

                Label {
                    text: qsTr("Choose an application to stream.")
                    opacity: 0.72
                }
            }

            Button {
                text: qsTr("Refresh")
                enabled: !page.applicationModel.loading
                focusPolicy: Qt.StrongFocus
                onClicked: page.applicationModel.refresh()
            }
        }

        BusyIndicator {
            Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
            Layout.fillHeight: true
            visible: page.applicationModel.loading
            running: visible
        }

        Frame {
            Layout.fillWidth: true
            visible: !page.applicationModel.loading && page.applicationModel.error.length > 0

            ColumnLayout {
                anchors.fill: parent
                spacing: 12

                Label {
                    Layout.fillWidth: true
                    text: page.applicationModel.error
                    wrapMode: Text.WordWrap
                }

                Button {
                    text: qsTr("Try again")
                    focusPolicy: Qt.StrongFocus
                    onClicked: page.applicationModel.refresh()
                }
            }
        }

        Frame {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !page.applicationModel.loading && page.applicationModel.error.length === 0 && page.applicationModel.count === 0

            Label {
                anchors.centerIn: parent
                text: qsTr("No applications were returned by this host.")
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }
        }

        GridView {
            id: gameGrid

            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !page.applicationModel.loading && page.applicationModel.error.length === 0 && page.applicationModel.count > 0
            clip: true
            cellWidth: width / page.columnCount
            cellHeight: page.televisionMode ? 180 : 150
            model: page.applicationModel
            ScrollBar.vertical: ScrollBar {}

            delegate: Frame {
                id: gameCard

                required property int index
                required property int applicationId
                required property string title
                required property bool hdrSupported
                required property bool appCollectorGame

                width: gameGrid.cellWidth - 12
                height: gameGrid.cellHeight - 12

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 8

                    Label {
                        Layout.fillWidth: true
                        text: gameCard.title.length > 0 ? gameCard.title : qsTr("Untitled application")
                        font.pixelSize: page.televisionMode ? 24 : 19
                        font.bold: true
                        wrapMode: Text.WordWrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                    }

                    Item { Layout.fillHeight: true }

                    RowLayout {
                        Layout.fillWidth: true

                        Label {
                            visible: gameCard.hdrSupported
                            text: qsTr("HDR")
                            opacity: 0.72
                        }

                        Item { Layout.fillWidth: true }

                        Label {
                            text: qsTr("Launch support next")
                            opacity: 0.55
                        }
                    }
                }
            }
        }
    }
}

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Page {
    id: page
    required property bool televisionMode
    required property string hostName
    required property string hostEndpoint
    required property var applicationModel
    required property var streamingController
    signal backRequested
    signal streamRequested(int applicationId, string title)
    signal navigationRequested
    function focusPrimaryAction() {
        backButton.forceActiveFocus();
    }
    readonly property bool compact: width < 600
    readonly property int columnCount: Math.max(1, Math.floor((gameGrid.width + 12) / (televisionMode ? 360 : 280)))
    padding: televisionMode ? 36 : 28
    background: Rectangle {
        color: Theme.background
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: page.compact ? 18 : 24
        RowLayout {
            Layout.fillWidth: true
            OneButton {
                id: backButton
                objectName: "backToComputers"
                text: qsTr("Computers")
                symbol: "back"
                quiet: true
                onClicked: page.backRequested()
                Keys.onUpPressed: page.navigationRequested()
                Keys.onLeftPressed: page.navigationRequested()
                Keys.onDownPressed: gameGrid.focusCard(0)
            }
            Item {
                Layout.fillWidth: true
            }
            OneButton {
                objectName: "refreshApplications"
                text: qsTr("Refresh")
                symbol: "refresh"
                quiet: true
                enabled: !page.applicationModel.loading && !page.applicationModel.pairingRequired
                onClicked: page.applicationModel.refresh()
            }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6
                Label {
                    Layout.fillWidth: true
                    text: page.hostName
                    font.pixelSize: Theme.titleSize
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }
                Label {
                    Layout.fillWidth: true
                    text: page.hostEndpoint
                    color: Theme.muted
                    font.pixelSize: Theme.television ? 16 : 12
                    elide: Text.ElideMiddle
                }
            }
            Label {
                text: qsTr("%1 applications").arg(page.applicationModel.count)
                color: Theme.muted
                visible: !page.applicationModel.loading && page.applicationModel.error.length === 0
            }
        }
        Surface {
            Layout.fillWidth: true
            visible: !page.streamingController.available
            padding: 16
            Label {
                width: parent.width
                text: page.streamingController.unavailableReason
                color: Theme.warning
                wrapMode: Text.WordWrap
            }
        }
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: page.applicationModel.loading
            spacing: 14
            Item {
                Layout.fillHeight: true
            }
            BusyIndicator {
                Layout.alignment: Qt.AlignHCenter
                running: parent.visible
            }
            Label {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("Loading applications…")
                color: Theme.muted
            }
            Item {
                Layout.fillHeight: true
            }
        }
        Surface {
            Layout.fillWidth: true
            Layout.fillHeight: true
            background: Item {}
            visible: !page.applicationModel.loading && (page.applicationModel.error.length > 0 || page.applicationModel.count === 0)
            ColumnLayout {
                anchors.centerIn: parent
                width: Math.min(parent.width, 480)
                spacing: 16
                OneIcon {
                    Layout.alignment: Qt.AlignHCenter
                    implicitWidth: 44
                    implicitHeight: 44
                    name: page.applicationModel.error.length > 0 ? "info" : "gamepad"
                    color: Theme.accent
                }
                Label {
                    Layout.fillWidth: true
                    text: page.applicationModel.error.length > 0 ? (page.applicationModel.pairingRequired ? qsTr("Computer is unpaired") : qsTr("Could not load applications")) : qsTr("No applications")
                    font.pixelSize: Theme.television ? 28 : 22
                    font.weight: Font.DemiBold
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                }
                Label {
                    Layout.fillWidth: true
                    text: page.applicationModel.pairingRequired ? qsTr("The certificate was rejected or no longer matches. Return to Computers and pair this instance again.") : page.applicationModel.error.length > 0 ? page.applicationModel.error : qsTr("Add games or applications in Sunshine, then refresh to see them here.")
                    color: Theme.muted
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                }
                OneButton {
                    Layout.alignment: Qt.AlignHCenter
                    objectName: "libraryRecovery"
                    text: page.applicationModel.pairingRequired ? qsTr("Back to computers") : qsTr("Try again")
                    symbol: page.applicationModel.pairingRequired ? "back" : "refresh"
                    visible: page.applicationModel.error.length > 0
                    onClicked: page.applicationModel.pairingRequired ? page.backRequested() : page.applicationModel.refresh()
                }
            }
        }
        GridView {
            id: gameGrid
            objectName: "gameGrid"
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !page.applicationModel.loading && page.applicationModel.error.length === 0 && page.applicationModel.count > 0
            clip: true
            cellWidth: width / page.columnCount
            cellHeight: page.televisionMode ? 370 : 334
            model: page.applicationModel
            ScrollBar.vertical: ScrollBar {}

            delegate: Button {
                id: gameCard
                required property int index
                required property int applicationId
                required property string title
                required property bool hdrSupported
                required property string artwork
                required property bool appCollectorGame
                readonly property string displayTitle: title.length > 0 ? title : qsTr("Untitled application")
                objectName: "gameCard" + index
                width: gameGrid.cellWidth - (page.columnCount > 1 ? 14 : 0)
                height: gameGrid.cellHeight - 14
                padding: 0
                text: displayTitle
                Accessible.name: qsTr("Stream %1").arg(displayTitle)
                enabled: page.streamingController.available && !page.streamingController.busy
                focusPolicy: Qt.StrongFocus
                hoverEnabled: true
                onClicked: page.streamRequested(applicationId, title)
                Component.onCompleted: page.applicationModel.requestArtwork(applicationId)
                Keys.onRightPressed: gameGrid.focusCard(index + 1)
                Keys.onLeftPressed: {
                    if (index % page.columnCount === 0)
                        page.navigationRequested();
                    else
                        gameGrid.focusCard(index - 1);
                }
                Keys.onDownPressed: gameGrid.focusCard(index + page.columnCount)
                Keys.onUpPressed: {
                    if (index < page.columnCount)
                        backButton.forceActiveFocus();
                    else
                        gameGrid.focusCard(index - page.columnCount);
                }
                ToolTip.visible: hovered
                ToolTip.delay: 700
                ToolTip.text: displayTitle
                background: Rectangle {
                    radius: Theme.radius
                    color: gameCard.hovered ? Theme.elevated : Theme.surface
                    border.color: gameCard.activeFocus || gameCard.hovered ? Theme.accent : Theme.border
                    border.width: gameCard.activeFocus ? 2 : 1
                    Behavior on color {
                        ColorAnimation {
                            duration: 120
                        }
                    }
                }
                contentItem: ColumnLayout {
                    spacing: 0
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.margins: 10
                        radius: 9
                        color: Theme.dark ? "#142521" : "#E7F1EC"
                        Image {
                            id: cover
                            anchors.fill: parent
                            anchors.margins: 1
                            source: gameCard.artwork
                            asynchronous: true
                            cache: false
                            fillMode: Image.PreserveAspectFit
                        }
                        HostArtwork {
                            anchors.fill: parent
                            anchors.margins: 12
                            visible: cover.status !== Image.Ready && gameCard.displayTitle.toLowerCase() === "desktop"
                        }
                        OneIcon {
                            anchors.centerIn: parent
                            name: "gamepad"
                            implicitWidth: page.televisionMode ? 76 : 60
                            implicitHeight: implicitWidth
                            color: Theme.accent
                            visible: cover.status !== Image.Ready && gameCard.displayTitle.toLowerCase() !== "desktop"
                        }
                        StatusBadge {
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: 12
                            visible: gameCard.hdrSupported
                            text: qsTr("HDR")
                            dot: false
                            tone: Theme.accent
                        }
                    }
                    Label {
                        Layout.fillWidth: true
                        Layout.leftMargin: 20
                        Layout.rightMargin: 20
                        Layout.preferredHeight: page.televisionMode ? 60 : 46
                        text: gameCard.displayTitle
                        font.pixelSize: page.televisionMode ? 23 : 18
                        font.weight: Font.DemiBold
                        color: Theme.text
                        wrapMode: Text.WordWrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.margins: 20
                        Layout.topMargin: 8
                        Label {
                            text: qsTr("Stream")
                            color: Theme.accent
                            font.pixelSize: Theme.television ? 18 : 13
                        }
                        Item {
                            Layout.fillWidth: true
                        }
                        OneIcon {
                            name: "play"
                            color: Theme.accent
                            implicitWidth: 18
                            implicitHeight: 18
                        }
                    }
                }
            }
            function focusCard(index) {
                if (index < 0 || index >= count)
                    return;
                positionViewAtIndex(index, GridView.Contain);
                const card = itemAtIndex(index);
                if (card)
                    card.forceActiveFocus();
            }
        }
    }
}

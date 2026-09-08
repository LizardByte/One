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
    readonly property int columnCount: Math.max(2, Math.floor((gameGrid.width + 18) / (televisionMode ? 210 : 190)))
    readonly property bool libraryReady: !applicationModel.loading && applicationModel.error.length === 0 && applicationModel.count > 0
    readonly property bool canStream: streamingController.available && !streamingController.busy
    readonly property var selectedCard: gameGrid.currentItem
    padding: televisionMode ? 36 : compact ? 20 : 28
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
                    font.pixelSize: page.televisionMode ? 24 : 18
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
                text: qsTr("%1 titles").arg(page.applicationModel.count)
                color: Theme.muted
                visible: !page.applicationModel.loading && page.applicationModel.error.length === 0
            }
        }
        // Reserve separate roles for future backdrops and metadata; today's
        // selected-title view uses only the real title and portrait artwork.
        Rectangle {
            id: preview
            objectName: "titlePreview"
            Layout.fillWidth: true
            Layout.preferredHeight: page.televisionMode ? 180 : 200
            visible: page.libraryReady && !page.compact && page.height >= (page.televisionMode ? 680 : 700)
            radius: 10
            color: Theme.surface
            RowLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 24
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 10
                    Label {
                        text: page.selectedCard && page.selectedCard.appCollectorGame ? qsTr("Game") : qsTr("Application")
                        color: Theme.muted
                        font.pixelSize: page.televisionMode ? 16 : 12
                    }
                    Label {
                        objectName: "selectedTitle"
                        Layout.fillWidth: true
                        text: page.selectedCard ? page.selectedCard.displayTitle : ""
                        font.pixelSize: page.televisionMode ? 36 : 34
                        font.weight: Font.DemiBold
                        maximumLineCount: 2
                        wrapMode: Text.WordWrap
                        elide: Text.ElideRight
                    }
                    Item {
                        Layout.fillHeight: true
                    }
                    OneButton {
                        objectName: "playSelected"
                        text: qsTr("Play")
                        symbol: "play"
                        primary: true
                        enabled: page.canStream && page.selectedCard !== null
                        Accessible.name: page.selectedCard ? qsTr("Play %1").arg(page.selectedCard.displayTitle) : text
                        onClicked: page.streamRequested(page.selectedCard.applicationId, page.selectedCard.title)
                        Keys.onDownPressed: gameGrid.focusCard(gameGrid.currentIndex)
                        Keys.onUpPressed: backButton.forceActiveFocus()
                    }
                }
                PosterArtwork {
                    Layout.fillHeight: true
                    Layout.preferredWidth: (preview.height - 40) * 3 / 4
                    title: page.selectedCard ? page.selectedCard.displayTitle : ""
                    source: page.selectedCard ? page.selectedCard.artwork : ""
                }
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
        Label {
            text: qsTr("Library")
            font.pixelSize: page.televisionMode ? 28 : 23
            font.weight: Font.DemiBold
            visible: page.libraryReady
        }
        GridView {
            id: gameGrid
            objectName: "gameGrid"
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !page.applicationModel.loading && page.applicationModel.error.length === 0 && page.applicationModel.count > 0
            clip: true
            readonly property int gap: page.televisionMode ? 24 : 18
            cellWidth: width / page.columnCount
            cellHeight: (cellWidth - gap) * 4 / 3 + (page.televisionMode ? 68 : 58)
            currentIndex: 0
            // Hover updates the preview without scrolling the shelf underneath
            // the pointer. Keyboard navigation positions the view explicitly.
            highlightFollowsCurrentItem: false
            model: page.applicationModel
            ScrollBar.vertical: ScrollBar {}

            delegate: Button {
                id: gameCard
                required property int index
                required property int applicationId
                required property string title
                required property bool hdrSupported
                required property string artwork
                required property string artworkError
                required property bool appCollectorGame
                readonly property string displayTitle: title.length > 0 ? title : qsTr("Untitled application")
                objectName: "gameCard" + index
                width: gameGrid.cellWidth - gameGrid.gap
                height: gameGrid.cellHeight - 14
                padding: 0
                text: displayTitle
                Accessible.name: qsTr("Play %1").arg(displayTitle)
                focusPolicy: Qt.StrongFocus
                hoverEnabled: true
                onClicked: if (page.canStream)
                    page.streamRequested(applicationId, title)
                onHoveredChanged: if (hovered)
                    gameGrid.currentIndex = index
                onActiveFocusChanged: if (activeFocus)
                    gameGrid.currentIndex = index
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
                ToolTip.text: artworkError.length > 0 ? artworkError : displayTitle
                background: Item {}
                contentItem: Item {
                    PosterArtwork {
                        id: cover
                        objectName: "gamePoster" + gameCard.index
                        width: parent.width
                        height: width * 4 / 3
                        source: gameCard.artwork
                        title: gameCard.displayTitle
                        Rectangle {
                            anchors.fill: parent
                            color: "#30000000"
                            visible: gameCard.hovered || gameCard.activeFocus
                        }
                        Rectangle {
                            anchors.centerIn: parent
                            width: page.televisionMode ? 64 : 48
                            height: width
                            radius: width / 2
                            color: Theme.accent
                            visible: page.canStream && (gameCard.hovered || gameCard.activeFocus)
                            OneIcon {
                                anchors.centerIn: parent
                                name: "play"
                                color: Theme.accentText
                            }
                        }
                        Rectangle {
                            anchors.fill: parent
                            color: "transparent"
                            radius: 6
                            border.width: gameCard.activeFocus ? 3 : 2
                            border.color: Theme.accent
                            visible: gameCard.hovered || gameCard.activeFocus
                        }
                        StatusBadge {
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: 10
                            visible: gameCard.hdrSupported
                            text: qsTr("HDR")
                            dot: false
                            tone: Theme.accent
                        }
                        OneButton {
                            objectName: "retryArtwork" + gameCard.index
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.bottom: parent.bottom
                            anchors.bottomMargin: 10
                            visible: gameCard.artworkError.length > 0
                            text: qsTr("Retry cover")
                            symbol: "refresh"
                            implicitHeight: page.televisionMode ? 48 : 36
                            onClicked: page.applicationModel.retryArtwork(gameCard.applicationId)
                            ToolTip.visible: hovered
                            ToolTip.text: gameCard.artworkError
                        }
                    }
                    Label {
                        anchors.top: cover.bottom
                        anchors.topMargin: 10
                        width: parent.width
                        text: gameCard.displayTitle
                        font.pixelSize: page.televisionMode ? 21 : 15
                        font.weight: gameCard.hovered || gameCard.activeFocus ? Font.DemiBold : Font.Normal
                        color: Theme.text
                        elide: Text.ElideRight
                    }
                }
            }
            function focusCard(index) {
                if (index < 0 || index >= count)
                    return;
                currentIndex = index;
                positionViewAtIndex(index, GridView.Contain);
                const card = itemAtIndex(index);
                if (card)
                    card.forceActiveFocus();
            }
        }
    }
}

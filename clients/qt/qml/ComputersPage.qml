pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Page {
    id: page
    required property bool televisionMode
    required property var computerModel
    signal gamesRequested(string hostName, string hostEndpoint)
    signal navigationRequested
    function focusPrimaryAction() {
        addComputerButton.forceActiveFocus();
    }
    readonly property bool compact: width < 600
    readonly property bool shortWindow: height < 560
    padding: televisionMode ? 36 : 28
    background: Rectangle {
        color: Theme.background
    }

    function beginPairing(row, stableId, endpoint) {
        const pin = page.computerModel.pairHost(row);
        pairingDialog.activeHostId = stableId;
        pairingDialog.instanceEndpoint = endpoint;
        pairingDialog.pin = pin;
        pairingDialog.pending = pin.length > 0;
        pairingDialog.resultText = pin.length > 0 ? "" : qsTr("Unable to start pairing for this computer.");
        pairingDialog.open();
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: page.shortWindow ? 16 : page.compact ? 20 : 28
        GridLayout {
            columns: page.compact ? 1 : 2
            Layout.fillWidth: true
            rowSpacing: 18
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 8
                Label {
                    Layout.fillWidth: true
                    text: qsTr("Computers")
                    font.pixelSize: Theme.titleSize
                    font.weight: Font.DemiBold
                    font.letterSpacing: 0
                }
                Label {
                    Layout.fillWidth: true
                    text: qsTr("Sunshine instances on your network")
                    visible: !page.shortWindow
                    color: Theme.muted
                    wrapMode: Text.WordWrap
                }
            }
            OneButton {
                id: addComputerButton
                objectName: "addComputerButton"
                text: qsTr("Add computer")
                symbol: "plus"
                primary: true
                focus: true
                onClicked: addComputerDialog.open()
                Keys.onUpPressed: page.navigationRequested()
                Keys.onLeftPressed: page.navigationRequested()
                Keys.onDownPressed: hostGrid.focusAction(0)
            }
        }

        Surface {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: page.computerModel.count === 0
            ScrollView {
                id: emptyScroll
                implicitWidth: 0
                implicitHeight: 0
                anchors.fill: parent
                contentWidth: availableWidth
                contentHeight: Math.max(availableHeight, emptyContent.implicitHeight)
                clip: true
                ColumnLayout {
                    id: emptyContent
                    x: (emptyScroll.availableWidth - width) / 2
                    y: Math.max(0, (emptyScroll.availableHeight - implicitHeight) / 2)
                    width: Math.min(emptyScroll.availableWidth, 440)
                    spacing: page.shortWindow ? 10 : 14
                    Label {
                        Layout.fillWidth: true
                        text: qsTr("No computers found")
                        font.pixelSize: page.televisionMode ? 28 : 22
                        font.weight: Font.DemiBold
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                    }
                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Open Sunshine on your computer. Hosts on your local network appear here automatically.")
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        color: Theme.muted
                        lineHeight: page.shortWindow ? 1.15 : 1.3
                    }
                    OneButton {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.topMargin: 4
                        text: qsTr("Add a computer manually")
                        symbol: "plus"
                        onClicked: addComputerDialog.open()
                    }
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: page.computerModel.count > 0
            spacing: 16
            RowLayout {
                Layout.fillWidth: true
                Label {
                    text: qsTr("Instances")
                    color: Theme.muted
                    font.pixelSize: Theme.television ? 14 : 11
                    font.letterSpacing: 0
                }
                Label {
                    text: page.computerModel.count
                    color: Theme.muted
                    font.pixelSize: Theme.television ? 14 : 12
                }
                Item {
                    Layout.fillWidth: true
                }
            }
            GridView {
                id: hostGrid
                objectName: "hostGrid"
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                readonly property int columns: Math.max(1, Math.floor((width + 16) / (page.televisionMode ? 340 : 280)))
                cellWidth: width / columns
                cellHeight: Math.min(page.televisionMode ? 354 : 328, height)
                model: page.computerModel
                ScrollBar.vertical: ScrollBar {}
                delegate: Surface {
                    id: hostCard
                    required property int index
                    required property string displayName
                    required property string stableId
                    required property string address
                    required property int port
                    required property bool online
                    required property bool paired
                    required property bool pairing
                    required property string status
                    readonly property string endpoint: (address.indexOf(":") >= 0 ? "[" + address + "]" : address) + ":" + port
                    width: hostGrid.cellWidth - (hostGrid.columns > 1 ? 14 : 0)
                    height: hostGrid.cellHeight - 16
                    padding: page.televisionMode ? 24 : 20
                    background: Rectangle {
                        radius: Theme.radius
                        color: Theme.surface
                        border.color: hostAction.activeFocus ? Theme.accent : Theme.border
                        border.width: hostAction.activeFocus ? 2 : 1
                    }
                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 9
                        RowLayout {
                            Layout.fillWidth: true
                            StatusBadge {
                                text: hostCard.pairing ? qsTr("Pairing…") : hostCard.online ? (hostCard.paired ? qsTr("Online") : qsTr("Unpaired")) : hostCard.status
                                tone: hostCard.online ? (hostCard.paired ? Theme.accent : Theme.warning) : Theme.muted
                            }
                            Item {
                                Layout.fillWidth: true
                            }
                            OneIcon {
                                name: "check"
                                color: Theme.accent
                                visible: hostCard.paired
                                Accessible.name: qsTr("Paired")
                            }
                        }
                        HostArtwork {
                            visible: hostGrid.height >= 270
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Layout.minimumHeight: 74
                            online: hostCard.online
                        }
                        Label {
                            Layout.fillWidth: true
                            text: hostCard.displayName
                            font.pixelSize: page.televisionMode ? 24 : 19
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                        }
                        Label {
                            Layout.fillWidth: true
                            text: hostCard.endpoint
                            color: Theme.muted
                            font.pixelSize: page.televisionMode ? 16 : 12
                            elide: Text.ElideMiddle
                        }
                        OneButton {
                            id: hostAction
                            objectName: "hostAction" + hostCard.index
                            Layout.fillWidth: true
                            Layout.topMargin: 5
                            primary: hostCard.online && hostCard.paired
                            text: hostCard.paired ? qsTr("View games") : hostCard.pairing ? qsTr("Pairing…") : qsTr("Pair")
                            symbol: hostCard.paired ? "arrow" : "plus"
                            enabled: hostCard.online && !hostCard.pairing
                            Accessible.name: text + " " + hostCard.displayName + " " + hostCard.endpoint
                            onClicked: {
                                if (hostCard.paired) {
                                    if (page.computerModel.openHost(hostCard.index))
                                        page.gamesRequested(hostCard.displayName, hostCard.endpoint);
                                } else {
                                    page.beginPairing(hostCard.index, hostCard.stableId, hostCard.endpoint);
                                }
                            }
                            Keys.onRightPressed: hostGrid.focusAction(hostCard.index + 1)
                            Keys.onLeftPressed: {
                                if (hostCard.index % hostGrid.columns === 0)
                                    page.navigationRequested();
                                else
                                    hostGrid.focusAction(hostCard.index - 1);
                            }
                            Keys.onDownPressed: hostGrid.focusAction(hostCard.index + hostGrid.columns)
                            Keys.onUpPressed: {
                                if (hostCard.index < hostGrid.columns)
                                    addComputerButton.forceActiveFocus();
                                else
                                    hostGrid.focusAction(hostCard.index - hostGrid.columns);
                            }
                        }
                    }
                    function focusAction() {
                        hostAction.forceActiveFocus();
                    }
                }
                function focusAction(index) {
                    if (index < 0 || index >= count)
                        return;
                    positionViewAtIndex(index, GridView.Contain);
                    const card = itemAtIndex(index);
                    if (card)
                        card.focusAction();
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            OneIcon {
                name: "info"
                color: Theme.muted
                implicitWidth: 16
                implicitHeight: 16
            }
            Label {
                Layout.fillWidth: true
                text: qsTr("Can’t see your computer? Add it using its hostname or IP address.")
                color: Theme.muted
                font.pixelSize: page.televisionMode ? 16 : 12
                wrapMode: Text.WordWrap
            }
        }
    }

    Connections {
        target: page.computerModel
        function onPairingCompleted(stableId, success, error) {
            if (stableId !== pairingDialog.activeHostId)
                return;
            pairingDialog.pending = false;
            pairingDialog.resultText = success ? qsTr("Pairing complete.") : error;
        }
    }
    OneDialog {
        id: pairingDialog
        objectName: "pairingDialog"
        property string activeHostId: ""
        property string instanceEndpoint: ""
        property string pin: ""
        property bool pending: false
        property string resultText: ""
        title: qsTr("Pair computer")
        standardButtons: Dialog.Close
        contentItem: ColumnLayout {
            spacing: 18
            Label {
                Layout.fillWidth: true
                text: pairingDialog.instanceEndpoint
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WrapAnywhere
            }
            Label {
                Layout.fillWidth: true
                visible: pairingDialog.pending
                text: qsTr("Enter this PIN in Sunshine to approve this computer.")
                color: Theme.muted
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: page.televisionMode ? 110 : 90
                visible: pairingDialog.pending
                radius: 12
                color: Theme.accentSoft
                Label {
                    anchors.centerIn: parent
                    text: pairingDialog.pin
                    color: Theme.accent
                    font.pixelSize: page.televisionMode ? 48 : 40
                    font.weight: Font.DemiBold
                    font.letterSpacing: 12
                }
            }
            BusyIndicator {
                Layout.alignment: Qt.AlignHCenter
                visible: pairingDialog.pending
                running: visible
                implicitWidth: 36
                implicitHeight: 36
            }
            Label {
                Layout.fillWidth: true
                visible: !pairingDialog.pending && pairingDialog.resultText.length > 0
                text: pairingDialog.resultText
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }
        }
    }
    OneDialog {
        id: addComputerDialog
        objectName: "addComputerDialog"
        title: qsTr("Add a computer")
        standardButtons: Dialog.Ok | Dialog.Cancel
        onOpened: {
            standardButton(Dialog.Ok).text = qsTr("Add computer");
            standardButton(Dialog.Ok).primary = true;
            standardButton(Dialog.Ok).enabled = hostAddress.text.trim().length > 0;
            hostAddress.forceActiveFocus();
        }
        onAccepted: {
            if (page.computerModel.addManualHost(hostAddress.text))
                hostAddress.clear();
        }
        contentItem: ColumnLayout {
            spacing: 12
            Label {
                Layout.fillWidth: true
                text: qsTr("Connect directly to a computer running Sunshine.")
                color: Theme.muted
                wrapMode: Text.WordWrap
            }
            Label {
                text: qsTr("Hostname or IP address (optional :port)")
                Layout.topMargin: 8
            }
            TextField {
                id: hostAddress
                objectName: "hostAddress"
                Layout.fillWidth: true
                implicitHeight: Theme.controlHeight
                padding: 12
                placeholderText: qsTr("192.168.1.100:47989")
                Accessible.name: qsTr("Hostname or IP address (optional :port)")
                inputMethodHints: Qt.ImhUrlCharactersOnly
                selectByMouse: true
                background: Rectangle {
                    radius: 8
                    color: Theme.background
                    border.color: hostAddress.activeFocus ? Theme.accent : Theme.border
                    border.width: hostAddress.activeFocus ? 2 : 1
                }
                onAccepted: {
                    if (text.trim().length > 0)
                        addComputerDialog.accept();
                }
                onTextChanged: {
                    if (addComputerDialog.visible)
                        addComputerDialog.standardButton(Dialog.Ok).enabled = text.trim().length > 0;
                }
            }
        }
    }
}

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
    readonly property bool compact: width < 600
    readonly property bool shortWindow: height < 500
    readonly property bool dense: height < (televisionMode ? 680 : 600)
    readonly property int avatarSize: shortWindow ? 124 : compact ? 156 : televisionMode ? (dense ? 168 : 216) : 192
    readonly property int carouselControlWidth: Theme.controlHeight + 16
    readonly property var selectedHost: hostList.currentItem
    padding: dense ? 16 : 24
    background: HostBackdrop {}
    StackView.onActivated: Qt.callLater(page.focusPrimaryAction)

    function focusPrimaryAction() {
        if (computerModel.count > 0)
            hostList.focusHost(Math.max(0, hostList.currentIndex));
        else
            addComputerButton.forceActiveFocus();
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
    function activateHost(host) {
        if (!host || !host.online || host.pairing)
            return;
        if (host.paired) {
            if (page.computerModel.openHost(host.index))
                page.gamesRequested(host.displayName, host.endpoint);
        } else {
            page.beginPairing(host.index, host.stableId, host.endpoint);
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: page.shortWindow ? 8 : page.dense ? 10 : 18
        Item {
            Layout.fillHeight: true
            Layout.maximumHeight: page.dense ? 0 : 60
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8
            Label {
                Layout.fillWidth: true
                text: page.computerModel.count > 0 ? qsTr("Choose a computer") : qsTr("No computers yet")
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: page.shortWindow ? 26 : page.compact ? 28 : 36
                font.weight: Font.DemiBold
            }
            Label {
                Layout.fillWidth: true
                text: page.computerModel.count === 1 ? qsTr("1 saved computer") : page.computerModel.count > 0 ? qsTr("%1 saved computers").arg(page.computerModel.count) : qsTr("Open Sunshine to discover a computer, or add one below.")
                horizontalAlignment: Text.AlignHCenter
                color: Theme.muted
                font.pixelSize: page.televisionMode ? 18 : 13
                wrapMode: Text.WordWrap
            }
        }

        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: page.compact ? 4 : 12
            visible: page.computerModel.count > 0
            OneButton {
                objectName: "previousHost"
                Layout.preferredWidth: page.carouselControlWidth
                Layout.alignment: Qt.AlignTop
                Layout.topMargin: (page.avatarSize - Theme.controlHeight) / 2
                visible: hostList.overflow
                enabled: hostList.currentIndex > 0
                symbol: "back"
                quiet: true
                Accessible.name: qsTr("Previous computer")
                onClicked: hostList.focusHost(hostList.currentIndex - 1)
            }
            ListView {
                id: hostList
                objectName: "hostGrid"
                readonly property int tileWidth: page.avatarSize + (page.compact ? 32 : 52)
                readonly property bool overflow: page.computerModel.count * tileWidth > page.availableWidth
                Layout.preferredWidth: page.compact && overflow ? tileWidth : Math.max(1, Math.min(page.availableWidth - (overflow ? 2 * (page.carouselControlWidth + 12) : 0), page.computerModel.count * tileWidth))
                Layout.preferredHeight: page.avatarSize + (page.televisionMode ? 100 : 86)
                orientation: ListView.Horizontal
                clip: true
                model: page.computerModel
                currentIndex: 0
                highlightFollowsCurrentItem: false
                snapMode: ListView.SnapToItem
                boundsBehavior: Flickable.StopAtBounds
                onMovementEnded: {
                    const row = indexAt(contentX + width / 2, height / 2);
                    if (row >= 0)
                        currentIndex = row;
                    keepSelectionVisible();
                }
                header: Item {
                    width: hostList.overflow ? Math.max(0, (hostList.width - hostList.tileWidth) / 2) : 0
                    height: 1
                }
                footer: Item {
                    width: hostList.overflow ? Math.max(0, (hostList.width - hostList.tileWidth) / 2) : 0
                    height: 1
                }
                onWidthChanged: Qt.callLater(keepSelectionVisible)
                onCountChanged: {
                    if (count > 0 && (currentIndex < 0 || currentIndex >= count))
                        currentIndex = 0;
                    Qt.callLater(keepSelectionVisible);
                }
                function keepSelectionVisible() {
                    if (currentIndex >= 0 && currentIndex < count)
                        positionViewAtIndex(currentIndex, overflow ? ListView.Center : ListView.Contain);
                }
                function focusHost(row) {
                    if (row < 0 || row >= count)
                        return;
                    currentIndex = row;
                    keepSelectionVisible();
                    Qt.callLater(function () {
                        const host = hostList.itemAtIndex(row);
                        if (host)
                            host.forceActiveFocus();
                    });
                }
                delegate: Button {
                    id: host
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
                    readonly property string statusText: pairing ? qsTr("Pairing…") : online ? paired ? qsTr("Online") : qsTr("Unpaired") : status
                    objectName: "hostAction" + index
                    width: hostList.tileWidth
                    height: hostList.height
                    padding: 0
                    focusPolicy: Qt.StrongFocus
                    hoverEnabled: true
                    text: displayName
                    Accessible.name: displayName + ", " + statusText + ", " + endpoint
                    Accessible.description: online ? paired ? qsTr("Open game library") : qsTr("Pair with this computer") : qsTr("Computer is offline. It remains saved.")
                    onClicked: {
                        hostList.currentIndex = index;
                        page.activateHost(host);
                    }
                    onActiveFocusChanged: {
                        if (activeFocus) {
                            hostList.currentIndex = index;
                            hostList.keepSelectionVisible();
                        }
                    }
                    Keys.onLeftPressed: hostList.focusHost(index - 1)
                    Keys.onRightPressed: hostList.focusHost(index + 1)
                    Keys.onUpPressed: page.navigationRequested()
                    Keys.onDownPressed: {
                        if (selectedHostAction.enabled)
                            selectedHostAction.forceActiveFocus();
                        else
                            addComputerButton.forceActiveFocus();
                    }
                    Keys.onPressed: event => {
                        if (event.key === Qt.Key_Home || event.key === Qt.Key_End) {
                            hostList.focusHost(event.key === Qt.Key_Home ? 0 : hostList.count - 1);
                            event.accepted = true;
                        }
                    }
                    background: Item {}
                    contentItem: ColumnLayout {
                        spacing: 6
                        HostAvatar {
                            objectName: "hostAvatar" + host.index
                            Layout.alignment: Qt.AlignHCenter
                            Layout.preferredWidth: page.avatarSize
                            Layout.preferredHeight: page.avatarSize
                            identity: host.stableId.length > 0 ? host.stableId + "|" + host.port : host.endpoint
                            online: host.online
                            paired: host.paired
                            selected: hostList.currentIndex === host.index
                        }
                        Label {
                            Layout.fillWidth: true
                            Layout.leftMargin: 8
                            Layout.rightMargin: 8
                            text: host.displayName
                            font.pixelSize: page.televisionMode ? 23 : 18
                            font.weight: Font.DemiBold
                            horizontalAlignment: Text.AlignHCenter
                            elide: Text.ElideRight
                        }
                        RowLayout {
                            Layout.alignment: Qt.AlignHCenter
                            spacing: 6
                            Rectangle {
                                implicitWidth: 6
                                implicitHeight: 6
                                radius: 3
                                color: host.online ? host.paired ? Theme.accent : Theme.warning : Theme.muted
                            }
                            Label {
                                text: host.statusText
                                color: host.online && !host.paired ? Theme.warning : Theme.muted
                                font.pixelSize: page.televisionMode ? 16 : 12
                            }
                        }
                        Label {
                            Layout.fillWidth: true
                            Layout.leftMargin: 8
                            Layout.rightMargin: 8
                            text: host.endpoint
                            color: Theme.muted
                            font.pixelSize: page.televisionMode ? 14 : 11
                            horizontalAlignment: Text.AlignHCenter
                            elide: Text.ElideMiddle
                        }
                    }
                    ToolTip.visible: hovered
                    ToolTip.delay: 800
                    ToolTip.text: displayName + "\n" + endpoint
                }
            }
            OneButton {
                objectName: "nextHost"
                Layout.preferredWidth: page.carouselControlWidth
                Layout.alignment: Qt.AlignTop
                Layout.topMargin: (page.avatarSize - Theme.controlHeight) / 2
                visible: hostList.overflow
                enabled: hostList.currentIndex + 1 < page.computerModel.count
                symbol: "arrow"
                quiet: true
                Accessible.name: qsTr("Next computer")
                onClicked: hostList.focusHost(hostList.currentIndex + 1)
            }
        }
        HostAvatar {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: page.avatarSize
            Layout.preferredHeight: page.avatarSize
            visible: page.computerModel.count === 0
            online: false
        }
        OneButton {
            id: selectedHostAction
            objectName: "selectedHostAction"
            Layout.alignment: Qt.AlignHCenter
            Layout.minimumWidth: page.televisionMode ? 220 : 180
            visible: page.computerModel.count > 0
            enabled: page.selectedHost !== null && page.selectedHost.online && !page.selectedHost.pairing
            primary: enabled
            text: !page.selectedHost ? "" : !page.selectedHost.online ? page.selectedHost.status : page.selectedHost.pairing ? qsTr("Pairing…") : page.selectedHost.paired ? qsTr("View games") : qsTr("Pair computer")
            symbol: page.selectedHost && page.selectedHost.online ? page.selectedHost.paired ? "arrow" : "plus" : ""
            Accessible.name: text + (page.selectedHost ? " " + page.selectedHost.displayName + " " + page.selectedHost.endpoint : "")
            onClicked: page.activateHost(page.selectedHost)
            Keys.onUpPressed: hostList.focusHost(hostList.currentIndex)
            Keys.onDownPressed: addComputerButton.forceActiveFocus()
        }
        Item {
            Layout.fillHeight: true
            Layout.maximumHeight: page.dense ? 0 : 60
        }
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 8
            OneButton {
                id: addComputerButton
                objectName: "addComputerButton"
                text: qsTr("Add computer")
                symbol: "plus"
                quiet: true
                focus: page.computerModel.count === 0
                onClicked: addComputerDialog.open()
                Keys.onUpPressed: page.focusPrimaryAction()
                Keys.onDownPressed: page.navigationRequested()
                Keys.onRightPressed: refreshButton.forceActiveFocus()
            }
            OneButton {
                id: refreshButton
                objectName: "refreshComputers"
                text: qsTr("Refresh")
                symbol: "refresh"
                quiet: true
                onClicked: page.computerModel.refresh()
                Keys.onUpPressed: page.focusPrimaryAction()
                Keys.onDownPressed: page.navigationRequested()
                Keys.onLeftPressed: addComputerButton.forceActiveFocus()
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

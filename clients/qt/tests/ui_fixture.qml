import QtQuick

// Isolated UI data: these objects never discover, pair, or launch a real host.
QtObject {
    property QtObject environment: QtObject {
        property string mode: "desktop"
        property string platform: "windows"
    }
    property ListModel computers: ListModel {
        property int openedRow: -1
        property int pairedRow: -1
        property string addedAddress: ""
        property int refreshCount: 0
        function refresh() {
            refreshCount++;
        }
        signal pairingCompleted(string stableId, bool success, string error)
        function openHost(row) {
            openedRow = row;
            return true;
        }
        function pairHost(row) {
            pairedRow = row;
            return "1234";
        }
        function addManualHost(address) {
            addedAddress = address;
            return true;
        }
        ListElement {
            displayName: "Living room PC"
            stableId: "living-room"
            address: "192.168.1.105"
            port: 47989
            online: true
            paired: true
            pairing: false
            status: "Online"
        }
        ListElement {
            displayName: "Living room PC"
            stableId: "studio"
            address: "192.168.1.105"
            port: 48089
            online: true
            paired: false
            pairing: false
            status: "Online"
        }
        ListElement {
            displayName: "Gaming laptop"
            stableId: "laptop"
            address: "192.168.1.112"
            port: 47989
            online: false
            paired: true
            pairing: false
            status: "Offline"
        }
    }
    property ListModel applications: ListModel {
        property bool pairingRequired: false
        property bool loading: false
        property string error: ""
        property int refreshCount: 0
        property int artworkRequests: 0
        property int retriedArtwork: -1
        function retryArtwork(applicationId) {
            retriedArtwork = applicationId;
        }
        function setArtwork(row, source, error) {
            setProperty(row, "artwork", source);
            setProperty(row, "artworkError", error);
        }
        function requestArtwork(applicationId) {
            artworkRequests++;
        }
        function refresh() {
            refreshCount++;
        }
        ListElement {
            applicationId: 1
            artwork: ""
            artworkError: ""
            title: "Desktop"
            hdrSupported: false
            appCollectorGame: false
        }
        ListElement {
            applicationId: 2
            artwork: ""
            artworkError: ""
            title: "Steam Big Picture"
            hdrSupported: false
            appCollectorGame: false
        }
        ListElement {
            applicationId: 3
            artwork: ""
            artworkError: ""
            title: "A game with a deliberately long title to check wrapping"
            hdrSupported: true
            appCollectorGame: true
        }
        ListElement {
            applicationId: 4
            artwork: ""
            artworkError: ""
            title: "Playnite"
            hdrSupported: false
            appCollectorGame: false
        }
    }
    property QtObject streaming: QtObject {
        property bool available: true
        property bool busy: false
        property bool streaming: false
        property string unavailableReason: "Streaming is not available on this device."
        property string status: "Connecting to Living room PC…"
        property string error: ""
        property int controllerCount: 1
        property int startedId: -1
        property int stopCount: 0
        function startApplication(id, title, output) {
            startedId = id;
            busy = true;
        }
        function stop() {
            stopCount++;
            busy = false;
        }
    }
}

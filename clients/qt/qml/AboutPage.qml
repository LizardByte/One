import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Page {
    id: page
    required property bool televisionMode
    signal navigationRequested
    function focusPrimaryAction() {
        aboutScroll.forceActiveFocus();
    }
    padding: televisionMode ? 32 : 24
    background: Rectangle {
        color: Theme.background
    }
    ScrollView {
        id: aboutScroll
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true
        Keys.onLeftPressed: page.navigationRequested()
        Keys.onEscapePressed: page.navigationRequested()
        ColumnLayout {
            width: parent.width
            spacing: 18
            Label {
                text: qsTr("About One")
                font.pixelSize: Theme.titleSize
                font.weight: Font.DemiBold
            }
            Label {
                text: qsTr("Version %1").arg(Qt.application.version)
                color: Theme.muted
            }
            Label {
                Layout.fillWidth: true
                Layout.maximumWidth: 640
                text: qsTr("One is a game-streaming client for Sunshine.")
                wrapMode: Text.WordWrap
            }
            Label {
                Layout.fillWidth: true
                Layout.maximumWidth: 640
                text: qsTr("A LizardByte project. Licensed under the GNU General Public License version 3.")
                wrapMode: Text.WordWrap
                color: Theme.muted
            }
        }
    }
}

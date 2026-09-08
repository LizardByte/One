pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Page {
    id: page
    required property bool televisionMode
    signal navigationRequested
    function focusPrimaryAction() {
        appearanceChoices.itemAt(Theme.appearance).forceActiveFocus();
    }
    padding: televisionMode ? 32 : 24
    background: Rectangle {
        color: Theme.background
    }

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true
        ColumnLayout {
            width: parent.width
            spacing: 24
            Label {
                text: qsTr("Settings")
                font.pixelSize: Theme.titleSize
                font.weight: Font.DemiBold
            }
            Surface {
                Layout.fillWidth: true
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 14
                    Label {
                        text: qsTr("Appearance")
                        font.pixelSize: Theme.television ? 24 : 18
                        font.weight: Font.DemiBold
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        Repeater {
                            id: appearanceChoices
                            model: [qsTr("Dark"), qsTr("Light"), qsTr("System")]
                            delegate: OneButton {
                                required property int index
                                required property string modelData
                                objectName: "appearance" + index
                                Layout.fillWidth: true
                                text: modelData
                                checkable: true
                                checked: Theme.appearance === index
                                onClicked: Theme.appearance = index
                                Keys.onUpPressed: page.navigationRequested()
                                Keys.onLeftPressed: {
                                    if (index > 0)
                                        appearanceChoices.itemAt(index - 1).forceActiveFocus();
                                    else
                                        page.navigationRequested();
                                }
                                Keys.onRightPressed: {
                                    if (index < 2)
                                        appearanceChoices.itemAt(index + 1).forceActiveFocus();
                                }
                            }
                        }
                    }
                }
            }
            Surface {
                Layout.fillWidth: true
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 14
                    Label {
                        text: qsTr("Streaming")
                        font.pixelSize: Theme.television ? 24 : 18
                        font.weight: Font.DemiBold
                    }
                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Current PC defaults. Quality settings are not configurable yet.")
                        color: Theme.muted
                        wrapMode: Text.WordWrap
                    }
                    Repeater {
                        model: [
                            {
                                label: qsTr("Resolution"),
                                value: qsTr("1920 × 1080")
                            },
                            {
                                label: qsTr("Frame rate"),
                                value: qsTr("60 FPS")
                            },
                            {
                                label: qsTr("Video format"),
                                value: qsTr("H.264 · SDR")
                            }
                        ]
                        delegate: RowLayout {
                            required property var modelData
                            Layout.fillWidth: true
                            Label {
                                text: parent.modelData.label
                                color: Theme.muted
                                Layout.fillWidth: true
                            }
                            Label {
                                text: parent.modelData.value
                            }
                        }
                    }
                }
            }
            Surface {
                Layout.fillWidth: true
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 12
                    Label {
                        text: qsTr("Keyboard and mouse")
                        font.pixelSize: Theme.television ? 24 : 18
                        font.weight: Font.DemiBold
                    }
                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Click the video to capture input. Press Ctrl + Alt + Shift + Z to release it.")
                        wrapMode: Text.WordWrap
                        color: Theme.muted
                    }
                }
            }
        }
    }
}

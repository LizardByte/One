pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Pane {
    id: navigationRoot
    property bool vertical: false
    property int currentIndex: 0
    signal activated(int index)
    signal contentFocusRequested
    function focusCurrentItem() {
        navigationRepeater.itemAt(currentIndex).forceActiveFocus();
    }
    padding: vertical ? 16 : 10
    background: Rectangle {
        color: Theme.sidebar
    }

    contentItem: ColumnLayout {
        spacing: 38
        RowLayout {
            visible: navigationRoot.vertical
            Layout.topMargin: 14
            Layout.leftMargin: 12
            spacing: 12
            Label {
                text: qsTr("One")
                font.pixelSize: 30
                font.weight: Font.DemiBold
                font.letterSpacing: 0
            }
        }
        GridLayout {
            columns: navigationRoot.vertical ? 1 : 3
            uniformCellWidths: true
            Layout.fillWidth: true
            columnSpacing: 8
            rowSpacing: 8
            Repeater {
                id: navigationRepeater
                model: [
                    {
                        title: qsTr("Computers"),
                        symbol: "computer"
                    },
                    {
                        title: qsTr("Settings"),
                        symbol: "settings"
                    },
                    {
                        title: qsTr("About"),
                        symbol: "info"
                    }
                ]
                delegate: Button {
                    id: navButton
                    required property int index
                    required property var modelData
                    objectName: "navigation" + index
                    Layout.fillWidth: true
                    implicitHeight: Theme.television ? 60 : navigationRoot.vertical ? 52 : 58
                    padding: 12
                    text: modelData.title
                    checked: index === navigationRoot.currentIndex
                    focusPolicy: Qt.StrongFocus
                    hoverEnabled: true
                    onClicked: navigationRoot.activated(index)
                    background: Rectangle {
                        radius: 10
                        color: navButton.checked ? Theme.accentSoft : navButton.hovered ? Theme.elevated : "transparent"
                        border.color: navButton.activeFocus ? Theme.accent : "transparent"
                        border.width: 2
                    }
                    contentItem: GridLayout {
                        columns: navigationRoot.vertical || Theme.television ? 2 : 1
                        rowSpacing: 4
                        columnSpacing: 12
                        OneIcon {
                            Layout.alignment: Qt.AlignHCenter
                            name: navButton.modelData.symbol
                            color: navButton.checked ? Theme.accent : Theme.muted
                        }
                        Label {
                            Layout.fillWidth: true
                            text: navButton.text
                            color: navButton.checked ? Theme.accent : Theme.muted
                            font.pixelSize: Theme.television ? 20 : navigationRoot.vertical ? 14 : 11
                            font.weight: navButton.checked ? Font.DemiBold : Font.Normal
                            horizontalAlignment: navigationRoot.vertical ? Text.AlignLeft : Text.AlignHCenter
                        }
                    }
                    Keys.onLeftPressed: {
                        if (!navigationRoot.vertical && index > 0)
                            navigationRepeater.itemAt(index - 1).forceActiveFocus();
                    }
                    Keys.onRightPressed: {
                        if (navigationRoot.vertical)
                            navigationRoot.contentFocusRequested();
                        else if (index + 1 < navigationRepeater.count)
                            navigationRepeater.itemAt(index + 1).forceActiveFocus();
                    }
                    Keys.onUpPressed: {
                        if (navigationRoot.vertical && index > 0)
                            navigationRepeater.itemAt(index - 1).forceActiveFocus();
                    }
                    Keys.onDownPressed: {
                        if (!navigationRoot.vertical)
                            navigationRoot.contentFocusRequested();
                        else if (index + 1 < navigationRepeater.count)
                            navigationRepeater.itemAt(index + 1).forceActiveFocus();
                    }
                }
            }
        }
        Item {
            visible: navigationRoot.vertical
            Layout.fillHeight: true
        }
    }
}

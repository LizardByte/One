import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Frame {
    id: navigationRoot

    property bool vertical: false
    property int currentIndex: 0
    signal activated(int index)

    padding: 10

    contentItem: GridLayout {
        columns: navigationRoot.vertical ? 1 : 3
        rows: navigationRoot.vertical ? 3 : 1
        columnSpacing: 8
        rowSpacing: 8

        Repeater {
            id: navigationRepeater
            model: [qsTr("Computers"), qsTr("Settings"), qsTr("About")]

            delegate: Button {
                required property int index
                required property string modelData

                Layout.fillWidth: true
                Layout.fillHeight: navigationRoot.vertical
                Layout.minimumHeight: navigationRoot.vertical ? 56 : 48
                text: modelData
                checkable: true
                checked: index === navigationRoot.currentIndex
                focusPolicy: Qt.StrongFocus
                onClicked: navigationRoot.activated(index)

                Keys.onLeftPressed: {
                    if (!navigationRoot.vertical && index > 0)
                        navigationRepeater.itemAt(index - 1).forceActiveFocus()
                }
                Keys.onRightPressed: {
                    if (!navigationRoot.vertical && index + 1 < navigationRepeater.count)
                        navigationRepeater.itemAt(index + 1).forceActiveFocus()
                }
                Keys.onUpPressed: {
                    if (navigationRoot.vertical && index > 0)
                        navigationRepeater.itemAt(index - 1).forceActiveFocus()
                }
                Keys.onDownPressed: {
                    if (navigationRoot.vertical && index + 1 < navigationRepeater.count)
                        navigationRepeater.itemAt(index + 1).forceActiveFocus()
                }
            }
        }
    }
}


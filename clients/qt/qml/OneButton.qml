import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Button {
    id: control
    property bool primary: false
    property bool quiet: false
    property string symbol: ""
    readonly property color foreground: !enabled ? Theme.muted : primary ? Theme.accentText : Theme.text
    implicitHeight: Theme.controlHeight
    implicitWidth: contentItem.implicitWidth + leftPadding + rightPadding
    leftPadding: 16
    rightPadding: 16
    focusPolicy: Qt.StrongFocus
    hoverEnabled: true
    font.pixelSize: Theme.bodySize
    Accessible.name: text
    contentItem: RowLayout {
        spacing: 9
        OneIcon {
            visible: control.symbol.length > 0
            name: control.symbol
            color: control.foreground
            Layout.preferredWidth: Theme.television ? 24 : 19
            Layout.preferredHeight: Layout.preferredWidth
        }
        Text {
            text: control.text
            font: control.font
            color: control.foreground
            horizontalAlignment: Text.AlignHCenter
            Layout.fillWidth: true
        }
    }
    background: Rectangle {
        radius: 9
        color: !control.enabled ? Theme.elevated : control.primary ? (control.hovered || control.down ? Theme.accentHover : Theme.accent) : control.hovered || control.down || control.checked ? Theme.elevated : control.quiet ? "transparent" : Theme.surface
        border.color: control.activeFocus ? Theme.accent : control.primary || control.quiet ? "transparent" : Theme.border
        border.width: control.activeFocus ? 2 : 1
        Rectangle {
            anchors.fill: parent
            anchors.margins: -2
            radius: 11
            color: "transparent"
            border.width: 1
            border.color: Theme.accent
            visible: control.activeFocus
        }
        Behavior on color {
            ColorAnimation {
                duration: 120
            }
        }
    }
}

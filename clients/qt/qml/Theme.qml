pragma Singleton

import QtQuick
import QtCore

QtObject {
    id: theme

    // The preference belongs to the shared interface, not a platform backend.
    property Settings preferences: Settings {
        id: appearanceSettings
        category: "Appearance"
        property int appearance: 0
    }
    property alias appearance: appearanceSettings.appearance
    property bool television: false
    readonly property bool dark: appearance === 0 || (appearance === 2 && Qt.styleHints.colorScheme === Qt.Dark)
    readonly property color background: dark ? "#101518" : "#F3F6F5"
    readonly property color sidebar: dark ? "#141B1E" : "#FFFFFF"
    readonly property color surface: dark ? "#1A2327" : "#FFFFFF"
    readonly property color elevated: dark ? "#222E33" : "#E8EFEC"
    readonly property color border: dark ? "#303E43" : "#D5DFDB"
    readonly property color text: dark ? "#F2F6F4" : "#192B26"
    readonly property color muted: dark ? "#A6B6B4" : "#536961"
    readonly property color accent: dark ? "#8DE0BB" : "#246C50"
    readonly property color accentHover: dark ? "#ADF2D3" : "#1B583F"
    readonly property color accentText: dark ? "#122C21" : "#FFFFFF"
    readonly property color accentSoft: dark ? "#263F36" : "#DBEEE3"
    readonly property color warning: dark ? "#EAC28A" : "#795014"
    readonly property color danger: dark ? "#FFAAA3" : "#A32E28"
    readonly property int bodySize: television ? 20 : 14
    readonly property int titleSize: television ? 38 : 32
    readonly property int controlHeight: television ? 56 : 44
    readonly property int radius: 14
}

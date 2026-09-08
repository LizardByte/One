pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import One

ApplicationWindow {
    id: root

    readonly property bool desktopMode: UiEnvironment.mode === "desktop"
    readonly property bool touchMode: UiEnvironment.mode === "touch"
    readonly property bool televisionMode: UiEnvironment.mode === "television"
    property int currentPage: 0
    // Navigation responds to the screen, never to the page being displayed.
    readonly property bool sidebarVisible: desktopMode && width >= 900
    function focusPage() {
        const page = currentPage === 0 ? computersStack.currentItem : currentPage === 1 ? settingsPage : aboutPage;
        page.focusPrimaryAction();
    }
    function focusNavigation() {
        const navigation = sidebarVisible ? desktopNavigation : televisionMode ? televisionNavigation : touchNavigation;
        navigation.focusCurrentItem();
    }
    function selectPage(index) {
        currentPage = index;
        Qt.callLater(root.focusNavigation);
    }

    width: televisionMode ? 1280 : (touchMode ? 430 : 1200)
    height: televisionMode ? 720 : (touchMode ? 820 : 800)
    minimumWidth: touchMode ? 360 : 800
    minimumHeight: 600
    visible: true
    title: qsTr("One")
    color: Theme.background
    font.pixelSize: Theme.bodySize
    font.family: Qt.platform.os === "windows" ? "Segoe UI" : Qt.platform.os === "osx" || Qt.platform.os === "ios" ? "Helvetica Neue" : "sans-serif"
    palette.window: Theme.background
    palette.windowText: Theme.text
    palette.base: Theme.surface
    palette.alternateBase: Theme.elevated
    palette.text: Theme.text
    palette.button: Theme.elevated
    palette.buttonText: Theme.text
    palette.highlight: Theme.accent
    palette.highlightedText: Theme.accentText
    palette.mid: Theme.border
    palette.dark: Theme.border
    palette.light: Theme.elevated
    palette.placeholderText: Theme.muted
    Component.onCompleted: Theme.television = televisionMode

    Component {
        id: gamesPageComponent

        GamesPage {
            onNavigationRequested: root.focusNavigation()
            televisionMode: root.televisionMode
            applicationModel: ApplicationModel
            streamingController: StreamingController
            onBackRequested: computersStack.pop()
            onStreamRequested: (applicationId, title) => computersStack.push(streamPageComponent, {
                    "applicationId": applicationId,
                    "applicationTitle": title
                })
        }
    }

    Component {
        id: streamPageComponent

        StreamPage {
            televisionMode: root.televisionMode
            streamingController: StreamingController
            onBackRequested: computersStack.pop()
        }
    }

    header: ToolBar {
        visible: !root.sidebarVisible && !StreamingController.busy
        implicitHeight: root.televisionMode ? 80 : 60
        background: Rectangle {
            color: Theme.sidebar
        }
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 20
            anchors.rightMargin: 20

            Label {
                text: qsTr("One")
                font.pixelSize: root.televisionMode ? 28 : 22
                font.bold: true
            }

            Item {
                Layout.fillWidth: true
            }

            AppNavigation {
                id: televisionNavigation
                onContentFocusRequested: root.focusPage()
                visible: root.televisionMode
                Layout.preferredWidth: 650
                currentIndex: root.currentPage
                onActivated: index => root.selectPage(index)
            }

            Item {
                visible: root.televisionMode
                Layout.fillWidth: true
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            AppNavigation {
                id: desktopNavigation
                onContentFocusRequested: root.focusPage()
                Layout.fillHeight: true
                Layout.preferredWidth: 224
                visible: root.sidebarVisible && !StreamingController.busy
                vertical: true
                currentIndex: root.currentPage
                onActivated: index => root.selectPage(index)
            }

            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: root.currentPage

                StackView {
                    id: computersStack
                    objectName: "computersStack"
                    clip: true
                    pushEnter: Transition {
                        NumberAnimation {
                            property: "opacity"
                            from: 0
                            to: 1
                            duration: 140
                        }
                    }
                    pushExit: Transition {
                        NumberAnimation {
                            property: "opacity"
                            from: 1
                            to: 0
                            duration: 140
                        }
                    }
                    popEnter: Transition {
                        NumberAnimation {
                            property: "opacity"
                            from: 0
                            to: 1
                            duration: 140
                        }
                    }
                    popExit: Transition {
                        NumberAnimation {
                            property: "opacity"
                            from: 1
                            to: 0
                            duration: 140
                        }
                    }

                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    initialItem: ComputersPage {
                        onNavigationRequested: root.focusNavigation()
                        televisionMode: root.televisionMode
                        computerModel: ComputerModel
                        onGamesRequested: (hostName, hostEndpoint) => computersStack.push(gamesPageComponent, {
                                "hostName": hostName,
                                "hostEndpoint": hostEndpoint
                            })
                    }
                }

                SettingsPage {
                    id: settingsPage
                    onNavigationRequested: root.focusNavigation()
                    televisionMode: root.televisionMode
                }

                AboutPage {
                    id: aboutPage
                    onNavigationRequested: root.focusNavigation()
                    televisionMode: root.televisionMode
                }
            }
        }

        AppNavigation {
            id: touchNavigation
            onContentFocusRequested: root.focusPage()
            Layout.fillWidth: true
            visible: !root.sidebarVisible && !root.televisionMode && !StreamingController.busy
            vertical: false
            currentIndex: root.currentPage
            onActivated: index => root.selectPage(index)
        }
    }
}

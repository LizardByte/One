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

    width: televisionMode ? 1280 : (touchMode ? 430 : 1100)
    height: televisionMode ? 720 : (touchMode ? 820 : 720)
    minimumWidth: touchMode ? 360 : 800
    minimumHeight: 600
    visible: true
    title: qsTr("One")

    Component {
        id: gamesPageComponent

        GamesPage {
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
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 20
            anchors.rightMargin: 20

            Label {
                text: qsTr("One")
                font.pixelSize: root.televisionMode ? 28 : 22
                font.bold: true
            }

            Item { Layout.fillWidth: true }

            Label {
                text: qsTr("%1 · %2").arg(UiEnvironment.platform).arg(UiEnvironment.mode)
                opacity: 0.7
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        AppNavigation {
            Layout.fillWidth: true
            visible: root.televisionMode && !StreamingController.busy
            vertical: false
            currentIndex: root.currentPage
            onActivated: index => root.currentPage = index
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            AppNavigation {
                Layout.fillHeight: true
                Layout.preferredWidth: 210
                visible: root.desktopMode && !StreamingController.busy
                vertical: true
                currentIndex: root.currentPage
                onActivated: index => root.currentPage = index
            }

            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: root.currentPage

                StackView {
                    id: computersStack

                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    initialItem: ComputersPage {
                        televisionMode: root.televisionMode
                        computerModel: ComputerModel
                        onGamesRequested: hostName => computersStack.push(gamesPageComponent, { "hostName": hostName })
                    }
                }

                SettingsPage {
                    televisionMode: root.televisionMode
                }

                AboutPage {
                    televisionMode: root.televisionMode
                }
            }
        }

        AppNavigation {
            Layout.fillWidth: true
            visible: root.touchMode && !StreamingController.busy
            vertical: false
            currentIndex: root.currentPage
            onActivated: index => root.currentPage = index
        }
    }
}

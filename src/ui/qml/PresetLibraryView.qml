import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Pane {
    // Emoji need a colour-emoji font; use plain text on platforms where it is not guaranteed.
    readonly property bool useEmoji: Qt.platform.os === "osx"
    id: root
    padding: 12

    property int lastAddedCount: 0

    ColumnLayout {
        anchors.fill: parent
        spacing: 10



        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            TextField {
                id: searchField
                placeholderText: useEmoji ? "🔍 Search presets" : "Search presets"
                text: presetLibrary.filterText
                selectByMouse: true
                Layout.fillWidth: true
                onTextEdited: presetLibrary.filterText = text
            }

            Shortcut {
                sequence: "Ctrl+F"
                onActivated: searchField.forceActiveFocus()
            }
        }

        Label {
            text: root.lastAddedCount > 0 ? root.lastAddedCount + " added to " + playlistManager.activePlaylistName : ""
            color: "#8b93a1"
            elide: Text.ElideRight
            Layout.fillWidth: true
            visible: text.length > 0
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: presetLibrary.totalCount === 0

            ColumnLayout {
                anchors.centerIn: parent
                spacing: 14
                width: Math.min(parent.width - 32, 280)

                Label {
                    text: "No presets loaded"
                    font.pixelSize: 18
                    font.bold: true
                    color: "#ffffff"
                    horizontalAlignment: Text.AlignHCenter
                    Layout.alignment: Qt.AlignHCenter
                }

                Label {
                    text: "Import a folder containing .milk files to get started."
                    color: "#b0bdd4"
                    font.pixelSize: 13
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                    Layout.fillWidth: true
                }

                Button {
                    id: emptyImportBtn
                    text: useEmoji ? "📁 Import Preset Folder" : "Import Preset Folder"
                    font.bold: true
                    font.pixelSize: 13
                    Layout.alignment: Qt.AlignHCenter
                    Layout.topMargin: 8
                    contentItem: Text {
                        text: emptyImportBtn.text
                        font: emptyImportBtn.font
                        color: "#ffffff"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: emptyImportBtn.down ? "#23477e" : (emptyImportBtn.hovered ? "#3b6bb8" : "#2e5ba6")
                        border.color: "#5b8fe0"
                        border.width: 1
                        radius: 6
                    }
                    onClicked: presetFolderDialog.open()
                }
            }
        }

        ListView {
            id: libraryList
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: presetLibrary.totalCount > 0
            model: presetLibrary
            clip: true
            spacing: 6
            keyNavigationEnabled: true

            delegate: Rectangle {
                required property int index
                required property string title
                required property string author
                required property string compatibilityStatus
                required property string compatibilityNote

                width: ListView.view.width
                height: 66
                radius: 6
                color: ListView.isCurrentItem ? "#232833" : "#171a20"
                border.color: "#2c313b"

                Rectangle {
                    width: 4
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    radius: 2
                    color: title === appController.activePresetTitle ? "#6ea8ff" : "transparent"
                }

                TapHandler {
                    onTapped: {
                        libraryList.currentIndex = index
                        appController.playPreset(index)
                    }
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 8

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Label {
                            text: title
                            color: "#f2f4f8"
                            font.pixelSize: 14
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Label {
                                text: author
                                color: "#8b93a1"
                                font.pixelSize: 12
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                                ToolTip.visible: compatibilityNote.length > 0 && hoverHandler.hovered
                                ToolTip.text: compatibilityNote
                            }

                            Rectangle {
                                width: statusLabel.width + 12
                                height: 18
                                radius: 9
                                color: compatibilityStatus === "ready" ? "#1b4f2c" : (compatibilityStatus === "failed" ? "#4f1b1b" : "#2c313b")
                                Label {
                                    id: statusLabel
                                    anchors.centerIn: parent
                                    text: compatibilityLabel(compatibilityStatus)
                                    color: compatibilityStatus === "ready" ? "#c8f2d0" : (compatibilityStatus === "failed" ? "#f2c8c8" : "#8b93a1")
                                    font.pixelSize: 10
                                    font.bold: true
                                }
                            }
                        }
                    }

                    ToolButton {
                        id: addButton
                        property bool justAdded: false
                        text: justAdded ? "Added!" : "Add"
                        visible: hoverHandler.hovered || justAdded
                        onClicked: {
                            appController.addPresetToPlaylist(index)
                            justAdded = true
                            addedTimer.start()
                        }
                        Timer {
                            id: addedTimer
                            interval: 1500
                            onTriggered: addButton.justAdded = false
                        }
                    }
                }

                HoverHandler {
                    id: hoverHandler
                }

                function compatibilityLabel(status) {
                    if (status === "failed") {
                        return "Failed"
                    }
                    if (status === "ready") {
                        return "Ready"
                    }
                    return "Untested"
                }
            }
        }

        Button {
            text: presetLibrary.count > 0 ? "Add all to playlist → (" + presetLibrary.count + ")" : "Add all to playlist →"
            enabled: presetLibrary.count > 0
            Layout.fillWidth: true
            visible: presetLibrary.totalCount > 0
            onClicked: root.lastAddedCount = appController.addVisiblePresetsToPlaylist()
        }
    }
}

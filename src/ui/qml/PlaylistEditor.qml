import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Pane {
    // Emoji need a colour-emoji font; use plain text on platforms where it is not guaranteed.
    readonly property bool useEmoji: Qt.platform.os === "osx"
    id: root
    padding: 0
    background: null

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        RowLayout {
            Layout.fillWidth: true

            Label {
                text: playlistManager.activePlaylistName.length > 0
                    ? "Editing: " + playlistManager.activePlaylistName
                    : "No playlist selected"
                color: "#a0b4d4"
                elide: Text.ElideRight
                Layout.fillWidth: true
            }

            Button {
                id: addPresetsBtn
                text: "+ Add Presets"
                font.bold: true
                enabled: playlistManager.activePlaylistName.length > 0 && presetLibrary.totalCount > 0
                contentItem: Text {
                    text: addPresetsBtn.text
                    font: addPresetsBtn.font
                    color: "#ffffff"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    color: addPresetsBtn.down ? "#23477e" : (addPresetsBtn.hovered ? "#3b6bb8" : "#2e5ba6")
                    border.color: "#5b8fe0"
                    border.width: 1
                    radius: 6
                }
                onClicked: {
                    presetLibrary.filterText = ""
                    presetPicker.open()
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true

            TextField {
                id: playlistName
                placeholderText: "New playlist name..."
                Layout.fillWidth: true
                onAccepted: {
                    playlistManager.createPlaylist(text)
                    text = ""
                }
            }

            Button {
                id: createBtn
                text: "Create"
                font.bold: true
                contentItem: Text {
                    text: createBtn.text
                    font: createBtn.font
                    color: "#ffffff"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    color: createBtn.down ? "#23477e" : (createBtn.hovered ? "#3b6bb8" : "#2e5ba6")
                    border.color: "#5b8fe0"
                    border.width: 1
                    radius: 6
                }
                onClicked: {
                    playlistManager.createPlaylist(playlistName.text)
                    playlistName.text = ""
                }
            }

            ToolButton {
                text: useEmoji ? "⚙️ Auto-DJ" : "Auto-DJ"
                onClicked: autoDjPopup.open()
            }
        }

        Popup {
            id: autoDjPopup
            padding: 20
            width: 320
            parent: Overlay.overlay
            x: Math.round((parent.width - width) / 2)
            y: Math.round((parent.height - height) / 2)
            modal: true
            
            background: Rectangle {
                color: "#1e222d"
                border.color: "#3d485c"
                border.width: 1.5
                radius: 10
            }

            ColumnLayout {
                spacing: 14
                anchors.fill: parent

                Label {
                    text: "Auto-DJ Settings"
                    font.pixelSize: 16
                    font.bold: true
                    color: "#ffffff"
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: "#323c4e"
                }

                RowLayout {
                    Layout.fillWidth: true
                    Label { text: "Playback Mode"; color: "#f0f3f8"; Layout.fillWidth: true }
                    ComboBox {
                        model: ["shuffle", "sequential"]
                        currentIndex: playlistManager.mode === "sequential" ? 1 : 0
                        onActivated: playlistManager.mode = currentText
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Label { text: "Preset Duration"; color: "#f0f3f8"; Layout.fillWidth: true }
                    Label { text: Math.round(playlistManager.presetDurationSeconds) + "s"; color: "#a0b4d4"; font.bold: true }
                }
                Slider {
                    from: 5
                    to: 180
                    stepSize: 5
                    value: playlistManager.presetDurationSeconds
                    Layout.fillWidth: true
                    onMoved: playlistManager.presetDurationSeconds = value
                }

                RowLayout {
                    Layout.fillWidth: true
                    Label { text: "Transition Duration"; color: "#f0f3f8"; Layout.fillWidth: true }
                    Label { text: playlistManager.transitionDurationSeconds.toFixed(1) + "s"; color: "#a0b4d4"; font.bold: true }
                }
                Slider {
                    from: 0
                    to: 20
                    stepSize: 0.5
                    value: playlistManager.transitionDurationSeconds
                    Layout.fillWidth: true
                    onMoved: playlistManager.transitionDurationSeconds = value
                }

                Button {
                    id: autoDjCloseBtn
                    text: "Close"
                    font.bold: true
                    Layout.alignment: Qt.AlignRight
                    contentItem: Text {
                        text: autoDjCloseBtn.text
                        font: autoDjCloseBtn.font
                        color: "#ffffff"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: autoDjCloseBtn.down ? "#23477e" : (autoDjCloseBtn.hovered ? "#3b6bb8" : "#2e5ba6")
                        border.color: "#5b8fe0"
                        border.width: 1
                        radius: 6
                    }
                    onClicked: autoDjPopup.close()
                }
            }
        }

        Popup {
            id: presetPicker
            padding: 18
            width: Math.min(520, Overlay.overlay.width - 32)
            height: Math.min(620, Overlay.overlay.height - 32)
            parent: Overlay.overlay
            x: Math.round((parent.width - width) / 2)
            y: Math.round((parent.height - height) / 2)
            modal: true
            focus: true

            background: Rectangle {
                color: "#1e222d"
                border.color: "#3d485c"
                border.width: 1.5
                radius: 10
            }

            ColumnLayout {
                anchors.fill: parent
                spacing: 10

                Label {
                    text: "Add Presets to " + playlistManager.activePlaylistName
                    color: "#ffffff"
                    font.pixelSize: 16
                    font.bold: true
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }

                TextField {
                    id: pickerSearch
                    placeholderText: "Search all presets"
                    selectByMouse: true
                    Layout.fillWidth: true
                    onTextEdited: presetLibrary.filterText = text
                }

                Label {
                    text: presetLibrary.count + " available preset" + (presetLibrary.count === 1 ? "" : "s")
                    color: "#8b93a1"
                    font.pixelSize: 12
                    Layout.fillWidth: true
                }

                ListView {
                    id: pickerList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 5
                    model: presetLibrary

                    delegate: Rectangle {
                        required property int index
                        required property string title
                        required property string author

                        width: ListView.view.width
                        height: 48
                        radius: 6
                        color: "#171a20"
                        border.color: "#2c313b"

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 8

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 1

                                Label {
                                    text: title
                                    color: "#f2f4f8"
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                                Label {
                                    text: author
                                    color: "#8b93a1"
                                    font.pixelSize: 11
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                            }

                            Button {
                                text: "Add"
                                onClicked: appController.addPresetToPlaylist(index)
                            }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true

                    Button {
                        text: "Add all shown (" + presetLibrary.count + ")"
                        enabled: presetLibrary.count > 0
                        onClicked: appController.addVisiblePresetsToPlaylist()
                    }

                    Item { Layout.fillWidth: true }

                    Button {
                        text: "Close"
                        onClicked: presetPicker.close()
                    }
                }
            }
        }

        ListView {
            id: playlistList
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: playlistManager
            clip: true
            spacing: 6

            delegate: Rectangle {
                required property int index
                required property string title
                required property string author
                required property bool favourite
                required property bool current

                width: ListView.view.width
                height: 60
                radius: 6
                color: current ? "#253046" : "#171a20"
                border.color: current ? "#5b83d6" : "#2c313b"

                Rectangle {
                    width: 4
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    radius: 2
                    color: current ? "#6ea8ff" : "transparent"
                }

                TapHandler {
                    onTapped: playlistManager.playIndex(index)
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 6

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

                        Label {
                            text: author
                            color: "#8b93a1"
                            font.pixelSize: 12
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                    }

                    ToolButton {
                        text: "▲"
                        visible: hoverHandler.hovered
                        onClicked: playlistManager.movePresetUp(index)
                    }

                    ToolButton {
                        text: "▼"
                        visible: hoverHandler.hovered
                        onClicked: playlistManager.movePresetDown(index)
                    }

                    ToolButton {
                        text: "×"
                        visible: hoverHandler.hovered
                        onClicked: playlistManager.removePreset(index)
                    }
                }
                
                HoverHandler {
                    id: hoverHandler
                }
            }
        }
    }
}

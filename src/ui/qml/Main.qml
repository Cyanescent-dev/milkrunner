import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import QtQuick.Window

ApplicationWindow {
    // Emoji need a colour-emoji font; use plain text on platforms where it is not guaranteed.
    readonly property bool useEmoji: Qt.platform.os === "osx"
    id: root
    width: 1280
    height: 820
    minimumWidth: 980
    minimumHeight: 640
    visible: true
    title: "Milk Runner Visualizer"
    color: "#1a1d24"

    property bool performanceMode: visibility === Window.FullScreen

    function toggleFullscreen() {
        visibility = visibility === Window.FullScreen ? Window.Windowed : Window.FullScreen
    }

    FolderDialog {
        id: presetFolderDialog
        title: "Import Preset Folder"
        onAccepted: appController.importPresetFolder(selectedFolder)
    }

    Shortcut {
        sequence: "F11"
        onActivated: root.toggleFullscreen()
    }

    Shortcut {
        sequence: "Escape"
        enabled: root.performanceMode
        onActivated: root.visibility = Window.Windowed
    }

    Shortcut {
        sequence: "Right"
        onActivated: appController.nextPreset()
    }

    Shortcut {
        sequence: "Left"
        onActivated: appController.previousPreset()
    }

    Shortcut {
        sequence: "Space"
        onActivated: appController.togglePlaybackPaused()
    }

    Shortcut {
        sequence: "L"
        onActivated: appController.togglePresetLock()
    }

    Shortcut {
        sequence: "R"
        onActivated: appController.randomPreset()
    }

    SettingsView {
        id: settingsPopup
    }

    header: ToolBar {
        height: 48
        visible: !root.performanceMode
        background: Rectangle { color: "#171a20"; border.color: "#2c313b"; border.width: 1 }
        
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            spacing: 14

            Label {
                text: useEmoji ? "🥛 Milk Runner Visualizer" : "Milk Runner Visualizer"
                font.pixelSize: 16
                font.bold: true
                color: "#f2f4f8"
            }

            Button {
                id: headerImportBtn
                text: useEmoji ? "📁 Import Folder" : "Import Folder"
                font.bold: true
                font.pixelSize: 13
                contentItem: Text {
                    text: headerImportBtn.text
                    font: headerImportBtn.font
                    color: "#ffffff"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    color: headerImportBtn.down ? "#23477e" : (headerImportBtn.hovered ? "#3b6bb8" : "#2e5ba6")
                    border.color: "#5b8fe0"
                    border.width: 1
                    radius: 6
                }
                onClicked: presetFolderDialog.open()
            }

            Item { Layout.fillWidth: true }
            
            Button {
                id: headerFullscreenBtn
                text: root.visibility === Window.FullScreen ? "Exit Fullscreen" : (useEmoji ? "⛶ Fullscreen" : "Fullscreen")
                font.pixelSize: 13
                contentItem: Text {
                    text: headerFullscreenBtn.text
                    font: headerFullscreenBtn.font
                    color: "#d8e1f0"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    color: headerFullscreenBtn.down ? "#222733" : (headerFullscreenBtn.hovered ? "#2e3646" : "#252b38")
                    border.color: "#3d475c"
                    border.width: 1
                    radius: 6
                }
                onClicked: root.toggleFullscreen()
            }
        }
    }

    footer: ToolBar {
        height: 56
        visible: !root.performanceMode
        background: Rectangle { color: "#171a20"; border.color: "#2c313b"; border.width: 1 }
        
        RowLayout {
            anchors.fill: parent
            anchors.margins: 8
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            spacing: 16

            // Now Playing Info
            ColumnLayout {
                spacing: 2
                Layout.preferredWidth: 250
                Label {
                    text: appController.activePresetTitle.length > 0 ? appController.activePresetTitle : "No preset playing"
                    font.bold: true
                    color: "#ffffff"
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
                Label {
                    text: appController.currentPresetPosition
                    color: "#a0aab8"
                    font.pixelSize: 12
                }
            }

            Item { Layout.fillWidth: true }

            // Playback Controls
            RowLayout {
                spacing: 4
                
                ToolButton {
                    text: useEmoji ? "⏮" : "Prev"
                    font.pixelSize: 18
                    onClicked: appController.previousPreset()
                }
                ToolButton {
                    text: appController.playbackPaused ? (useEmoji ? "▶️" : "Play") : (useEmoji ? "⏸" : "Pause")
                    font.pixelSize: 18
                    onClicked: appController.togglePlaybackPaused()
                }
                ToolButton {
                    text: useEmoji ? "⏭" : "Next"
                    font.pixelSize: 18
                    onClicked: appController.nextPreset()
                }
                ToolButton {
                    text: appController.presetLocked ? (useEmoji ? "🔒" : "Locked") : (useEmoji ? "🔓" : "Lock")
                    font.pixelSize: 18
                    onClicked: appController.togglePresetLock()
                }
                ToolButton {
                    text: useEmoji ? "🔀" : "Random"
                    font.pixelSize: 18
                    onClicked: appController.randomPreset()
                }
            }

            Item { Layout.fillWidth: true }

            // Audio Controls
            RowLayout {
                spacing: 8
                
                Label {
                    text: !useEmoji ? "Input" : appController.selectedAudioInputDevice.indexOf("System Audio") !== -1 ? "🔊" :
                          (appController.selectedAudioInputDevice === "Scion MIDI" ? "🎹" : "🎤")
                    font.pixelSize: 15
                    ToolTip.visible: inputIconHover.hovered
                    ToolTip.text: "Audio Input Source"
                    HoverHandler { id: inputIconHover }
                }
                ComboBox {
                    id: inputDevicePicker
                    model: appController.audioInputDevices
                    currentIndex: Math.max(0, appController.audioInputDevices.indexOf(appController.selectedAudioInputDevice))
                    enabled: appController.audioInputDevices.length > 0
                    Layout.preferredWidth: 230
                    onActivated: appController.selectedAudioInputDevice = currentText
                }
                
                Slider {
                    from: 0.25
                    to: 8.0
                    stepSize: 0.25
                    value: appController.audioGain
                    Layout.preferredWidth: 90
                    onMoved: appController.audioGain = value
                    ToolTip.visible: pressed
                    ToolTip.text: value.toFixed(2) + "x Gain"
                }

                ProgressBar {
                    value: appController.audioLevel
                    Layout.preferredWidth: 60
                }

                ToolSeparator {}

                ToolButton {
                    text: useEmoji ? "⚙️" : "Settings"
                    font.pixelSize: 16
                    onClicked: settingsPopup.open()
                }
            }
        }
    }

    SplitView {
        anchors.fill: parent
        orientation: Qt.Horizontal

        VisualizerView {
            performanceMode: root.performanceMode
            SplitView.fillWidth: true
            SplitView.minimumWidth: 520
        }

        Pane {
            visible: !root.performanceMode
            SplitView.preferredWidth: 410
            SplitView.minimumWidth: 340
            padding: 0
            background: Rectangle { color: "#1a1d24" }

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                // Unified List Selector Header
                Rectangle {
                    Layout.fillWidth: true
                    height: 50
                    color: "#171a20"
                    border.color: "#2c313b"
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 10

                        ComboBox {
                            id: listSelector
                            Layout.fillWidth: true
                            model: {
                                let m = ["All Presets"]
                                for (let i = 0; i < playlistManager.playlistNames.length; i++) {
                                    m.push(playlistManager.playlistNames[i])
                                }
                                return m
                            }
                            onActivated: {
                                if (currentIndex === 0) {
                                    unifiedStack.currentIndex = 0
                                } else {
                                    playlistManager.setActivePlaylist(currentText)
                                    unifiedStack.currentIndex = 1
                                }
                            }
                            
                            Connections {
                                target: playlistManager
                                function onPlaylistNamesChanged() {
                                    let m = ["All Presets"]
                                    for (let i = 0; i < playlistManager.playlistNames.length; i++) {
                                        m.push(playlistManager.playlistNames[i])
                                    }
                                    listSelector.model = m
                                    if (unifiedStack.currentIndex === 1) {
                                        listSelector.currentIndex = Math.max(1, m.indexOf(playlistManager.activePlaylistName))
                                    }
                                }
                            }
                        }
                    }
                }

                StackLayout {
                    id: unifiedStack
                    currentIndex: 0
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    PresetLibraryView {}
                    PlaylistEditor {}
                }
            }
        }
    }
}

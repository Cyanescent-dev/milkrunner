import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    // Emoji need a colour-emoji font; use plain text on platforms where it is not guaranteed.
    readonly property bool useEmoji: Qt.platform.os === "osx"
    id: root
    padding: 24
    modal: true
    focus: true
    anchors.centerIn: parent
    // Let the pane follow the layout's required width. Native ComboBox controls
    // may require more room than their preferred width on macOS.
    width: settingsContent.width + leftPadding + rightPadding

    background: Rectangle {
        color: "#1e222d"
        border.color: "#3d485c"
        border.width: 1.5
        radius: 12
    }

    ColumnLayout {
        id: settingsContent
        spacing: 16
        width: Math.max(360, implicitWidth)

        RowLayout {
            Layout.fillWidth: true
            Label {
                text: useEmoji ? "⚙️ Settings" : "Settings"
                font.pixelSize: 18
                font.bold: true
                color: "#ffffff"
                Layout.fillWidth: true
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: "#323c4e"
        }

        // Section: Performance & Graphics
        Label {
            text: "Performance & Rendering"
            font.pixelSize: 12
            font.bold: true
            color: "#8fa3c7"
            Layout.fillWidth: true
        }

        RowLayout {
            Layout.fillWidth: true
            Label {
                text: "FPS Cap"
                color: "#f0f3f8"
                font.pixelSize: 14
                Layout.fillWidth: true
            }
            ComboBox {
                id: fpsPicker
                model: ["30", "60", "Uncapped"]
                currentIndex: appController.fpsCap === 30 ? 0 : appController.fpsCap === 60 ? 1 : 2
                onActivated: appController.fpsCap = currentText === "Uncapped" ? 0 : Number(currentText)
                Layout.preferredWidth: 120
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Label {
                text: "Render Scale"
                color: "#f0f3f8"
                font.pixelSize: 14
                Layout.fillWidth: true
            }
            ComboBox {
                id: scalePicker
                model: ["0.5x", "0.75x", "1.0x"]
                currentIndex: appController.renderScale <= 0.5 ? 0 : appController.renderScale < 1.0 ? 1 : 2
                onActivated: appController.renderScale = currentIndex === 0 ? 0.5 : currentIndex === 1 ? 0.75 : 1.0
                Layout.preferredWidth: 120
            }
        }

        Switch {
            id: lowPowerSwitch
            text: "Low-Power Mode"
            checked: appController.lowPowerMode
            onToggled: appController.lowPowerMode = checked
            Layout.fillWidth: true
            contentItem: Text {
                text: lowPowerSwitch.text
                font: lowPowerSwitch.font
                color: "#f0f3f8"
                verticalAlignment: Text.AlignVCenter
                leftPadding: lowPowerSwitch.indicator.width + lowPowerSwitch.spacing
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: "#323c4e"
        }

        // Section: Audio & Synth
        Label {
            text: "Audio & Synthesis"
            font.pixelSize: 12
            font.bold: true
            color: "#8fa3c7"
            Layout.fillWidth: true
        }

        Label {
            text: "Scion Synth Layers"
            color: "#f0f3f8"
            font.pixelSize: 14
            Layout.fillWidth: true
        }

        GridLayout {
            columns: 2
            columnSpacing: 18
            rowSpacing: 2
            Layout.fillWidth: true

            Repeater {
                model: appController.scionAudioModes

                CheckBox {
                    required property var modelData
                    text: modelData.name
                    checked: appController.scionEnabledSynthModes.indexOf(modelData.id) !== -1
                    onToggled: appController.setScionSynthEnabled(modelData.id, checked)

                    contentItem: Text {
                        text: parent.text
                        font: parent.font
                        color: "#f0f3f8"
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: parent.indicator.width + parent.spacing
                    }
                }
            }
        }

        Switch {
            id: playInternalAudioSwitch
            text: "Play Internal Audio (Scion)"
            checked: appController.playAudioOutput
            onToggled: appController.playAudioOutput = checked
            Layout.fillWidth: true
            contentItem: Text {
                text: playInternalAudioSwitch.text
                font: playInternalAudioSwitch.font
                color: "#f0f3f8"
                verticalAlignment: Text.AlignVCenter
                leftPadding: playInternalAudioSwitch.indicator.width + playInternalAudioSwitch.spacing
            }
        }

        // Info Card
        Rectangle {
            Layout.fillWidth: true
            height: infoColumn.implicitHeight + 16
            color: "#161922"
            border.color: "#2c3545"
            radius: 8

            ColumnLayout {
                id: infoColumn
                anchors.fill: parent
                anchors.margins: 8
                spacing: 4

                Label {
                    text: "Renderer: " + appController.rendererName
                    color: "#b0bdd4"
                    font.pixelSize: 12
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }

                Label {
                    text: "Config: " + appController.configFolder
                    color: "#8e9cb5"
                    font.pixelSize: 11
                    Layout.fillWidth: true
                    elide: Text.ElideMiddle
                }
            }
        }

        Button {
            id: closeBtn
            text: "Close"
            font.bold: true
            Layout.alignment: Qt.AlignRight
            contentItem: Text {
                text: closeBtn.text
                font: closeBtn.font
                color: "#ffffff"
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                color: closeBtn.down ? "#284a7e" : (closeBtn.hovered ? "#3b64a4" : "#2e4f84")
                border.color: "#5b8be0"
                border.width: 1
                radius: 6
            }
            onClicked: root.close()
        }
    }
}

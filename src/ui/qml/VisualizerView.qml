import QtQuick
import QtQuick.Controls
import MilkRunnerVisualizer

Item {
    id: root
    clip: true

    property bool performanceMode: false
    property real targetInterval: appController.fpsCap === 0 ? 16 : Math.max(16, 1000 / appController.fpsCap)

    Timer {
        interval: root.targetInterval
        repeat: true
        running: true
        onTriggered: visualSurface.update()
    }

    Rectangle {
        anchors.fill: parent
        color: "#07080a"
    }

    OpenGlVisualizerItem {
        id: visualSurface
        objectName: "visualSurface"
        anchors.fill: parent
        audioLevel: appController.audioLevel
        renderScale: appController.renderScale
        lowPowerMode: appController.lowPowerMode
        presetPath: appController.activePresetPath
        textureRoot: appController.presetFolder
        presetDurationSeconds: appController.presetDurationSeconds
        transitionDurationSeconds: appController.transitionDurationSeconds
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 34
        visible: !root.performanceMode && visualSurface.diagnosticsText.length > 0
        color: "#8a05070a"

        Label {
            anchors.fill: parent
            anchors.leftMargin: 18
            anchors.rightMargin: 18
            verticalAlignment: Text.AlignVCenter
            text: visualSurface.diagnosticsText
            color: "#b9bec8"
            font.pixelSize: 12
            elide: Text.ElideRight
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: footer.top
        height: 36
        visible: !root.performanceMode && (visualSurface.statusMessage.length > 0 || appController.audioStatusMessage.length > 0)
        color: "#b01a1010"

        Label {
            anchors.fill: parent
            anchors.leftMargin: 18
            anchors.rightMargin: 18
            verticalAlignment: Text.AlignVCenter
            text: visualSurface.statusMessage.length > 0 ? visualSurface.statusMessage : appController.audioStatusMessage
            color: "#ffd9b8"
            font.pixelSize: 13
            elide: Text.ElideRight
        }
    }

    Rectangle {
        id: footer
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 56
        visible: !root.performanceMode
        color: "#9907080a"

        Row {
            anchors.fill: parent
            anchors.leftMargin: 18
            anchors.rightMargin: 18
            spacing: 16

            Label {
                anchors.verticalCenter: parent.verticalCenter
                text: appController.currentPresetPosition + "  "
                    + (appController.activePresetTitle.length > 0 ? appController.activePresetTitle : "No active preset")
                    + (appController.playbackPaused ? "  [paused]" : "")
                    + (appController.presetLocked ? "  [locked]" : "")
                color: "#f4f5f7"
                font.pixelSize: 18
                elide: Text.ElideRight
                width: parent.width * 0.62
            }

            Label {
                anchors.verticalCenter: parent.verticalCenter
                text: appController.rendererName + "  |  audio " + appController.rawAudioLevel.toFixed(2) + ">" + appController.audioLevel.toFixed(2) + " peak " + appController.audioPeakLevel.toFixed(2)
                color: "#b9bec8"
                font.pixelSize: 13
                elide: Text.ElideRight
                width: parent.width * 0.34
                horizontalAlignment: Text.AlignRight
            }
        }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 3
            color: "#1f2630"

            Rectangle {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: parent.width * appController.presetCycleProgress
                color: appController.presetLocked ? "#f0c36a" : appController.playbackPaused ? "#8b93a1" : "#6ea8ff"
            }
        }
    }
}

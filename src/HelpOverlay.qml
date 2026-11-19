import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Every shortcut in one place, on ?.
Rectangle {
    id: overlay
    color: "#cc000000"

    MouseArea {
        anchors.fill: parent
        onClicked: overlay.visible = false
    }

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(parent.width - 80, 560)
        height: help.implicitHeight + 48
        radius: Math.min(8, theme.radius)
        color: theme.surface
        border.width: 1
        border.color: theme.subtle

        ColumnLayout {
            id: help
            anchors.fill: parent
            anchors.margins: 24
            spacing: 10

            Label {
                text: qsTr("Keyboard")
                font.pixelSize: 18
                font.bold: true
                color: theme.foreground
            }

            Repeater {
                model: [
                    ["Space", qsTr("Apply the profile to the mouse")],
                    ["1 / 2 / 3", qsTr("DPI, performance, timing")],
                    ["R", qsTr("Back to the factory defaults")],
                    ["Battery", qsTr("Read the level from the mouse")],
                    ["?", qsTr("This list")],
                    ["Q", qsTr("Quit")],
                ]
                delegate: RowLayout {
                    required property var modelData
                    spacing: 16
                    Label {
                        Layout.preferredWidth: 110
                        text: modelData[0]
                        color: theme.accent
                        font.pixelSize: 13
                        font.bold: true
                    }
                    Label {
                        Layout.fillWidth: true
                        text: modelData[1]
                        color: theme.foreground
                        font.pixelSize: 13
                        wrapMode: Text.WordWrap
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                Layout.topMargin: 8
                wrapMode: Text.WordWrap
                text: qsTr("The R1 cannot report its settings back, so shark keeps them in a profile on this machine and writes them whenever you apply.")
                color: theme.subtle
                font.pixelSize: 12
            }
        }
    }
}

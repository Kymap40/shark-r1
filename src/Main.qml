import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: win

    width: 900
    height: 640
    visible: true
    title: qsTr("Attack Shark R1")
    color: theme.background

    Material.theme: Material.Dark
    Material.accent: theme.accent
    Material.background: theme.background
    Material.foreground: theme.foreground
    Material.roundedScale: theme.radius

    property int page: 0
    property bool showHelp: false
    property bool confirmQuit: false

    // Narrow tiles fold the page list down to its numbers so the settings
    // themselves keep the room.
    readonly property bool compact: width < 820
    readonly property var pageLabels: [qsTr("DPI stages"), qsTr("Performance"), qsTr("Timing")]

    function stageDpi(index) {
        const stages = backend.settings.dpi;
        return stages && index < stages.length ? stages[index] : 800;
    }
    function stageActive(index) {
        return backend.settings.activeStage === index + 1;
    }

    LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
    LayoutMirroring.childrenInherit: true

    Shortcut { sequence: "Space"; context: Qt.ApplicationShortcut; onActivated: backend.apply() }
    Shortcut { sequence: "1"; context: Qt.ApplicationShortcut; onActivated: win.page = 0 }
    Shortcut { sequence: "2"; context: Qt.ApplicationShortcut; onActivated: win.page = 1 }
    Shortcut { sequence: "3"; context: Qt.ApplicationShortcut; onActivated: win.page = 2 }
    Shortcut { sequence: "?"; context: Qt.ApplicationShortcut; onActivated: win.showHelp = !win.showHelp }
    Shortcut { sequence: "R"; context: Qt.ApplicationShortcut; onActivated: backend.resetDefaults() }
    Shortcut {
        sequence: "Q"
        context: Qt.ApplicationShortcut
        onActivated: backend.dirty ? win.confirmQuit = true : Qt.quit()
    }
    Shortcut { sequences: [StandardKey.Quit]; context: Qt.ApplicationShortcut; onActivated: Qt.quit() }
    Shortcut { sequence: "Escape"; context: Qt.ApplicationShortcut; onActivated: win.showHelp = false }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 14

        // ------------------------------------------------------------ header
        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            ColumnLayout {
                spacing: 0
                Label {
                    text: qsTr("Attack Shark R1")
                    font.pixelSize: 20
                    font.bold: true
                    color: theme.foreground
                }
                Label {
                    text: backend.deviceStatus
                    font.pixelSize: 11
                    elide: Text.ElideRight
                    Layout.maximumWidth: 340
                    color: backend.connected ? theme.accent : theme.subtle
                }
            }

            Item { Layout.fillWidth: true }

            Label {
                visible: backend.battery >= 0
                text: qsTr("%1% · %2").arg(backend.battery).arg(
                          backend.batteryState === "charging" ? qsTr("charging")
                        : backend.batteryState === "full" ? qsTr("full")
                        : backend.batteryState === "discharging" ? qsTr("on battery")
                        : qsTr("battery"))
                font.pixelSize: 12
                color: backend.battery <= 15 ? "#e5484d" : theme.foreground
            }

            Button {
                text: qsTr("Battery")
                flat: true
                font.pixelSize: 12
                onClicked: backend.refreshBattery()
            }
        }

        // -------------------------------------------------------------- body
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            // page list, plus the profiles underneath it
            ColumnLayout {
                id: sidebar
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: win.compact ? 40 : 168
                spacing: 3

                Repeater {
                    model: win.pageLabels

                    delegate: Rectangle {
                        id: navItem
                        required property int index
                        required property string modelData
                        Layout.fillWidth: true
                        implicitHeight: 34
                        radius: Math.min(4, theme.radius)
                        color: win.page === index ? theme.accent
                             : navMouse.containsMouse ? theme.surface : "transparent"

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 8
                            spacing: 6

                            Label {
                                Layout.fillWidth: true
                                text: win.compact ? "" : modelData
                                elide: Text.ElideRight
                                font.pixelSize: 13
                                color: win.page === index ? theme.accentForeground : theme.foreground
                            }
                            // The digit is the keyboard shortcut, kept apart
                            // from the name so it never reads as a preset.
                            Label {
                                text: index + 1
                                font.pixelSize: 11
                                opacity: 0.6
                                color: win.page === index ? theme.accentForeground : theme.subtle
                            }
                        }
                        MouseArea {
                            id: navMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: win.page = index
                        }
                    }
                }

                Item { Layout.fillHeight: true }

                Label {
                    visible: !win.compact
                    text: qsTr("Profiles")
                    font.pixelSize: 11
                    color: theme.subtle
                }
                ComboBox {
                    Layout.fillWidth: true
                    visible: !win.compact
                    font.pixelSize: 12
                    model: backend.profileNames
                    currentIndex: Math.max(0, backend.profileNames.indexOf(backend.activeProfile))
                    onActivated: index => backend.selectProfile(backend.profileNames[index])
                }
                RowLayout {
                    Layout.fillWidth: true
                    visible: !win.compact
                    spacing: 4
                    Button {
                        Layout.fillWidth: true
                        text: qsTr("Add")
                        flat: true
                        font.pixelSize: 11
                        onClicked: newProfileDialog.open()
                    }
                    Button {
                        Layout.fillWidth: true
                        text: qsTr("Delete")
                        flat: true
                        font.pixelSize: 11
                        enabled: backend.profileNames.length > 1
                        onClicked: backend.deleteProfile(backend.activeProfile)
                    }
                }
            }

            // page content
            Item {
                id: contentArea
                anchors.left: sidebar.right
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.leftMargin: 16
                clip: true

                // ---------------------------------------------------- DPI
                Flickable {
                    anchors.fill: parent
                    visible: win.page === 0
                    clip: true
                    contentWidth: width
                    contentHeight: dpiColumn.implicitHeight
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar {}

                    ColumnLayout {
                        id: dpiColumn
                        width: parent.width
                        spacing: 8

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Six stages; the mouse cycles them with its DPI button.")
                            color: theme.subtle
                            font.pixelSize: 11
                            elide: Text.ElideRight
                        }

                        Repeater {
                            model: 6

                            delegate: Rectangle {
                                required property int index
                                Layout.fillWidth: true
                                implicitHeight: 52
                                radius: Math.min(6, theme.radius)
                                color: win.stageActive(index) ? theme.surface : "transparent"
                                border.width: win.stageActive(index) ? 1 : 0
                                border.color: theme.accent

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 8
                                    anchors.rightMargin: 10
                                    spacing: 8

                                    Label {
                                        Layout.preferredWidth: 16
                                        text: index + 1
                                        color: win.stageActive(index) ? theme.accent : theme.subtle
                                        font.pixelSize: 14
                                    }

                                    Slider {
                                        Layout.fillWidth: true
                                        Layout.minimumWidth: 60
                                        from: 100
                                        to: 18000
                                        stepSize: 100
                                        value: win.stageDpi(index)
                                        onMoved: backend.setValue("dpi" + index, value)
                                    }

                                    Label {
                                        Layout.preferredWidth: 84
                                        Layout.maximumWidth: 84
                                        horizontalAlignment: Text.AlignRight
                                        text: win.stageDpi(index).toLocaleString(Qt.locale(), "f", 0)
                                        color: theme.foreground
                                        font.pixelSize: 12
                                        elide: Text.ElideNone
                                    }
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6
                            Label {
                                text: qsTr("Active stage")
                                color: theme.subtle
                                font.pixelSize: 11
                            }
                            Repeater {
                                model: 6
                                delegate: Rectangle {
                                    id: stageButton
                                    required property int index
                                    implicitWidth: 34
                                    implicitHeight: 26
                                    radius: Math.min(4, theme.radius)
                                    color: win.stageActive(index) ? theme.accent : "transparent"
                                    border.width: 1
                                    border.color: win.stageActive(index) ? theme.accent : theme.subtle

                                    Label {
                                        anchors.centerIn: parent
                                        text: index + 1
                                        font.pixelSize: 12
                                        color: win.stageActive(index) ? theme.accentForeground : theme.foreground
                                    }
                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: backend.setValue("activeStage", index + 1)
                                    }
                                }
                            }
                            Item { Layout.fillWidth: true }
                        }
                    }
                }

                // ---------------------------------------------- performance
                Flickable {
                    anchors.fill: parent
                    visible: win.page === 1
                    clip: true
                    contentWidth: width
                    contentHeight: perfColumn.implicitHeight
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar {}

                    ColumnLayout {
                        id: perfColumn
                        width: parent.width
                        spacing: 14

                        Label { text: qsTr("Polling rate"); color: theme.subtle; font.pixelSize: 11 }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6
                            Repeater {
                                model: [125, 250, 500, 1000]
                                delegate: Rectangle {
                                    required property int modelData
                                    Layout.fillWidth: true
                                    implicitHeight: 34
                                    radius: Math.min(4, theme.radius)
                                    color: backend.settings.pollingHz === modelData ? theme.accent : "transparent"
                                    border.width: 1
                                    border.color: backend.settings.pollingHz === modelData ? theme.accent : theme.subtle

                                    Label {
                                        anchors.centerIn: parent
                                        text: modelData + " Hz"
                                        font.pixelSize: 12
                                        color: backend.settings.pollingHz === modelData ? theme.accentForeground : theme.foreground
                                    }
                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: backend.setValue("pollingHz", modelData)
                                    }
                                }
                            }
                        }

                        Switch {
                            text: qsTr("Ripple control")
                            checked: backend.settings.ripple
                            font.pixelSize: 12
                            onToggled: backend.setValue("ripple", checked)
                        }
                        Switch {
                            text: qsTr("Angle snap")
                            checked: backend.settings.angleSnap
                            font.pixelSize: 12
                            onToggled: backend.setValue("angleSnap", checked)
                        }

                        Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: qsTr("Both are stored on the mouse itself, so they keep working on another computer.")
                            color: theme.subtle
                            font.pixelSize: 11
                        }
                        Item { Layout.fillHeight: true }
                    }
                }

                // --------------------------------------------------- timing
                Flickable {
                    anchors.fill: parent
                    visible: win.page === 2
                    clip: true
                    contentWidth: width
                    contentHeight: timingColumn.implicitHeight
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar {}

                    ColumnLayout {
                        id: timingColumn
                        width: parent.width
                        spacing: 10

                        Label {
                            text: qsTr("Idle sleep: %1 min").arg(backend.settings.sleepMin.toLocaleString(Qt.locale(), "f", 1))
                            color: theme.subtle
                            font.pixelSize: 11
                        }
                        Slider {
                            Layout.fillWidth: true
                            from: 0.5
                            to: 30
                            stepSize: 0.5
                            value: backend.settings.sleepMin
                            onMoved: backend.setValue("sleepMin", value)
                        }

                        Label {
                            text: qsTr("Deep sleep: %1 min").arg(backend.settings.deepSleepMin)
                            color: theme.subtle
                            font.pixelSize: 11
                        }
                        Slider {
                            Layout.fillWidth: true
                            from: 1
                            to: 60
                            stepSize: 1
                            value: backend.settings.deepSleepMin
                            onMoved: backend.setValue("deepSleepMin", value)
                        }

                        Label {
                            text: qsTr("Click response: %1 ms").arg(backend.settings.debounceMs)
                            color: theme.subtle
                            font.pixelSize: 11
                        }
                        Slider {
                            Layout.fillWidth: true
                            from: 4
                            to: 50
                            stepSize: 2
                            value: backend.settings.debounceMs
                            onMoved: backend.setValue("debounceMs", value)
                        }

                        Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: qsTr("A shorter click response feels sharper; a longer one filters double-clicks from worn switches.")
                            color: theme.subtle
                            font.pixelSize: 11
                        }
                        Item { Layout.fillHeight: true }
                    }
                }
            }
        }

        // ------------------------------------------------------------ footer
        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Label {
                Layout.fillWidth: true
                text: backend.message.length > 0 ? backend.message
                    : backend.dirty ? qsTr("Changed -- Space applies it to the mouse")
                    : qsTr("In sync with the mouse")
                color: backend.message.length > 0 ? (backend.ok ? theme.accent : "#e5484d") : theme.subtle
                font.pixelSize: 11
                elide: Text.ElideRight
            }

            Button {
                text: qsTr("Apply")
                highlighted: true
                enabled: !backend.busy
                font.pixelSize: 12
                onClicked: backend.apply()
            }
            Button {
                text: "?"
                flat: true
                font.pixelSize: 12
                onClicked: win.showHelp = !win.showHelp
            }
        }
    }

    // -------------------------------------------------------------- overlays
    HelpOverlay {
        anchors.fill: parent
        visible: win.showHelp
        onDismissed: win.showHelp = false
    }

    Dialog {
        id: newProfileDialog
        title: qsTr("New profile")
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Ok | Dialog.Cancel

        TextField {
            id: newProfileName
            placeholderText: qsTr("Name")
            width: 240
        }
        onAccepted: {
            backend.addProfile(newProfileName.text);
            newProfileName.text = "";
        }
    }

    Dialog {
        id: quitDialog
        title: qsTr("Leave without applying?")
        visible: win.confirmQuit
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Yes | Dialog.No
        Label { text: qsTr("The changes are saved in the profile, but the mouse still runs the old settings.") }
        onAccepted: Qt.quit()
        onRejected: win.confirmQuit = false
    }
}

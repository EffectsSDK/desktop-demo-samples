import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property string title
    property alias checked: toggle.checked
    property bool resettable: false
    default property alias content: body.data

    signal toggled(bool checked)
    signal resetRequested()

    Layout.fillWidth: true
    implicitHeight: column.implicitHeight
    radius: 12
    color: "#2A2A2E"
    opacity: enabled ? 1 : 0.5
    clip: true

    Behavior on implicitHeight {
        NumberAnimation {
            duration: 120
            easing.type: Easing.OutQuad
        }
    }

    ColumnLayout {
        id: column

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 52
            Layout.leftMargin: 16
            Layout.rightMargin: 6

            Label {
                Layout.fillWidth: true
                text: root.title
                font.bold: true
                font.pixelSize: 15
                elide: Text.ElideRight
            }

            Item {
                Layout.preferredWidth: 32
                Layout.preferredHeight: 32
                visible: root.resettable && root.checked

                ToolButton {
                    anchors.centerIn: parent
                    width: 40
                    height: 40
                    padding: 10
                    display: AbstractButton.IconOnly
                    icon.source: "../resources/icons/reset.svg"
                    icon.width: 20
                    icon.height: 20

                    ToolTip.visible: hovered
                    ToolTip.delay: 600
                    ToolTip.text: "Reset to defaults"

                    onClicked: root.resetRequested()
                }
            }

            Switch {
                id: toggle

                leftPadding: 0

                onToggled: root.toggled(checked)
            }
        }

        ColumnLayout {
            id: body

            Layout.fillWidth: true
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            Layout.bottomMargin: 12
            visible: root.checked && (children.length > 0)
            spacing: 4
        }
    }
}

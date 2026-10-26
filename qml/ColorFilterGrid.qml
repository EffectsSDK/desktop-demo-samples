pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Effects
import QtQuick.Layouts
import QtQuick.Shapes

GridLayout {
    id: root

    property var model: []
    property int currentIndex: -1

    signal activated(int index)

    Layout.fillWidth: true
    columns: 4
    columnSpacing: 8
    rowSpacing: 8

    Repeater {
        model: root.model

        delegate: AbstractButton {
            id: tile

            required property int index
            required property string modelData
            readonly property bool selected: index === root.currentIndex

            Layout.fillWidth: true
            Layout.preferredHeight: width * 9 / 16
            hoverEnabled: true
            opacity: pressed ? 0.7 : 1
            z: selected ? 1 : 0

            ToolTip.visible: hovered
            ToolTip.delay: 600
            ToolTip.text: modelData

            onClicked: root.activated(index)

            contentItem: Item {
                Image {
                    id: preview

                    anchors.fill: parent
                    source: Qt.resolvedUrl("../resources/color_filters/filter_%1.jpg".arg(tile.index + 1))
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true
                    visible: false
                }

                Rectangle {
                    id: roundedMask

                    anchors.fill: parent
                    radius: 6
                    visible: false
                    layer.enabled: true
                }

                MultiEffect {
                    anchors.fill: preview
                    source: preview
                    maskEnabled: true
                    maskSource: roundedMask
                    maskThresholdMin: 0.5
                    maskSpreadAtMin: 1
                }
            }

            background: Rectangle {
                anchors.fill: parent
                anchors.margins: -3
                radius: 9
                color: "transparent"
                border.width: 2
                border.color: tile.selected
                    ? tile.Material.accent
                    : (tile.hovered ? "#55FFFFFF" : "transparent")
            }

            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.verticalCenter: parent.bottom
                width: 20
                height: 20
                radius: 10
                visible: tile.selected
                color: tile.Material.accent

                Shape {
                    anchors.fill: parent
                    preferredRendererType: Shape.CurveRenderer

                    ShapePath {
                        strokeColor: "white"
                        strokeWidth: 2
                        fillColor: "transparent"
                        capStyle: ShapePath.RoundCap
                        joinStyle: ShapePath.RoundJoin

                        PathPolyline {
                            path: [Qt.point(6, 10.5), Qt.point(9, 13.5), Qt.point(14.5, 7)]
                        }
                    }
                }
            }
        }
    }
}

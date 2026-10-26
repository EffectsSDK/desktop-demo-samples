import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root

    property string title
    property real from: 0
    property real to: 1
    property alias value: slider.value
    property real stepSize: 0.01
    property int decimals: 2

    signal moved(real value)

    Layout.fillWidth: true
    spacing: -8

    RowLayout {
        Layout.fillWidth: true

        Label {
            Layout.fillWidth: true
            text: root.title
            elide: Text.ElideRight
        }

        Label {
            text: slider.value.toFixed(root.decimals)
            opacity: 0.7
        }
    }

    Slider {
        id: slider

        Layout.fillWidth: true
        from: root.from
        to: root.to
        stepSize: root.stepSize
        onMoved: root.moved(value)
    }
}

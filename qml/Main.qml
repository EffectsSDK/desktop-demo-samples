import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Dialogs
import QtQuick.Layouts
import QtMultimedia

import SampleQml

ApplicationWindow {
    id: window

    required property SampleController controller
    required property string copyright

    readonly property string imageNameFilter: "Images (*.png *.jpg *.jpeg *.jpe *.tiff *.tif)"

    width: 1280
    height: 800
    minimumWidth: 960
    minimumHeight: 600
    visible: true
    title: "Video Effects SDK"

    Material.theme: Material.Dark
    Material.accent: Material.Teal

    component GroupCaption: Label {
        Layout.fillWidth: true
        Layout.topMargin: 12
        Layout.leftMargin: 4
        font.pixelSize: 12
        font.capitalization: Font.AllUppercase
        font.letterSpacing: 1
        opacity: 0.6
    }

    Connections {
        target: window.controller

        function onErrorOccurred(message: string) {
            errorDialog.text = message;
            errorDialog.open();
        }
    }

    MessageDialog {
        id: errorDialog

        title: "Error"
        buttons: MessageDialog.Ok
    }

    Dialog {
        id: aboutDialog

        anchors.centerIn: Overlay.overlay
        modal: true
        title: Qt.application.name
        standardButtons: Dialog.Ok

        Label {
            text: "%1 %2\n\nCopyright %3\nAll Rights Reserved"
                .arg(Qt.application.name)
                .arg(Qt.application.version)
                .arg(window.copyright)
        }
    }

    FileDialog {
        id: backgroundDialog

        title: "Open Background"
        nameFilters: [window.imageNameFilter]
        onAccepted: window.controller.backgroundFile = selectedFile
    }

    FileDialog {
        id: gradingReferenceDialog

        title: "Open Color Grading Reference"
        nameFilters: [window.imageNameFilter]
        onAccepted: {
            window.controller.colorGradingReference = selectedFile;
            window.controller.colorGradingEnabled = true;
        }
    }

    header: ToolBar {
        Material.primary: window.Material.background

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 8
            spacing: 12

            Label {
                text: "Camera"
            }

            ComboBox {
                Layout.preferredWidth: 220
                model: window.controller.camera.cameras
                currentIndex: window.controller.camera.cameraIndex
                onActivated: index => window.controller.camera.cameraIndex = index
            }

            ComboBox {
                Layout.preferredWidth: 110
                textRole: "text"
                model: [
                    { text: "2160p", size: Qt.size(3840, 2160) },
                    { text: "1440p", size: Qt.size(2560, 1440) },
                    { text: "1080p", size: Qt.size(1920, 1080) },
                    { text: "720p", size: Qt.size(1280, 720) },
                    { text: "480p", size: Qt.size(640, 480) },
                    { text: "360p", size: Qt.size(480, 360) }
                ]
                currentIndex: model.findIndex(item =>
                    (item.size.width === window.controller.camera.resolution.width)
                    && (item.size.height === window.controller.camera.resolution.height))
                onActivated: index => window.controller.camera.resolution = model[index].size
            }

            ComboBox {
                Layout.preferredWidth: 100
                enabled: count > 0
                model: window.controller.camera.frameRates.map(fps => fps + " fps")
                currentIndex: window.controller.camera.frameRates.indexOf(
                    window.controller.camera.activeFrameRate)
                onActivated: index => window.controller.camera.frameRate =
                    window.controller.camera.frameRates[index]
            }

            ToolSeparator {}

            Label {
                text: "Preset"
            }

            ComboBox {
                Layout.preferredWidth: 140
                enabled: window.controller.sdkAvailable
                model: ["Quality", "Balanced"]
                currentIndex: window.controller.preset
                onActivated: index => window.controller.preset = index
            }

            ToolSeparator {}

            TabBar {
                id: backendBar

                Layout.preferredWidth: 160
                enabled: window.controller.sdkAvailable
                background: null
                currentIndex: window.controller.gpuBackend ? 0 : 1


                TabButton {
                    text: "GPU"
                    onClicked: window.controller.gpuBackend = true
                }

                TabButton {
                    text: "CPU"
                    onClicked: window.controller.gpuBackend = false
                }
            }

            Switch {
                text: "Apple Neural Engine"
                enabled: window.controller.sdkAvailable && (Qt.platform.os === "osx")
                checked: window.controller.appleNeuralEngineEnabled
                onToggled: window.controller.appleNeuralEngineEnabled = checked
            }

            Item {
                Layout.fillWidth: true
            }

            ToolButton {
                text: "About"
                onClicked: aboutDialog.open()
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "black"

            VideoOutput {
                anchors.fill: parent
                fillMode: VideoOutput.PreserveAspectFit

                Component.onCompleted: window.controller.camera.videoSink = videoSink
            }

            Label {
                anchors.centerIn: parent
                visible: !window.controller.camera.cameraAvailable
                text: "The camera is not available"
                font.pixelSize: 18
                opacity: 0.7
            }

            Rectangle {
                visible: window.controller.camera.cameraAvailable
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.margins: 12
                width: metricsLabel.implicitWidth + 16
                height: metricsLabel.implicitHeight + 8
                radius: 4
                color: "#99000000"

                Label {
                    id: metricsLabel

                    anchors.centerIn: parent
                    color: "white"
                    text: {
                        const camera = window.controller.camera;
                        const parts = [];
                        parts.push(camera.frameTimeMs.toFixed(1) + " ms per frame");
                        parts.push(camera.fps.toFixed(1) + " fps");
                        parts.push(camera.frameSize.width + "×" + camera.frameSize.height);
                        return parts.join("  ·  ");
                    }
                }
            }
        }

        Pane {
            Layout.preferredWidth: 380
            Layout.fillHeight: true
            padding: 0

            ScrollView {
                anchors.fill: parent
                contentWidth: availableWidth

                ColumnLayout {
                    x: 12
                    width: parent.width - 24
                    spacing: 8

                    Item {
                        Layout.preferredHeight: 4
                    }

                    Label {
                        Layout.fillWidth: true
                        Layout.leftMargin: 4
                        visible: !window.controller.sdkAvailable
                        text: "Video Effects SDK is not available: the tsvb library failed to load or to authorize."
                        wrapMode: Text.Wrap
                        color: Material.color(Material.Red, Material.Shade300)
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        enabled: window.controller.sdkAvailable
                        spacing: 8

                        GroupCaption {
                            text: "Background"
                            Layout.topMargin: 0
                        }

                        FeatureCard {
                            title: "Blur"
                            checked: window.controller.blurEnabled
                            onToggled: checked => window.controller.blurEnabled = checked
                        }

                        FeatureCard {
                            title: "Replace"
                            checked: window.controller.replaceEnabled
                            onToggled: checked => window.controller.replaceEnabled = checked

                            Button {
                                Layout.fillWidth: true
                                text: "Open Background…"
                                onClicked: backgroundDialog.open()
                            }
                        }

                        GroupCaption {
                            text: "Face"
                        }

                        FeatureCard {
                            title: "Beautification"
                            resettable: true
                            onResetRequested: window.controller.resetToDefaults(["beautificationLevel"])
                            enabled: window.controller.gpuBackend
                            checked: window.controller.beautificationEnabled
                            onToggled: checked => window.controller.beautificationEnabled = checked

                            LabeledSlider {
                                title: "Level"
                                value: window.controller.beautificationLevel
                                onMoved: value => window.controller.beautificationLevel = value
                            }
                        }

                        FeatureCard {
                            title: "Smart Zoom"
                            resettable: true
                            onResetRequested: window.controller.resetToDefaults(["smartZoomLevel"])
                            checked: window.controller.smartZoomEnabled
                            onToggled: checked => window.controller.smartZoomEnabled = checked

                            LabeledSlider {
                                title: "Zoom Level"
                                value: window.controller.smartZoomLevel
                                onMoved: value => window.controller.smartZoomLevel = value
                            }
                        }

                        FeatureCard {
                            title: "Portrait Lighting"
                            resettable: true
                            onResetRequested: window.controller.resetToDefaults(["portraitLightStrength", "portraitBackgroundDarkening"])
                            enabled: window.controller.gpuBackend
                            checked: window.controller.portraitLightingEnabled
                            onToggled: checked => window.controller.portraitLightingEnabled = checked

                            LabeledSlider {
                                title: "Light Strength"
                                value: window.controller.portraitLightStrength
                                onMoved: value => window.controller.portraitLightStrength = value
                            }

                            LabeledSlider {
                                title: "Background Darkening"
                                value: window.controller.portraitBackgroundDarkening
                                onMoved: value => window.controller.portraitBackgroundDarkening = value
                            }
                        }

                        GroupCaption {
                            text: "Light & Detail"
                        }

                        FeatureCard {
                            title: "Noise Suppression"
                            enabled: window.controller.gpuBackend
                            checked: window.controller.noiseSuppressionEnabled
                            onToggled: checked => window.controller.noiseSuppressionEnabled = checked
                        }

                        FeatureCard {
                            title: "Low Light Adjustment"
                            resettable: true
                            onResetRequested: window.controller.resetToDefaults(["lowLightPower"])
                            enabled: window.controller.gpuBackend
                            checked: window.controller.lowLightEnabled
                            onToggled: checked => window.controller.lowLightEnabled = checked

                            LabeledSlider {
                                title: "Power"
                                value: window.controller.lowLightPower
                                onMoved: value => window.controller.lowLightPower = value
                            }
                        }

                        FeatureCard {
                            title: "Sharpening"
                            resettable: true
                            onResetRequested: window.controller.resetToDefaults(["sharpeningPower"])
                            checked: window.controller.sharpeningEnabled
                            onToggled: checked => window.controller.sharpeningEnabled = checked

                            LabeledSlider {
                                title: "Power"
                                value: window.controller.sharpeningPower
                                onMoved: value => window.controller.sharpeningPower = value
                            }
                        }

                        GroupCaption {
                            text: "Color"
                        }

                        FeatureCard {
                            title: "Color Correction"
                            resettable: true
                            onResetRequested: window.controller.resetToDefaults(["colorCorrectionPower", "colorCorrectionWhiteBalance", "colorCorrectionExposure", "colorCorrectionTemperature", "colorCorrectionTint", "colorCorrectionVibrance", "colorCorrectionContrast", "colorCorrectionUpdatePeriodMs"])
                            enabled: window.controller.gpuBackend
                            checked: window.controller.colorCorrectionEnabled
                            onToggled: checked => window.controller.colorCorrectionEnabled = checked

                            CheckBox {
                                text: "Auto Mode"
                                checked: window.controller.colorCorrectionAutoMode
                                onToggled: window.controller.colorCorrectionAutoMode = checked
                            }

                            LabeledSlider {
                                title: "Power"
                                value: window.controller.colorCorrectionPower
                                onMoved: value => window.controller.colorCorrectionPower = value
                            }

                            LabeledSlider {
                                title: "White Balance"
                                enabled: !window.controller.colorCorrectionAutoMode
                                value: window.controller.colorCorrectionWhiteBalance
                                onMoved: value => window.controller.colorCorrectionWhiteBalance = value
                            }

                            LabeledSlider {
                                title: "Exposure"
                                from: -1
                                enabled: !window.controller.colorCorrectionAutoMode
                                value: window.controller.colorCorrectionExposure
                                onMoved: value => window.controller.colorCorrectionExposure = value
                            }

                            LabeledSlider {
                                title: "Temperature"
                                from: -1
                                value: window.controller.colorCorrectionTemperature
                                onMoved: value => window.controller.colorCorrectionTemperature = value
                            }

                            LabeledSlider {
                                title: "Tint"
                                from: -1
                                value: window.controller.colorCorrectionTint
                                onMoved: value => window.controller.colorCorrectionTint = value
                            }

                            LabeledSlider {
                                title: "Vibrance"
                                from: -1
                                value: window.controller.colorCorrectionVibrance
                                onMoved: value => window.controller.colorCorrectionVibrance = value
                            }

                            LabeledSlider {
                                title: "Contrast"
                                from: -1
                                enabled: !window.controller.colorCorrectionAutoMode
                                value: window.controller.colorCorrectionContrast
                                onMoved: value => window.controller.colorCorrectionContrast = value
                            }

                            LabeledSlider {
                                title: "Update Period, ms"
                                to: 5000
                                stepSize: 50
                                decimals: 0
                                value: window.controller.colorCorrectionUpdatePeriodMs
                                onMoved: value => window.controller.colorCorrectionUpdatePeriodMs = value
                            }
                        }

                        FeatureCard {
                            title: "Color Grading"
                            resettable: true
                            onResetRequested: window.controller.resetToDefaults(["colorGradingPower"])
                            checked: window.controller.colorGradingEnabled
                            onToggled: checked => {
                                if (checked && window.controller.colorGradingReference.toString() === "") {
                                    gradingReferenceDialog.open();
                                }
                                window.controller.colorGradingEnabled = checked;
                            }

                            Button {
                                Layout.fillWidth: true
                                text: "Open Reference…"
                                onClicked: gradingReferenceDialog.open()
                            }

                            LabeledSlider {
                                title: "Intensity"
                                value: window.controller.colorGradingPower
                                onMoved: value => window.controller.colorGradingPower = value
                            }
                        }

                        FeatureCard {
                            title: "Color Filter"
                            resettable: true
                            onResetRequested: window.controller.resetToDefaults(["colorFilterPower"])
                            checked: window.controller.colorFilterEnabled
                            onToggled: checked => window.controller.colorFilterEnabled = checked

                            ColorFilterGrid {
                                Layout.topMargin: 4
                                Layout.bottomMargin: 8
                                model: window.controller.colorFilters
                                currentIndex: window.controller.colorFilterIndex
                                onActivated: index => window.controller.colorFilterIndex = index
                            }

                            LabeledSlider {
                                title: "Intensity"
                                value: window.controller.colorFilterPower
                                onMoved: value => window.controller.colorFilterPower = value
                            }
                        }
                    }

                    Item {
                        Layout.preferredHeight: 8
                    }
                }
            }
        }
    }
}

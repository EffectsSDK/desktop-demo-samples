#include "sample_ui.h"

#include "media_utils.h"
#include "sample.h"

SampleUI::SampleUI(Sample* sample)
	: m_sample(sample)
{
	createUI();
}

SampleUI::~SampleUI() = default;

void SampleUI::setCurrentPreset(Preset preset)
{
	for (int i = 0; i < presetBox->count(); ++i) {
		auto itemPreset = Preset(presetBox->itemData(i).toInt());
		if (preset == itemPreset) {
			presetBox->setCurrentIndex(i);
			break;
		}
	}
}

void SampleUI::setColorFilters(const QStringList& fileNames)
{
	for (auto& fileName : fileNames) {
		colorFilterComoBox->addItem(QFileInfo(fileName).baseName(), fileName);
	}
}

QString SampleUI::currentColorFilter() const
{
	return colorFilterComoBox->currentData().toString();
}

void SampleUI::setCurrentColorFilter(const QString& fileName)
{
	for (int i = 0; i < colorFilterComoBox->count(); ++i) {
		QString item = colorFilterComoBox->itemData(i).toString();
		if (fileName == item) {
			colorFilterComoBox->setCurrentIndex(i);
			break;
		}
	}
}

void SampleUI::createUI()
{
	 auto mainLayout = new QHBoxLayout(m_sample);

	frameView = new FrameView(m_sample);
	frameView->setMinimumSize(480, 270);
	mainLayout->addWidget(frameView, 1);

	metricsView = new MetricsView;
	auto metricsViewLayout = new QVBoxLayout(frameView);
	metricsViewLayout->addWidget(metricsView, 0, Qt::AlignRight | Qt::AlignBottom);

	auto controlsPanelScrollArea = new QScrollArea(m_sample);
	mainLayout->addWidget(controlsPanelScrollArea);
	controlsPanelScrollArea->setWidgetResizable(true);
	controlsPanelScrollArea->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Ignored);
	controlsPanelScrollArea->setFrameShape(QFrame::NoFrame);
	controlsPanelScrollArea->setSizeAdjustPolicy(QScrollArea::AdjustToContents);
	controlsPanelScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

	auto controlsWidget = new QWidget;
	controlsWidget->setContentsMargins(
		0,
		0,
		controlsPanelScrollArea->verticalScrollBar()->sizeHint().width(),
		0
	);
	controlsPanelScrollArea->setWidget(controlsWidget);
	auto controlsLayout = new QVBoxLayout(controlsWidget);

	cameraComoBox = new QComboBox(m_sample);
	controlsLayout->addWidget(cameraComoBox);
	for (const auto& name : availableCameras()) {
		cameraComoBox->addItem(name);
	}
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
	cameraComoBox->setPlaceholderText("Default Camera");
#endif
	connect(
		cameraComoBox, QOverload<int>::of(&QComboBox::activated),
		m_sample, &Sample::onCameraPicked
	);

	auto cameraScaleLabel = new QHBoxLayout;
	cameraScaleLabel->addWidget(new QLabel("Camera scale"));
	cameraScaleComoBox = new QComboBox(m_sample);
	cameraScaleLabel->addWidget(cameraScaleComoBox, 1);
	cameraScaleComoBox->addItem("2160p", QSize(3840, 2160));
	cameraScaleComoBox->addItem("1440p", QSize(2560, 1440));
	cameraScaleComoBox->addItem("1080p", QSize(1920, 1080));
	cameraScaleComoBox->addItem(DEFAULT_CAMERA_SCALE,
								  QSize(DEFAULT_SCALE_WIDTH, DEFAULT_SCALE_HEIGHT));
	cameraScaleComoBox->addItem("480p", QSize(640, 480));
	cameraScaleComoBox->addItem("360p", QSize(480, 360));
	connect(
		cameraScaleComoBox, QOverload<int>::of(&QComboBox::activated),
		this, [this](int index) {
			QSize size = cameraScaleComoBox->itemData(index).toSize();
			m_sample->setCameraScale(size);
	});
	controlsLayout->addLayout(cameraScaleLabel);

	virtualBackgroundBox = new QGroupBox("Virtual Background", m_sample);
	controlsLayout->addWidget(virtualBackgroundBox);
	auto vbLayout = new QVBoxLayout(virtualBackgroundBox);

	blurCheckBox = new QCheckBox("Blur", virtualBackgroundBox);
	connect(
		blurCheckBox, &QCheckBox::clicked,
		m_sample, &Sample::toggleBlurEnabled
	);
	vbLayout->addWidget(blurCheckBox, 0, Qt::AlignLeft);

	denoiseCheckBox = new QCheckBox("Denoise", virtualBackgroundBox);
	connect(
		denoiseCheckBox, &QCheckBox::clicked,
		m_sample, &Sample::toggleDenoiseEnabled
	);
	vbLayout->addWidget(denoiseCheckBox, 0, Qt::AlignLeft);

	replaceCheckBox = new QCheckBox("Replace", virtualBackgroundBox);
	connect(
		replaceCheckBox, &QCheckBox::clicked,
		m_sample, &Sample::toggleReplaceEnabled
	);
	vbLayout->addWidget(replaceCheckBox, 0, Qt::AlignLeft);

	beautificateCheckBox = new QCheckBox("Beautificate", virtualBackgroundBox);
	connect(
		beautificateCheckBox, &QCheckBox::clicked,
		m_sample, &Sample::toggleBeautificateEnabled
	);
	vbLayout->addWidget(beautificateCheckBox, 0, Qt::AlignLeft);
	smartZoomCheckBox = new QCheckBox("Smart Zoom", virtualBackgroundBox);
	connect(
		smartZoomCheckBox, &QCheckBox::clicked,
		m_sample, &Sample::toggleSmartZoomEnabled
	);
	vbLayout->addWidget(smartZoomCheckBox, 0, Qt::AlignLeft);

	openBackgroundButton = new QPushButton("Open Background", virtualBackgroundBox);
	connect(
		openBackgroundButton, &QPushButton::clicked,
		m_sample, &Sample::openBackground
	);
	vbLayout->addWidget(openBackgroundButton);

	auto powerLabel = new QHBoxLayout;
	powerLabel->addWidget(new QLabel("Beautification Level"));
	beautificationLevelLabel = new QLabel;
	powerLabel->addWidget(beautificationLevelLabel, 0, Qt::AlignRight);
	vbLayout->addLayout(powerLabel);
	beautificationLevelSlider = new QSlider(Qt::Horizontal, virtualBackgroundBox);
	beautificationLevelSlider->setRange(0, 1000);
	connect(
		beautificationLevelSlider, &QSlider::actionTriggered,
		m_sample, &Sample::onBeautificationLevelSliderMoved,
		Qt::QueuedConnection
	);
	vbLayout->addWidget(beautificationLevelSlider);

	auto powerDenoiseLabel = new QHBoxLayout;
	powerDenoiseLabel->addWidget(new QLabel("Denoise Power"));
	denoisePowerLabel = new QLabel;
	powerDenoiseLabel->addWidget(denoisePowerLabel, 0, Qt::AlignRight);
	vbLayout->addLayout(powerDenoiseLabel);
	denoisePowerSlider = new QSlider(Qt::Horizontal, virtualBackgroundBox);
	denoisePowerSlider->setRange(0, 1000);
	connect(
		denoisePowerSlider, &QSlider::actionTriggered,
		m_sample, &Sample::onDenoiseLevelSliderMoved,
		Qt::QueuedConnection
	);
	vbLayout->addWidget(denoisePowerSlider);

	denoiseWithFaceCheckBox = new QCheckBox(
		"Denoise With Face (without beautification)",
		virtualBackgroundBox
	);
	connect(
		denoiseWithFaceCheckBox, &QCheckBox::clicked,
		m_sample, &Sample::toggleDenoiseWithFaceClicked
	);
	vbLayout->addWidget(denoiseWithFaceCheckBox, 0, Qt::AlignLeft);

	auto zoomLabelLayout = new QHBoxLayout;
	zoomLabelLayout->addWidget(new QLabel("Zoom Level"));
	zoomLevelLabel = new QLabel;
	zoomLabelLayout->addWidget(zoomLevelLabel, 0, Qt::AlignRight);
	vbLayout->addLayout(zoomLabelLayout);
	zoomLevelSlider = new QSlider(Qt::Horizontal, virtualBackgroundBox);
	zoomLevelSlider->setRange(0, 1000);
	connect(
		zoomLevelSlider, &QSlider::actionTriggered,
		m_sample, &Sample::onZoomLevelSliderMoved,
		Qt::QueuedConnection
	);
	vbLayout->addWidget(zoomLevelSlider);

	colorBox = new QGroupBox("Color", m_sample);
	vbLayout->addWidget(colorBox);
	auto colorLayout = new QVBoxLayout(colorBox);

	correctColorsCheckbox = new QCheckBox("Correct Colors", colorBox);
	connect(
		correctColorsCheckbox, &QCheckBox::clicked,
		m_sample, &Sample::toggleCorrectColorsEnabled
	);
	colorLayout->addWidget(correctColorsCheckbox);

	colorGradingCheckbox = new QCheckBox("Color Grading", colorBox);
	connect(
		colorGradingCheckbox, &QCheckBox::clicked,
		m_sample, &Sample::toggleColorGradingEnabled
	);

	colorLayout->addWidget(colorGradingCheckbox);

	colorFilterCheckbox = new QCheckBox("Color Filter", colorBox);
	connect(
		colorFilterCheckbox, &QCheckBox::clicked,
		m_sample, &Sample::toggleColorFilterEnabled
	);
	colorFilterComoBox = new QComboBox;
	connect(
		colorFilterComoBox, QOverload<int>::of(&QComboBox::activated),
		this, [this](int index) {
			QString fileName = colorFilterComoBox->itemData(index).toString();
			m_sample->onColorFilterPicked(fileName);
		}
	);
	auto colorFilterLabelLayout = new QHBoxLayout;
	colorFilterLabelLayout->setContentsMargins(0, 0, 0, 0);
	colorFilterLabelLayout->setSpacing(8);
	colorFilterLabelLayout->addWidget(colorFilterCheckbox, 0, Qt::AlignLeft);
	colorFilterLabelLayout->addWidget(colorFilterComoBox, 1);
	colorLayout->addLayout(colorFilterLabelLayout);

	colorIntensitySlider = new QSlider(Qt::Horizontal, virtualBackgroundBox);
	colorIntensitySlider->setRange(0, 100);
	connect(
		colorIntensitySlider, &QSlider::actionTriggered,
		m_sample, &Sample::onColorIntensitySliderMoved,
		Qt::QueuedConnection
	);
	colorLayout->addWidget(colorIntensitySlider);
	colorIntensitySlider->setValue(80);
	colorIntensitySlider->setEnabled(false);

	openReferencedButton = new QPushButton("Open Color Grading Reference", colorBox);
	connect(
		openReferencedButton, &QPushButton::clicked,
		m_sample, &Sample::openColorGradingReference
	);
	colorLayout->addWidget(openReferencedButton);

	controlsLayout->addStretch();

	backendBox = new QGroupBox("Backend", m_sample);
	controlsLayout->addWidget(backendBox);
	auto backendsLayout = new QVBoxLayout(backendBox);
	gpuRadioButton = new QRadioButton("GPU", backendBox);
	connect(
		gpuRadioButton, &QPushButton::clicked,
		m_sample, &Sample::switchToGPU
	);
	backendsLayout->addWidget(gpuRadioButton, 0, Qt::AlignLeft);
	cpuRadioButton = new QRadioButton("CPU", backendBox);
	connect(
		cpuRadioButton, &QPushButton::clicked,
		m_sample, &Sample::switchToCPU
	);
	backendsLayout->addWidget(cpuRadioButton, 0, Qt::AlignLeft);

	auto presetLayout = new QVBoxLayout;
	presetLayout->addWidget(new QLabel("Preset"));
	presetBox = new QComboBox(m_sample);
	presetLayout->addWidget(presetBox);
	presetBox->addItem("Quality", int(Preset::quality));
	presetBox->addItem("Balanced", int(Preset::balanced));
	presetBox->addItem("Speed", int(Preset::speed));
	presetBox->addItem("Lightning", int(Preset::lightning));
	
	connect(
		presetBox, QOverload<int>::of(&QComboBox::activated),
		this, [this](int index) {
			auto preset = Preset(presetBox->itemData(index).toInt());
			m_sample->onPresetPicked(preset);
		}
	);

	vbLayout->addLayout(presetLayout);

	controlsLayout->addStretch(1);

	about = new QPushButton("About", m_sample);
	connect(
		about, &QPushButton::clicked,
		this, &SampleUI::openAboutDialog,
		Qt::QueuedConnection
	);
	controlsLayout->addWidget(about);
}

void SampleUI::openAboutDialog()
{
	QString description =
		TSVB_BUNDLE_NAME " " TSVB_VERSION_STRING "\n\n"
		"Copyright " TSVB_COPYRIGHT "\n"
		"All Rights Reserved";
	QMessageBox::about(m_sample, TSVB_APP_NAME, description);
}

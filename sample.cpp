#include "sample.h"

#include "media_utils.h"
#include "pipeline.h"
#include "sample_ui.h"

#ifdef Q_OS_MACOS
#include "camera_access_authorization.h"
#endif

const char CAMERA_NAME[] = "camera_name";
const char CAMERA_SCALE[] = "camera_scale";
const char BLUR_ENABLED[] = "blur_enabled";
const char REPLACE_ENABLED[] = "replace_enabled";
const char BEAUTIFICATION_ENABLED[] = "beautification_enabled";
const char BEAUTIFICATION_LEVEL[] = "beautification_level";
const char DENOISE_ENABLED[] = "denoise_enabled";
const char DENOISE_LEVEL[] = "denoise_level";
const char DENOISE_WITH_FACE[] = "denoise_with_face";
const char SMART_ZOOM_ENABLED[] = "smart_zoom_enabled";
const char LOW_LIGHT_ADJUSTMENT_ENABLED[] = "low_light_adjustment_enabled";
const char SHARPENING_ENABLED[] = "sharpening_enabled";
const char ZOOM_LEVEL[] = "zoom_level";
const char LOW_LIGHT_ADJUSTMENT_POWER[] = "low_light_adjustment_power";
const char BACKGROUND_FILEPATH[] = "background_filepath";
const char SHARPENING_POWER[] = "sharpening_power";
const char BACKEND[] = "backend";
const char PRESET[] = "preset";
const char COLOR_GRADING_REFERENCE_PATH[] = "color_grading_reference_path";
const char ENABLED_COLOR_CORRECTION_MODE[] = "enabled_color_correction_mode";
const char COLOR_CORRECTION_MODE_AUTO[] = "color_correction_mode_auto";
const char COLOR_CORRECTION_MODE_FILTER[] = "color_correction_mode_filter";
const char COLOR_CORRECTION_MODE_GRADING[] = "color_correction_mode_grading";
const char COLOR_FILTER_FILE[] = "color_filter_file";


#ifdef Q_OS_MACOS
static QString bundleResourcesPath()
{
	QDir bundleContent(QCoreApplication::applicationDirPath());
	bundleContent.cdUp();
	return bundleContent.absoluteFilePath("Resources");
}
#endif

#ifdef Q_OS_LINUX
static QString appImageUsrSharePath()
{
	auto env = QProcessEnvironment::systemEnvironment();
	if (env.contains("APPDIR")) {
		QDir appImgDir(env.value("APPDIR"));
		bool exists = appImgDir.cd("usr");
		exists &= appImgDir.cd("share");
		if (exists) {
			return appImgDir.absolutePath();
		}
	}

	QDir bundleContent(QCoreApplication::applicationDirPath());
	bundleContent.cdUp();
	return bundleContent.absoluteFilePath("share");
}
#endif

static QString settingsFilePath()
{
	const char settingsFileName[] = "sample-settings.ini";
#ifdef Q_OS_WINDOWS
	QDir appDir = QCoreApplication::applicationDirPath();
	QString settingsPath = appDir.absoluteFilePath(settingsFileName);
	return settingsPath;
#else
	QDir appDataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
	QString settingsPath = appDataDir.absoluteFilePath(settingsFileName);
	return settingsPath;
#endif
}

static QString defaultBackgroundPath()
{
	const char backgroundFileName[] = "background.jpg";
#ifdef Q_OS_WINDOWS
	QDir appDir = QCoreApplication::applicationDirPath();
	QString path = appDir.absoluteFilePath(backgroundFileName);
	return path;
#elif defined(Q_OS_MACOS)
	QDir bundleResourcesDir = bundleResourcesPath();
	QString path = bundleResourcesDir.absoluteFilePath(backgroundFileName);
	return path;
#else 
	QDir appImgResourcesDir = appImageUsrSharePath();
	QString path = appImgResourcesDir.absoluteFilePath(backgroundFileName);
	return path;
#endif
}

static QString dialogImageFilter()
{
	return "Image (*.png *.jpg *.jpeg *.jpe *.tiff *.tif)";
}

Sample::Sample()
	: m_ui(new SampleUI(this))
	, m_pipeline(new Pipeline)
{
	m_settings.reset(new QSettings(settingsFilePath(), QSettings::Format::IniFormat));
	if (!m_settings->isWritable()) {
		QMessageBox::warning(
			this,
			"Settings is not writable",
			"Can not open settings file for writting. Changes will not be saved."
		);
	}

	m_ui->setColorFilters(enumerateColorLutFiles());

	bool enabled = restoreUIAndPipelineFromSettings();
	if (!enabled) {
		QMessageBox::warning(nullptr, "Error", "Failure to initialize the SDK");
		exit(-1);
	}
	updateUIState();
 
	connect(
		m_pipeline.get(), &Pipeline::frameAvailable,
		this, &Sample::processFrame
	);
#ifdef Q_OS_MACOS
	auto accessStatus = videoCaptureAuthorizationStatus();
	if (CaptureAuthorizationStatus::authorized == accessStatus) {
		m_pipeline->start();
	}
	else {
		auto pipeline = m_pipeline;
		requestVideoCaptureAccess([pipeline](bool granted) {
			if (granted) {
				pipeline->start();
			}
		});
	}
#else
	m_pipeline->start();
#endif

	connect(
		&m_updateMetricsTimer, &QTimer::timeout,
		this, &Sample::updateMetrics
	);
	m_updateMetricsTimer.start(250);

	
	this->resize(1024, 576);
}

Sample::~Sample() = default;

void Sample::updateUIState()
{
	VideoFilter* videoFilter = m_pipeline->videoFilter();
	m_ui->blurCheckBox->setChecked(videoFilter->isBlurEnabled());
	bool isDenoiseEnabled = videoFilter->isDenoiseEnabled();
	m_ui->denoiseCheckBox->setChecked(isDenoiseEnabled);
	m_ui->replaceCheckBox->setChecked(videoFilter->isReplaceEnabled());
	m_ui->beautificateCheckBox->setChecked(videoFilter->isBeautificationEnabled());
	m_ui->correctColorsCheckbox->setChecked(videoFilter->isColorCorrectionEnabled());
	m_ui->colorGradingCheckbox->setChecked(videoFilter->isColorGradingEnabled());
	m_ui->smartZoomCheckBox->setChecked(videoFilter->isSmartZoomEnabled());
	m_ui->lowLightAdjustmentCheckbox->setChecked(videoFilter->isLowLightAdjustmentEnabled());
	m_ui->sharpeningCheckbox->setChecked(videoFilter->isSharpeningEnabled());

	m_ui->beautificationLevelSlider->setEnabled(videoFilter->isBeautificationEnabled());
	float beautificationLevel = videoFilter->beautificationLevel();
	m_ui->beautificationLevelSlider->setValue(static_cast<int>(
		m_ui->beautificationLevelSlider->maximum() * beautificationLevel
	));
	m_ui->beautificationLevelLabel->setText(stringFromNumber(beautificationLevel));

	m_ui->denoisePowerSlider->setEnabled(isDenoiseEnabled);
	m_ui->denoiseWithFaceCheckBox->setEnabled(isDenoiseEnabled);
	float denoisePower = videoFilter->denoisePower();
	m_ui->denoisePowerSlider->setValue(static_cast<int>(
		m_ui->denoisePowerSlider->maximum() * denoisePower
	));
	m_ui->denoisePowerLabel->setText(stringFromNumber(denoisePower));

	float zoomLevel = videoFilter->smartZoomLevel();
	m_ui->zoomLevelSlider->setValue(static_cast<int>(
		m_ui->zoomLevelSlider->maximum() * zoomLevel
	));
	m_ui->zoomLevelLabel->setText(stringFromNumber(zoomLevel));
	m_ui->zoomLevelSlider->setEnabled(videoFilter->isSmartZoomEnabled());

	m_ui->correctColorsCheckbox->setChecked(videoFilter->isColorCorrectionEnabled());
	m_ui->colorFilterCheckbox->setChecked(videoFilter->isColorFilterEnabled());
	m_ui->colorGradingCheckbox->setChecked(videoFilter->isColorGradingEnabled());
	bool colorCorrectionEnabled =
		videoFilter->isColorCorrectionEnabled() ||
		videoFilter->isColorFilterEnabled() ||
		videoFilter->isColorGradingEnabled();
	m_ui->colorIntensitySlider->setEnabled(colorCorrectionEnabled);

	float lowLightPower = videoFilter->getLowLightAdjustmentPower();
	m_ui->lowLightAdjustmentPowerSlider->setValue(static_cast<int>(
		m_ui->lowLightAdjustmentPowerSlider->maximum() * lowLightPower
	));
	m_ui->lowLightAdjustmentPowerLabel->setText(stringFromNumber(lowLightPower));
	m_ui->lowLightAdjustmentPowerSlider->setEnabled(videoFilter->isLowLightAdjustmentEnabled());

	float sharpeningPower = videoFilter->sharpeningPower();
	m_ui->sharpeningPowerLabel->setText(stringFromNumber(sharpeningPower));
	m_ui->sharpeningPowerSlider->setValue(static_cast<int>(
		sharpeningPower * m_ui->sharpeningPowerSlider->maximum()
	));
	m_ui->sharpeningPowerSlider->setEnabled(videoFilter->isSharpeningEnabled());

	auto backend = videoFilter->backend();
	m_ui->gpuRadioButton->setChecked(Backend::gpu == backend);
	m_ui->cpuRadioButton->setChecked(Backend::cpu == backend);

	auto preset = videoFilter->preset();
	m_ui->setCurrentPreset(preset);

	checkCPUPipelineAvailable();
	checkGPUOnlyFeaturesEnabled();
}

bool Sample::restoreUIAndPipelineFromSettings()
{
	VideoFilter* videoFilter = m_pipeline->videoFilter();
	if (!videoFilter->isValid()) {
		return false;
	}

	Preset currentPreset = videoFilter->preset();
	int presetInt = m_settings->value(PRESET, int(currentPreset)).toInt();
	bool presetIsValid = 
		(int(Preset::quality) <= presetInt) && (presetInt <= int(Preset::lightning));
	if (presetIsValid && (currentPreset != Preset(presetInt))) {
		videoFilter->setPreset(Preset(presetInt));
	}

	auto enableBGReplacer = [](Backend backend, VideoFilter* videoFilter) {
		if (!videoFilter->isValid()) {
			return false;
		}
		bool ok = videoFilter->setBackend(backend);
		if (!ok) {
			return false;
		}
		return true;
	};
	QString backendStr = m_settings->value(BACKEND, "GPU").toString();
	backendStr = (backendStr == "GPU" || backendStr == "CPU") ? backendStr : "GPU";
	if (backendStr == "GPU") {
		if (!enableBGReplacer(Backend::gpu, videoFilter)) {
			if (!enableBGReplacer(Backend::cpu, videoFilter)) {
				return false;
			}
		}
	}
	else {
		if (!enableBGReplacer(Backend::cpu, videoFilter)) {
			if (!enableBGReplacer(Backend::gpu, videoFilter)) {
				return false;
			}
		}
	}

	QString cameraName = m_settings->value(CAMERA_NAME, 0).toString();
	setCameraByName(cameraName);
	m_ui->cameraComoBox->setCurrentText(cameraName);

	QSize cameraScale = 
		m_settings->value(CAMERA_SCALE).toSize();
	if (cameraScale.width() <= 0 || cameraScale.height() <= 0) {
		cameraScale.setWidth(DEFAULT_SCALE_WIDTH);
		cameraScale.setHeight(DEFAULT_SCALE_HEIGHT);
	}
	int cameraScaleIndex = m_ui->cameraScaleComoBox->findData(cameraScale);
	m_ui->cameraScaleComoBox->setCurrentIndex(cameraScaleIndex);

	m_pipeline->trySetFrameSize(cameraScale.width(), cameraScale.height());

	bool isBlurEnabled = m_settings->value(BLUR_ENABLED, false).toBool();
	if (isBlurEnabled) {
		videoFilter->enableBlur();
	}
	else {
		videoFilter->disableBlur();
	}

	bool isReplaceEnabled = m_settings->value(REPLACE_ENABLED, true).toBool();
	if (isReplaceEnabled) {
		videoFilter->enableReplacement();
	}
	else {
		videoFilter->disableReplacement();
	}

	bool isBeautificationEnabled = 
		m_settings->value(BEAUTIFICATION_ENABLED, false).toBool();
	if (isBeautificationEnabled) {
		videoFilter->enableBeautification();
	}
	else {
		videoFilter->disableBeautification();
	}

	bool isDenoiseEnabled = m_settings->value(DENOISE_ENABLED, false).toBool();
	if (isDenoiseEnabled) {
		videoFilter->enableDenoise();
	}
	else {
		videoFilter->disableDenoise();
	}

	bool isSmartZoomEnabled = m_settings->value(SMART_ZOOM_ENABLED, false).toBool();
	if (isSmartZoomEnabled) {
		videoFilter->enableSmartZoom();
	}
	else {
		videoFilter->disableSmartZoom();
	}

	bool isLowLightEnabled = m_settings->value(LOW_LIGHT_ADJUSTMENT_ENABLED, false).toBool();
	if (isLowLightEnabled) {
		videoFilter->enableLowLightAdjustment();
	}
	else {
		videoFilter->disableLowLightAdjustment();
	}
	bool isSharpeningEnabled = m_settings->value(SHARPENING_ENABLED, false).toBool();
	if (isSharpeningEnabled) {
		videoFilter->enableSharpening();
	}
	else {
		videoFilter->disableSharpening();
	}

	QString backgroundFilePath = m_settings->value(
		BACKGROUND_FILEPATH,
		defaultBackgroundPath()
	).toString();
	if (!backgroundFilePath.isEmpty()) {
		videoFilter->setBackground(backgroundFilePath);
	}

	float defaultBeautificationLevel = videoFilter->beautificationLevel();
	float beautificationLevel = m_settings->value(
		BEAUTIFICATION_LEVEL,
		defaultBeautificationLevel
	).toFloat();
	beautificationLevel = std::min(std::max(beautificationLevel, 0.0f), 1.0f);
	videoFilter->setBeautificationLevel(beautificationLevel);

	float defaultDenoiseLevel = videoFilter->denoisePower();
	float denoiseLevel = m_settings->value(
		DENOISE_LEVEL,
		defaultDenoiseLevel
	).toFloat();
	denoiseLevel = std::min(std::max(denoiseLevel, 0.0f), 1.0f);
	videoFilter->setDenoisePower(denoiseLevel);

	bool isDenoiseWithFace = m_settings->value(DENOISE_WITH_FACE, false).toBool();
	videoFilter->setDenoiseWithFace(isDenoiseWithFace);

	float defaultZoomLevel = videoFilter->smartZoomLevel();
	float zoomLevel = m_settings->value(ZOOM_LEVEL, defaultZoomLevel).toFloat();
	zoomLevel = std::min(std::max(zoomLevel, 0.0f), 1.0f);
	videoFilter->setSmartZoomLevel(zoomLevel);

	float defaultLowLightPower = videoFilter->getLowLightAdjustmentPower();
	float lowLightPower = m_settings->value(LOW_LIGHT_ADJUSTMENT_POWER, defaultLowLightPower).toFloat();
	lowLightPower = std::min(std::max(lowLightPower, 0.0f), 1.0f);
	videoFilter->setLowLightAdjustmentPower(lowLightPower);

	float defaultSharpeningPower = videoFilter->sharpeningPower();
	float sharpeningPower = m_settings->value(SHARPENING_POWER, defaultSharpeningPower).toFloat();
	sharpeningPower = std::min(std::max(sharpeningPower, 0.0f), 1.0f);
	videoFilter->setSharpeningPower(sharpeningPower);

	QString colorGradingRefPath = 
		m_settings->value(COLOR_GRADING_REFERENCE_PATH).toString();
	if (QFile::exists(colorGradingRefPath)) {
		m_colorGradingRefPath = colorGradingRefPath;
	}

	QString colorFilterFile = 
		m_settings->value(COLOR_FILTER_FILE, QString()).toString();
	if (!colorFilterFile.isEmpty()) {
		m_ui->setCurrentColorFilter(colorFilterFile);
	}

	QString colorCorrectionMode =
		m_settings->value(ENABLED_COLOR_CORRECTION_MODE, QString()).toString();
	if (COLOR_CORRECTION_MODE_AUTO == colorCorrectionMode) {
		videoFilter->enableColorCorrection();
	}
	else if (COLOR_CORRECTION_MODE_FILTER == colorCorrectionMode) {
		QString filePath = selectedColorLutFilePath();
		videoFilter->enableColorFilter(filePath);
	}
	else if (COLOR_CORRECTION_MODE_GRADING == colorCorrectionMode) {
		videoFilter->enableColorGrading(m_colorGradingRefPath);
	}

	return true;
}

QStringList Sample::colorLutSearchPaths() const
{
	const char colorLutsDirName[] = "color_luts";
#ifdef Q_OS_WINDOWS
	QDir appDir(QCoreApplication::applicationDirPath());
	QDir currentDir = QDir::current();

	QStringList paths = { appDir.absoluteFilePath("color_luts") };
	if (currentDir != appDir) {
		paths << currentDir.absoluteFilePath("color_luts");
	}
	return paths;
#elif defined(Q_OS_MACOS)
	QDir resourcesDir = bundleResourcesPath();
	QString lutsDir = resourcesDir.absoluteFilePath(colorLutsDirName);
	return { lutsDir };
#elif defined(Q_OS_LINUX)
	QDir appImageShareDir = appImageUsrSharePath();
	return { appImageShareDir.absoluteFilePath(colorLutsDirName) };
#else
	return {};
#endif
}

QStringList Sample::enumerateColorLutFiles() const
{
	QStringList fileNames;
	for (int i = 0; i < 20; ++i) {
		QString fileName = "Filter " + QString::number(i + 1) + ".cube";
		fileNames << fileName;
	}

	return fileNames;
}

QString Sample::findColorLutFilePath(const QString& fileName) const
{
	for (auto& path : colorLutSearchPaths()) {
		QDir dir(path);
		if (dir.exists(fileName)) {
			return dir.absoluteFilePath(fileName);
		}
	}

	return QString();
}

QString Sample::selectedColorLutFilePath() const
{
	QString lutFileName = m_ui->currentColorFilter();
	QString lutFilePath = findColorLutFilePath(lutFileName);
	return lutFilePath;
}

void Sample::checkCPUPipelineAvailable()
{
	auto videoFilter = m_pipeline->videoFilter();
	bool needDisable = videoFilter->isDenoiseEnabled();
#ifndef Q_OS_WINDOWS
	needDisable |= videoFilter->isBeautificationEnabled();
#endif

	m_ui->cpuRadioButton->setDisabled(needDisable);
}

void Sample::checkGPUOnlyFeaturesEnabled()
{
	bool isGPU = (Backend::gpu == m_pipeline->videoFilter()->backend());
	m_ui->denoiseCheckBox->setEnabled(isGPU);
#ifndef Q_OS_WINDOWS
	m_ui->beautificateCheckBox->setEnabled(isGPU);
#endif
}

bool Sample::setCameraByName(const QString& name)
{
#ifdef Q_OS_LINUX
	std::string devicePath = findDevicePathByName(name);
	if (devicePath.empty()) {
		return false;
	}
	m_pipeline->setMediaPath(devicePath);
#else
	int deviceIndex = findDeviceIndexByName(name);
	if (-1 == deviceIndex) {
		return false;
	}
	m_pipeline->setDeviceIndex(deviceIndex);
#endif

	return true;
}

QString Sample::stringFromNumber(double number)
{
	return QString::number(number, 'g', 2);
}

bool Sample::enableColorFilter(const QString& lutFilePath)
{
	bool result = m_pipeline->videoFilter()->enableColorFilter(lutFilePath);
	if (!result) {
		if (!m_pipeline->videoFilter()->isColorFilterEnabled()) {
			m_ui->colorFilterCheckbox->setChecked(false);
			m_ui->colorIntensitySlider->setEnabled(false);
			QMessageBox::warning(this, "Error", "Failure to enable Color Filter");
		}
		else {
			QMessageBox::warning(this, "Error", "Failure to switch Color Filter");
		}		
		return false;
	}
	m_ui->colorIntensitySlider->setEnabled(true);
	onColorIntensitySliderMoved();
	m_settings->setValue(ENABLED_COLOR_CORRECTION_MODE, COLOR_CORRECTION_MODE_FILTER);

	return true;
}

void Sample::onCameraPicked(const QString& cameraName)
{
	if (setCameraByName(cameraName)) {
		m_settings->setValue(CAMERA_NAME, cameraName);
	}
}

void Sample::setCameraScale(const QSize& scale)
{
	m_pipeline->trySetFrameSize(scale.width(), scale.height());
	m_settings->setValue(CAMERA_SCALE, scale);
}

void Sample::toggleBlurEnabled()
{
	bool enabled = m_pipeline->videoFilter()->isBlurEnabled();
	if (enabled) {
		m_pipeline->videoFilter()->disableBlur();
		m_settings->setValue(BLUR_ENABLED, false);
		return;
	}
	bool result = m_pipeline->videoFilter()->enableBlur();
	if (result) {
		m_settings->setValue(BLUR_ENABLED, true);
		return;
	}

	m_ui->blurCheckBox->setChecked(false);
	QMessageBox::warning(this, "Error", "Failure to enable Blur");
}

void Sample::toggleDenoiseEnabled()
{
	bool enabled = m_pipeline->videoFilter()->isDenoiseEnabled();
	if (enabled) {
		m_pipeline->videoFilter()->disableDenoise();
		m_ui->denoisePowerSlider->setEnabled(false);
		m_ui->denoiseWithFaceCheckBox->setEnabled(false);
		m_ui->denoisePowerLabel->setText(QString());
		m_settings->setValue(DENOISE_ENABLED, false);
		checkCPUPipelineAvailable();
		return;
	}
	bool result = m_pipeline->videoFilter()->enableDenoise();
	m_ui->denoisePowerSlider->setEnabled(result);
	m_ui->denoiseWithFaceCheckBox->setEnabled(result);
	if (!result) {
		m_ui->denoiseCheckBox->setChecked(false);
		QMessageBox::warning(this, "Error", "Failure to enable Denoise");
		return;
	}

	float denoiseLevel = m_pipeline->videoFilter()->denoisePower();
	bool isDenoiseWithFace = m_pipeline->videoFilter()->isDenoiseWithFace();
	int sliderValue = static_cast<int>(
		denoiseLevel * m_ui->denoisePowerSlider->maximum()
	);
	m_ui->denoisePowerSlider->setValue(sliderValue);
	m_ui->denoisePowerLabel->setText(stringFromNumber(denoiseLevel));
	m_ui->denoiseWithFaceCheckBox->setChecked(isDenoiseWithFace);
	checkCPUPipelineAvailable();
	m_settings->setValue(DENOISE_ENABLED, true);
	m_settings->setValue(DENOISE_LEVEL, denoiseLevel);
	m_settings->setValue(DENOISE_WITH_FACE, isDenoiseWithFace);
}

void Sample::toggleDenoiseWithFaceClicked()
{
	bool isDenoiseWithFace = !m_pipeline->videoFilter()->isDenoiseWithFace();
	m_pipeline->videoFilter()->setDenoiseWithFace(isDenoiseWithFace);
	m_settings->setValue(DENOISE_WITH_FACE, isDenoiseWithFace);
}

void Sample::toggleReplaceEnabled()
{
	bool enabled = m_pipeline->videoFilter()->isReplaceEnabled();
	if (enabled) {
		m_pipeline->videoFilter()->disableReplacement();
		m_settings->setValue(REPLACE_ENABLED, false);
		return;
	}
	bool result = m_pipeline->videoFilter()->enableReplacement();
	if (result) {
		m_settings->setValue(REPLACE_ENABLED, true);
		return;
	}

	m_ui->replaceCheckBox->setChecked(false);
	QMessageBox::warning(this, "Error", "Failure to enable Replace");
}

void Sample::toggleBeautificateEnabled()
{
	bool enabled = m_pipeline->videoFilter()->isBeautificationEnabled();
	if (enabled) {
		m_pipeline->videoFilter()->disableBeautification();
		m_ui->beautificationLevelSlider->setEnabled(false);
		m_ui->beautificationLevelLabel->setText(QString());
		m_settings->setValue(BEAUTIFICATION_ENABLED, false);
		checkCPUPipelineAvailable();
		return;
	}
	bool result = m_pipeline->videoFilter()->enableBeautification();
	m_ui->beautificationLevelSlider->setEnabled(result);
	if (!result) {
		m_ui->beautificateCheckBox->setChecked(false);
		QMessageBox::warning(this, "Error", "Failure to enable Beautificate");
		return;
	}

	float level = m_pipeline->videoFilter()->beautificationLevel();
	int sliderValue = 
		static_cast<int>(level * m_ui->beautificationLevelSlider->maximum());
	m_ui->beautificationLevelSlider->setValue(sliderValue);
	m_ui->beautificationLevelLabel->setText(stringFromNumber(level));
	checkCPUPipelineAvailable();
	m_settings->setValue(BEAUTIFICATION_ENABLED, true);
	m_settings->setValue(BEAUTIFICATION_LEVEL, level);
}

void Sample::toggleCorrectColorsEnabled()
{
	if (m_pipeline->videoFilter()->isColorGradingEnabled()) {
		toggleColorGradingEnabled();
	} else if (m_pipeline->videoFilter()->isColorFilterEnabled()) {
		toggleColorFilterEnabled();
	}
	bool enabled = m_pipeline->videoFilter()->isColorCorrectionEnabled();
	if (enabled) {
		m_pipeline->videoFilter()->disableColorCorrection();
		m_ui->correctColorsCheckbox->setChecked(false);
		m_ui->colorIntensitySlider->setEnabled(false);
		m_settings->remove(ENABLED_COLOR_CORRECTION_MODE);
		return;
	}
	bool result = m_pipeline->videoFilter()->enableColorCorrection();
	if (!result) {
		m_ui->correctColorsCheckbox->setChecked(false);
		m_ui->colorIntensitySlider->setEnabled(false);
		QMessageBox::warning(this, "Error", "Failure to enable Color Correction");
		return;
	}

	m_ui->colorIntensitySlider->setEnabled(true);
	onColorIntensitySliderMoved();
	m_settings->setValue(ENABLED_COLOR_CORRECTION_MODE, COLOR_CORRECTION_MODE_AUTO);
}

void Sample::toggleSmartZoomEnabled()
{
	bool enabled = m_pipeline->videoFilter()->isSmartZoomEnabled();
	if (enabled) {
		m_pipeline->videoFilter()->disableSmartZoom();
		m_settings->setValue(SMART_ZOOM_ENABLED, false);
		m_ui->zoomLevelSlider->setDisabled(true);
		return;
	}
	bool result = m_pipeline->videoFilter()->enableSmartZoom();
	if (!result) {
		m_ui->smartZoomCheckBox->setChecked(false);
		QMessageBox::warning(this, "Error", "Failure to enable Smart Zoom");
		return;
	}
	m_ui->zoomLevelSlider->setEnabled(true);
	m_settings->setValue(SMART_ZOOM_ENABLED, true);
}

void Sample::toggleColorGradingEnabled()
{
	if (m_pipeline->videoFilter()->isColorCorrectionEnabled()) {
		toggleCorrectColorsEnabled();
	} else if (m_pipeline->videoFilter()->isColorFilterEnabled()) {
		toggleColorFilterEnabled();
	}
	bool enabled = m_pipeline->videoFilter()->isColorGradingEnabled();
	if (enabled) {
		m_pipeline->videoFilter()->disableColorGrading();
		m_ui->colorGradingCheckbox->setChecked(false);
		m_ui->colorIntensitySlider->setEnabled(false);
		m_settings->remove(ENABLED_COLOR_CORRECTION_MODE);
		return;
	}
	if (m_colorGradingRefPath.isEmpty()) {
		openColorGradingReference();
		if (m_colorGradingRefPath.isEmpty()) {
			m_ui->colorGradingCheckbox->setChecked(false);
			return;
		}
	}

	bool result = m_pipeline->videoFilter()->enableColorGrading(m_colorGradingRefPath);
	if (!result) {
		m_ui->colorGradingCheckbox->setChecked(false);
		m_ui->colorIntensitySlider->setEnabled(false);
		QMessageBox::warning(this, "Error", "Failure to enable Color Grading");
		return;
	}
	m_settings->setValue(ENABLED_COLOR_CORRECTION_MODE, COLOR_CORRECTION_MODE_GRADING);
	m_ui->colorIntensitySlider->setEnabled(true);
	onColorIntensitySliderMoved();
}

void Sample::toggleColorFilterEnabled()
{
	bool enabled = m_pipeline->videoFilter()->isColorFilterEnabled();
	if (enabled) {
		m_pipeline->videoFilter()->disableColorFilter();
		m_ui->colorFilterCheckbox->setChecked(false);
		m_ui->colorIntensitySlider->setEnabled(false);
		m_settings->remove(ENABLED_COLOR_CORRECTION_MODE);
		return;
	}

	if (m_pipeline->videoFilter()->isColorGradingEnabled()) {
		toggleColorGradingEnabled();
	}
	else if (m_pipeline->videoFilter()->isColorCorrectionEnabled()) {
		toggleCorrectColorsEnabled();
	}

	auto lutFilePath = selectedColorLutFilePath();
	if (lutFilePath.isEmpty()) {
		onColorLUTFileNotFoundError(m_ui->currentColorFilter());
	}
	enableColorFilter(lutFilePath);
}

void Sample::toggleLowLightAdjustment()
{
	bool enabled = m_pipeline->videoFilter()->isLowLightAdjustmentEnabled();
	if (enabled) {
		m_pipeline->videoFilter()->disableLowLightAdjustment();
		m_ui->lowLightAdjustmentCheckbox->setChecked(false);
		m_ui->lowLightAdjustmentPowerSlider->setEnabled(false);
		m_settings->setValue(LOW_LIGHT_ADJUSTMENT_ENABLED, false);
		return;
	}

	bool ok = m_pipeline->videoFilter()->enableLowLightAdjustment();
	m_ui->lowLightAdjustmentCheckbox->setChecked(ok);
	m_ui->lowLightAdjustmentPowerSlider->setEnabled(ok);
	m_settings->setValue(LOW_LIGHT_ADJUSTMENT_ENABLED, ok);
	if (!ok) {
		QMessageBox::warning(this, "Error", "Failure to enable Low Light");
	}
}

void Sample::toggleSharpening()
{
	bool enabled = m_pipeline->videoFilter()->isSharpeningEnabled();
	if (enabled) {
		m_pipeline->videoFilter()->disableSharpening();
		m_ui->sharpeningCheckbox->setChecked(false);
		m_ui->sharpeningPowerSlider->setEnabled(false);
		m_settings->setValue(SHARPENING_ENABLED, false);
		return;
	}

	bool ok = m_pipeline->videoFilter()->enableSharpening();
	m_ui->sharpeningCheckbox->setChecked(ok);
	m_ui->sharpeningPowerSlider->setEnabled(ok);
	m_settings->setValue(SHARPENING_ENABLED, ok);
	if (!ok) {
		QMessageBox::warning(this, "Error", "Failure to enable Sharpening");
	}
}

void Sample::onColorFilterPicked(const QString& fileName)
{
	if (m_pipeline->videoFilter()->isColorFilterEnabled()) {
		auto lutFilePath = selectedColorLutFilePath();
		if (lutFilePath.isEmpty()) {
			onColorLUTFileNotFoundError(m_ui->currentColorFilter());
		}
		bool ok = enableColorFilter(lutFilePath);
		if (!ok) {
			return;
		}
	}
	m_settings->setValue(COLOR_FILTER_FILE, fileName);
}

void Sample::onColorIntensitySliderMoved()
{
	auto value = static_cast<float>(m_ui->colorIntensitySlider->value());
	value /= static_cast<float>(m_ui->colorIntensitySlider->maximum());

	if (m_pipeline->videoFilter()->isColorCorrectionEnabled()) {
		m_pipeline->videoFilter()->setColorCorrectionPower(value);
	} else if (m_pipeline->videoFilter()->isColorGradingEnabled()) {
		m_pipeline->videoFilter()->setColorGradingPower(value);
	} else if (m_pipeline->videoFilter()->isColorFilterEnabled()) {
		m_pipeline->videoFilter()->setColorFilterPower(value);
	}
}

void Sample::onColorLUTFileNotFoundError(const QString& fileName)
{
	QString msg = 
		QString("Could not find Color LUT file '%1' in paths:").arg(fileName);
	for (auto& searchPath : colorLutSearchPaths()) {
		msg.append('\n');
		msg.append(QDir::toNativeSeparators(searchPath));
	}

	QMessageBox::warning(this, "File not found", msg);
}

void Sample::onLowLightAdjustmentPowerSliderMoved()
{
	float value = 
		float(m_ui->lowLightAdjustmentPowerSlider->value()) / float(m_ui->lowLightAdjustmentPowerSlider->maximum());
	m_pipeline->videoFilter()->setLowLightAdjustmentPower(value);
	m_ui->lowLightAdjustmentPowerLabel->setText(stringFromNumber(value));
	m_settings->setValue(LOW_LIGHT_ADJUSTMENT_POWER, value);
}

void Sample::onSharpeningPowerSliderMoved()
{
	float value =
		float(m_ui->sharpeningPowerSlider->value()) / float(m_ui->sharpeningPowerSlider->maximum());
	m_pipeline->videoFilter()->setSharpeningPower(value);
	m_ui->sharpeningPowerLabel->setText(stringFromNumber(value));
	m_settings->setValue(SHARPENING_POWER, value);
}

void Sample::openBackground()
{
	QString filepath = QFileDialog::getOpenFileName(
			this,
			"Background",
			QString(),
			dialogImageFilter()
	);
	if (!filepath.isEmpty()) {
		m_pipeline->videoFilter()->setBackground(filepath);
		m_settings->setValue(BACKGROUND_FILEPATH, filepath);
	}
}

void Sample::openColorGradingReference()
{
	auto colorGradingRefPath = QFileDialog::getOpenFileName(
		this,
		"Reference",
		QString(),
		dialogImageFilter()
	);
	if (colorGradingRefPath.isEmpty()) {
		return;
	}

	if (m_pipeline->videoFilter()->isColorGradingEnabled()) {
		bool ok = m_pipeline->videoFilter()->enableColorGrading(colorGradingRefPath);
		if (!ok) {
			return;
		}
	}

	m_colorGradingRefPath = colorGradingRefPath;
	m_settings->setValue(COLOR_GRADING_REFERENCE_PATH, colorGradingRefPath);
}

void Sample::onBeautificationLevelSliderMoved()
{
	float value = float(m_ui->beautificationLevelSlider->value());
	value /= float(m_ui->beautificationLevelSlider->maximum());

	m_pipeline->videoFilter()->setBeautificationLevel(value);
	m_ui->beautificationLevelLabel->setText(stringFromNumber(value));
	m_settings->setValue(BEAUTIFICATION_LEVEL, value);
}

void Sample::onDenoiseLevelSliderMoved()
{
	float value = float(m_ui->denoisePowerSlider->value());
	value /= float(m_ui->denoisePowerSlider->maximum());

	m_pipeline->videoFilter()->setDenoisePower(value);
	m_ui->denoisePowerLabel->setText(stringFromNumber(value));
	m_settings->setValue(DENOISE_LEVEL, value);
}

void Sample::onZoomLevelSliderMoved()
{
	float value = float(m_ui->zoomLevelSlider->value());
	value /= float(m_ui->zoomLevelSlider->maximum());

	m_pipeline->videoFilter()->setSmartZoomLevel(value);
	m_ui->zoomLevelLabel->setText(stringFromNumber(value));
	m_settings->setValue(ZOOM_LEVEL, value);
}

void Sample::processFrame(const QImage& frame)
{
	m_ui->frameView->present(frame);
}

void Sample::switchToCPU()
{
	bool ok = m_pipeline->videoFilter()->setBackend(Backend::cpu);
	m_ui->cpuRadioButton->setChecked(ok);
	m_ui->gpuRadioButton->setChecked(!ok);
	checkGPUOnlyFeaturesEnabled();

	if (!ok) {
		QMessageBox::warning(this, "Error", "Failure to initialize Software pipeline");
		return;
	}
	m_settings->setValue(BACKEND, "CPU");
}

void Sample::switchToGPU()
{
	bool ok = m_pipeline->videoFilter()->setBackend(Backend::gpu);
	m_ui->cpuRadioButton->setChecked(!ok);
	m_ui->gpuRadioButton->setChecked(ok);
	checkGPUOnlyFeaturesEnabled();

	if (!ok) {
		QMessageBox::warning(this, "Error", "Failure to initialize GPU pipeline");
		return;
	}
	m_settings->setValue(BACKEND, "GPU");
}

void Sample::onPresetPicked(Preset preset)
{
	bool ok = m_pipeline->videoFilter()->setPreset(preset);
	if (!ok) {
		QMessageBox::warning(this, "Error", "Failure to switch preset");
		Preset actualPreset = m_pipeline->videoFilter()->preset();
		m_ui->setCurrentPreset(actualPreset);
		return;
	}

	m_settings->setValue(PRESET, int(preset));
}

void Sample::updateMetrics()
{
	auto metrics = m_pipeline->metrics();
	if (metrics->isCameraSwitch()) {
		m_ui->metricsView->setCameraSwitch();
	}
	else if (metrics->hasCameraError()) {
		m_ui->metricsView->setCameraError();
	}
	else {
		m_ui->metricsView->update(metrics->avgTimePerFrame(), metrics->lastFrameSize());
	}
}

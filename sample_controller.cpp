#include "sample_controller.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QMetaProperty>

#include <utility>

namespace {

constexpr int colorFilterCount = 20;

QString appDirFile(const QString& name)
{
	QDir dir(QCoreApplication::applicationDirPath());
#ifdef Q_OS_MACOS
	dir.cd(QStringLiteral("../Resources"));
#endif
	return dir.absoluteFilePath(name);
}

QString colorFilterName(int index)
{
	return QString("Filter %1").arg(index + 1);
}

QByteArray toSDKPath(const QString& filePath)
{
	return QDir::toNativeSeparators(filePath).toUtf8();
}

constexpr bool succeeded(tsvb::PipelineError error)
{
	return (tsvb::PipelineErrorCode::ok == error);
}

QString describe(tsvb::PipelineError error)
{
	switch (error) {
	case tsvb::PipelineErrorCode::invalidArgument:
		return QStringLiteral("invalid argument");
	case tsvb::PipelineErrorCode::noFeaturesEnabled:
		return QStringLiteral("no effect is enabled");
	case tsvb::PipelineErrorCode::engineInitializationError:
		return QStringLiteral("the processing engine could not be initialized");
	case tsvb::PipelineErrorCode::resourceAllocationError:
		return QStringLiteral("not enough memory or other resources");
	case tsvb::PipelineErrorCode::notSupportedByBackend:
		return QStringLiteral("not available on this backend");
	case tsvb::PipelineErrorCode::enabledUnsupportedFeatures:
		return QStringLiteral("an enabled effect is not available on that backend");
	case tsvb::PipelineErrorCode::invalidUsage:
		return QStringLiteral("the SDK is used against its documentation");
	default:
		return QStringLiteral("error %1").arg(error);
	}
}

}

SampleController::SampleController(QObject* parent)
	: QObject(parent)
	, _camera(_filter.isValid() ? &_filter : nullptr)
{
	connect(&_camera, &CameraController::errorOccurred, this, &SampleController::errorOccurred);

	if (_filter.isValid()) {
		tsvb::IPipeline& pipeline = _filter.pipeline();
		SDKUniquePtr<tsvb::IPipelineConfiguration> config(pipeline.copyConfiguration());
		_gpuBackend = (tsvb::GPU == config->getBackend());
		_preset = config->getSegmentationPreset();
		_appleNeuralEngineEnabled = (0 != (config->getSegmentationMLBackends() & tsvb::mlBackendAppleNeuralEngine));

		_beautificationLevel = pipeline.getBeautificationLevel();
		_smartZoomLevel = pipeline.getSmartZoomLevel();
		_portraitLightStrength = pipeline.getPortraitLightingParam(tsvb::portraitLightingParamLightStrength, nullptr);
		_portraitBackgroundDarkening = pipeline.getPortraitLightingParam(tsvb::portraitLightingParamBackgroundDarkening, nullptr);
		_lowLightPower = pipeline.getLowLightAdjustmentPower();
		_sharpeningPower = pipeline.getSharpeningPower();

		auto colorParam = [&pipeline](tsvb::ColorCorrectionParam param) {
			return pipeline.getColorCorrectionParam(param, nullptr);
		};
		_colorCorrectionAutoMode = (0 != colorParam(tsvb::colorCorrectionParamAutoMode));
		_colorCorrectionWhiteBalance = colorParam(tsvb::colorCorrectionParamWhiteBalance);
		_colorCorrectionExposure = colorParam(tsvb::colorCorrectionParamExposure);
		_colorCorrectionTemperature = colorParam(tsvb::colorCorrectionParamTemperature);
		_colorCorrectionTint = colorParam(tsvb::colorCorrectionParamTint);
		_colorCorrectionVibrance = colorParam(tsvb::colorCorrectionParamVibrance);
		_colorCorrectionContrast = colorParam(tsvb::colorCorrectionParamContrast);
		_colorCorrectionUpdatePeriodMs = colorParam(tsvb::colorCorrectionParamUpdatePeriodMs);
		// The SDK has one power for all three color modes.
		_colorCorrectionPower = colorParam(tsvb::colorCorrectionParamPower);
		_colorGradingPower = _colorCorrectionPower;
		_colorFilterPower = _colorCorrectionPower;

		const QString background = appDirFile("background.jpg");
		if (loadBackground(background)) {
			_backgroundFile = QUrl::fromLocalFile(background);
		}
	}
	connectParams();

	const QMetaObject* meta = metaObject();
	for (int i = meta->propertyOffset(); i < meta->propertyCount(); ++i) {
		const QMetaProperty property = meta->property(i);
		if (property.isWritable()) {
			_defaults.insert(QString::fromLatin1(property.name()), property.read(this));
		}
	}
}

SampleController::~SampleController() = default;

CameraController* SampleController::camera()
{
	return &_camera;
}

bool SampleController::sdkAvailable() const
{
	return _filter.isValid();
}

bool SampleController::gpuBackend() const
{
	return _gpuBackend;
}

void SampleController::setGpuBackend(bool gpu)
{
	if (!_filter.isValid() || (gpu == _gpuBackend)) {
		emit gpuBackendChanged();
		return;
	}

	const QString backend = gpu ? QStringLiteral("GPU") : QStringLiteral("CPU");
	const tsvb::PipelineError error = (!gpu && _beautificationEnabled)
		? tsvb::PipelineError(tsvb::PipelineErrorCode::enabledUnsupportedFeatures)
		: configure([gpu](tsvb::IPipelineConfiguration& config) {
			config.setBackend(gpu ? tsvb::GPU : tsvb::CPU);
		});
	if (succeeded(error)) {
		_gpuBackend = gpu;
	}
	else if (tsvb::PipelineErrorCode::enabledUnsupportedFeatures == error) {
		emit errorOccurred(QStringLiteral("Some enabled effects are not supported by the %1 backend. Turn them off before switching the backend.").arg(backend));
	}
	else {
		emit errorOccurred(QStringLiteral("Failed to switch to the %1 backend: %2.").arg(backend, describe(error)));
	}
	emit gpuBackendChanged();
}

int SampleController::preset() const
{
	return _preset;
}

void SampleController::setPreset(int preset)
{
	if (!_filter.isValid() || (preset == _preset)) {
		emit presetChanged();
		return;
	}

	const tsvb::PipelineError error = configure([preset](tsvb::IPipelineConfiguration& config) {
		config.setSegmentationPreset(preset);
	});
	if (succeeded(error)) {
		_preset = preset;
	}
	else {
		emit errorOccurred(QStringLiteral("Failed to switch the preset: %1.").arg(describe(error)));
	}
	emit presetChanged();
}

bool SampleController::appleNeuralEngineEnabled() const
{
	return _appleNeuralEngineEnabled;
}

void SampleController::setAppleNeuralEngineEnabled(bool enabled)
{
	if (!_filter.isValid() || (enabled == _appleNeuralEngineEnabled)) {
		emit appleNeuralEngineEnabledChanged();
		return;
	}

	const tsvb::PipelineError error = configure([enabled](tsvb::IPipelineConfiguration& config) {
		int backends = config.getSegmentationMLBackends();
		if (enabled) {
			backends |= tsvb::mlBackendAppleNeuralEngine;
		}
		else {
			backends &= ~int(tsvb::mlBackendAppleNeuralEngine);
		}
		config.setSegmentationMLBackends(backends);
	});
	if (succeeded(error)) {
		_appleNeuralEngineEnabled = enabled;
	}
	else {
		emit errorOccurred(QStringLiteral("Failed to switch the Apple Neural Engine: %1.").arg(describe(error)));
	}
	emit appleNeuralEngineEnabledChanged();
}

bool SampleController::blurEnabled() const
{
	return _blurEnabled;
}

void SampleController::setBlurEnabled(bool enabled)
{
	toggle(
		_blurEnabled, enabled,
		[this](tsvb::IPipeline& pipeline) { return pipeline.enableBlurBackground(_blurPower); },
		[](tsvb::IPipeline& pipeline) { pipeline.disableBackgroundBlur(); },
		&SampleController::blurEnabledChanged,
		QStringLiteral("Blur")
	);
}

bool SampleController::replaceEnabled() const
{
	return _replaceEnabled;
}

void SampleController::setReplaceEnabled(bool enabled)
{
	toggle(
		_replaceEnabled, enabled,
		[this](tsvb::IPipeline& pipeline) {
			tsvb::IReplacementController* controller = nullptr;
			const tsvb::PipelineError error = pipeline.enableReplaceBackground(&controller);
			_replacementController.reset(controller);
			if ((nullptr != _replacementController) && (nullptr != _background)) {
				_replacementController->setBackgroundImage(_background.get());
			}
			return error;
		},
		[this](tsvb::IPipeline& pipeline) {
			pipeline.disableReplaceBackground();
			_replacementController = nullptr;
		},
		&SampleController::replaceEnabledChanged,
		QStringLiteral("Replace")
	);
}

QUrl SampleController::backgroundFile() const
{
	return _backgroundFile;
}

void SampleController::setBackgroundFile(const QUrl& file)
{
	if (!_filter.isValid() || !file.isLocalFile() || (file == _backgroundFile)) {
		emit backgroundFileChanged();
		return;
	}

	if (loadBackground(file.toLocalFile())) {
		_backgroundFile = file;
	}
	else {
		emit errorOccurred(QStringLiteral("Failed to load the background: %1")
			.arg(QDir::toNativeSeparators(file.toLocalFile())));
	}
	emit backgroundFileChanged();
}

bool SampleController::noiseSuppressionEnabled() const
{
	return _noiseSuppressionEnabled;
}

void SampleController::setNoiseSuppressionEnabled(bool enabled)
{
	toggle(
		_noiseSuppressionEnabled, enabled,
		[](tsvb::IPipeline& pipeline) { return pipeline.enableNoiseSuppression(); },
		[](tsvb::IPipeline& pipeline) { pipeline.disableNoiseSuppression(); },
		&SampleController::noiseSuppressionEnabledChanged,
		QStringLiteral("Noise Suppression")
	);
}

bool SampleController::beautificationEnabled() const
{
	return _beautificationEnabled;
}

void SampleController::setBeautificationEnabled(bool enabled)
{
	if (enabled && !_gpuBackend && _filter.isValid() && !_beautificationEnabled) {
		emit errorOccurred(QStringLiteral("Beautification is not available on the CPU backend."));
		emit beautificationEnabledChanged();
		return;
	}
	toggle(
		_beautificationEnabled, enabled,
		[this](tsvb::IPipeline& pipeline) {
			const tsvb::PipelineError error = pipeline.enableBeautification();
			if (succeeded(error)) {
				pipeline.setBeautificationLevel(float(_beautificationLevel));
			}
			return error;
		},
		[](tsvb::IPipeline& pipeline) { pipeline.disableBeautification(); },
		&SampleController::beautificationEnabledChanged,
		QStringLiteral("Beautification")
	);
}

bool SampleController::smartZoomEnabled() const
{
	return _smartZoomEnabled;
}

void SampleController::setSmartZoomEnabled(bool enabled)
{
	toggle(
		_smartZoomEnabled, enabled,
		[this](tsvb::IPipeline& pipeline) {
			const tsvb::PipelineError error = pipeline.enableSmartZoom();
			if (succeeded(error)) {
				pipeline.setSmartZoomLevel(float(_smartZoomLevel));
			}
			return error;
		},
		[](tsvb::IPipeline& pipeline) { pipeline.disableSmartZoom(); },
		&SampleController::smartZoomEnabledChanged,
		QStringLiteral("Smart Zoom")
	);
}

bool SampleController::portraitLightingEnabled() const
{
	return _portraitLightingEnabled;
}

void SampleController::setPortraitLightingEnabled(bool enabled)
{
	toggle(
		_portraitLightingEnabled, enabled,
		[](tsvb::IPipeline& pipeline) { return pipeline.enablePortraitLighting(); },
		[](tsvb::IPipeline& pipeline) { pipeline.disablePortraitLighting(); },
		&SampleController::portraitLightingEnabledChanged,
		QStringLiteral("Portrait Lighting")
	);
}

double SampleController::portraitLightStrength() const
{
	return _portraitLightStrength;
}

void SampleController::setPortraitLightStrength(double strength)
{
	setPortraitLightingParam(
		tsvb::portraitLightingParamLightStrength,
		_portraitLightStrength, strength,
		&SampleController::portraitLightStrengthChanged
	);
}

double SampleController::portraitBackgroundDarkening() const
{
	return _portraitBackgroundDarkening;
}

void SampleController::setPortraitBackgroundDarkening(double darkening)
{
	setPortraitLightingParam(
		tsvb::portraitLightingParamBackgroundDarkening,
		_portraitBackgroundDarkening, darkening,
		&SampleController::portraitBackgroundDarkeningChanged
	);
}

bool SampleController::lowLightEnabled() const
{
	return _lowLightEnabled;
}

void SampleController::setLowLightEnabled(bool enabled)
{
	toggle(
		_lowLightEnabled, enabled,
		[](tsvb::IPipeline& pipeline) { return pipeline.enableLowLightAdjustment(); },
		[](tsvb::IPipeline& pipeline) { pipeline.disableLowLightAdjustment(); },
		&SampleController::lowLightEnabledChanged,
		QStringLiteral("Low Light Adjustment")
	);
}

bool SampleController::sharpeningEnabled() const
{
	return _sharpeningEnabled;
}

void SampleController::setSharpeningEnabled(bool enabled)
{
	toggle(
		_sharpeningEnabled, enabled,
		[](tsvb::IPipeline& pipeline) { return pipeline.enableSharpening(); },
		[](tsvb::IPipeline& pipeline) { pipeline.disableSharpening(); },
		&SampleController::sharpeningEnabledChanged,
		QStringLiteral("Sharpening")
	);
}

bool SampleController::colorCorrectionEnabled() const
{
	return (ColorMode::correction == _colorMode);
}

void SampleController::setColorCorrectionEnabled(bool enabled)
{
	if (enabled == colorCorrectionEnabled()) {
		emit colorModeChanged();
		return;
	}
	setColorMode(enabled ? ColorMode::correction : ColorMode::none);
}

bool SampleController::colorCorrectionAutoMode() const
{
	return _colorCorrectionAutoMode;
}

void SampleController::setColorCorrectionAutoMode(bool autoMode)
{
	if (_filter.isValid()) {
		const tsvb::PipelineError error = _filter.pipeline().setColorCorrectionParam(
			tsvb::colorCorrectionParamAutoMode,
			autoMode ? 1.0f : 0.0f
		);
		if (succeeded(error)) {
			_colorCorrectionAutoMode = autoMode;
		}
	}
	emit colorCorrectionAutoModeChanged();
}

double SampleController::colorCorrectionWhiteBalance() const
{
	return _colorCorrectionWhiteBalance;
}

void SampleController::setColorCorrectionWhiteBalance(double value)
{
	setColorCorrectionParam(
		tsvb::colorCorrectionParamWhiteBalance,
		_colorCorrectionWhiteBalance, value,
		&SampleController::colorCorrectionWhiteBalanceChanged
	);
}

double SampleController::colorCorrectionExposure() const
{
	return _colorCorrectionExposure;
}

void SampleController::setColorCorrectionExposure(double value)
{
	setColorCorrectionParam(
		tsvb::colorCorrectionParamExposure,
		_colorCorrectionExposure, value,
		&SampleController::colorCorrectionExposureChanged
	);
}

double SampleController::colorCorrectionTemperature() const
{
	return _colorCorrectionTemperature;
}

void SampleController::setColorCorrectionTemperature(double value)
{
	setColorCorrectionParam(
		tsvb::colorCorrectionParamTemperature,
		_colorCorrectionTemperature, value,
		&SampleController::colorCorrectionTemperatureChanged
	);
}

double SampleController::colorCorrectionTint() const
{
	return _colorCorrectionTint;
}

void SampleController::setColorCorrectionTint(double value)
{
	setColorCorrectionParam(
		tsvb::colorCorrectionParamTint,
		_colorCorrectionTint, value,
		&SampleController::colorCorrectionTintChanged
	);
}

double SampleController::colorCorrectionVibrance() const
{
	return _colorCorrectionVibrance;
}

void SampleController::setColorCorrectionVibrance(double value)
{
	setColorCorrectionParam(
		tsvb::colorCorrectionParamVibrance,
		_colorCorrectionVibrance, value,
		&SampleController::colorCorrectionVibranceChanged
	);
}

double SampleController::colorCorrectionContrast() const
{
	return _colorCorrectionContrast;
}

void SampleController::setColorCorrectionContrast(double value)
{
	setColorCorrectionParam(
		tsvb::colorCorrectionParamContrast,
		_colorCorrectionContrast, value,
		&SampleController::colorCorrectionContrastChanged
	);
}

double SampleController::colorCorrectionUpdatePeriodMs() const
{
	return _colorCorrectionUpdatePeriodMs;
}

void SampleController::setColorCorrectionUpdatePeriodMs(double value)
{
	setColorCorrectionParam(
		tsvb::colorCorrectionParamUpdatePeriodMs,
		_colorCorrectionUpdatePeriodMs, value,
		&SampleController::colorCorrectionUpdatePeriodMsChanged
	);
}

bool SampleController::colorGradingEnabled() const
{
	return (ColorMode::grading == _colorMode);
}

void SampleController::setColorGradingEnabled(bool enabled)
{
	if (enabled == colorGradingEnabled()) {
		emit colorModeChanged();
		return;
	}
	setColorMode(enabled ? ColorMode::grading : ColorMode::none);
}

QUrl SampleController::colorGradingReference() const
{
	return _colorGradingReference;
}

void SampleController::setColorGradingReference(const QUrl& file)
{
	if (!file.isLocalFile() || (file == _colorGradingReference)) {
		emit colorGradingReferenceChanged();
		return;
	}

	const QUrl previous = std::exchange(_colorGradingReference, file);
	if (colorGradingEnabled()) {
		const tsvb::PipelineError error = applyColorMode(ColorMode::grading);
		if (!succeeded(error)) {
			_colorGradingReference = previous;
			emit errorOccurred(QStringLiteral("Failed to use this color grading reference: %1 (%2).")
				.arg(QDir::toNativeSeparators(file.toLocalFile()), describe(error)));
		}
	}
	emit colorGradingReferenceChanged();
}

bool SampleController::colorFilterEnabled() const
{
	return (ColorMode::filter == _colorMode);
}

void SampleController::setColorFilterEnabled(bool enabled)
{
	if (enabled == colorFilterEnabled()) {
		emit colorModeChanged();
		return;
	}
	setColorMode(enabled ? ColorMode::filter : ColorMode::none);
}

QStringList SampleController::colorFilters() const
{
	QStringList names;
	for (int i = 0; i < colorFilterCount; ++i) {
		names << colorFilterName(i);
	}
	return names;
}

int SampleController::colorFilterIndex() const
{
	return _colorFilterIndex;
}

void SampleController::setColorFilterIndex(int index)
{
	if ((index == _colorFilterIndex) || (index < 0) || (index >= colorFilterCount)) {
		emit colorFilterIndexChanged();
		return;
	}

	const int previous = std::exchange(_colorFilterIndex, index);
	if (colorFilterEnabled()) {
		const tsvb::PipelineError error = applyColorMode(ColorMode::filter);
		if (!succeeded(error)) {
			// The SDK keeps the filter it had.
			_colorFilterIndex = previous;
			emit errorOccurred(QStringLiteral("Failed to switch the color filter: %1 (%2).")
				.arg(QDir::toNativeSeparators(colorFilterPath(index)), describe(error)));
		}
	}
	emit colorFilterIndexChanged();
}

SampleController::ColorMode SampleController::colorMode() const
{
	return _colorMode;
}

void SampleController::setColorMode(ColorMode mode)
{
	if (!_filter.isValid() || (mode == _colorMode)) {
		emit colorModeChanged();
		return;
	}

	if ((ColorMode::grading == mode) && _colorGradingReference.isEmpty()) {
		emit colorModeChanged();
		return;
	}

	const tsvb::PipelineError error = applyColorMode(mode);
	if (!succeeded(error)) {
		switch (mode) {
		case ColorMode::correction:
			emit errorOccurred(QStringLiteral("Failed to enable Color Correction: %1.").arg(describe(error)));
			break;
		case ColorMode::grading:
			emit errorOccurred(QStringLiteral("Failed to enable Color Grading: %1.").arg(describe(error)));
			break;
		case ColorMode::filter:
			emit errorOccurred(QStringLiteral("Failed to enable the color filter: %1 (%2).")
				.arg(QDir::toNativeSeparators(colorFilterPath(_colorFilterIndex)), describe(error)));
			break;
		case ColorMode::none:
			break;
		}
	}
	emit colorModeChanged();
}

void SampleController::resetToDefaults(const QStringList& properties)
{
	const QMetaObject* meta = metaObject();
	for (const QString& name : properties) {
		const int index = meta->indexOfProperty(name.toLatin1().constData());
		if ((index < 0) || !_defaults.contains(name)) {
			qWarning() << "No default for" << name;
			continue;
		}
		meta->property(index).write(this, _defaults.value(name));
	}
}

void SampleController::start()
{
	_camera.start();
}

void SampleController::connectParams()
{
	if (!_filter.isValid()) {
		return;
	}

	connect(this, &SampleController::beautificationLevelChanged, this, [this] {
		_filter.pipeline().setBeautificationLevel(float(_beautificationLevel));
	});
	connect(this, &SampleController::smartZoomLevelChanged, this, [this] {
		_filter.pipeline().setSmartZoomLevel(float(_smartZoomLevel));
	});
	connect(this, &SampleController::lowLightPowerChanged, this, [this] {
		_filter.pipeline().setLowLightAdjustmentPower(float(_lowLightPower));
	});
	connect(this, &SampleController::sharpeningPowerChanged, this, [this] {
		_filter.pipeline().setSharpeningPower(float(_sharpeningPower));
	});

	// The three color modes share one power in the SDK: each slider drives it
	// only while its own mode is the one on.
	auto connectPower = [this](void (SampleController::*signal)(), ColorMode mode) {
		connect(this, signal, this, [this, mode] {
			if (mode == _colorMode) {
				_filter.pipeline().setColorCorrectionPower(colorModePower(mode));
			}
		});
	};
	connectPower(&SampleController::colorCorrectionPowerChanged, ColorMode::correction);
	connectPower(&SampleController::colorGradingPowerChanged, ColorMode::grading);
	connectPower(&SampleController::colorFilterPowerChanged, ColorMode::filter);
}

tsvb::PipelineError SampleController::configure(
		const std::function<void(tsvb::IPipelineConfiguration&)>& change)
{
	tsvb::IPipeline& pipeline = _filter.pipeline();
	SDKUniquePtr<tsvb::IPipelineConfiguration> config(pipeline.copyConfiguration());
	change(*config);
	return pipeline.setConfiguration(config.get());
}

void SampleController::setChecked(
		double& state,
		double value,
		const std::function<tsvb::PipelineError(tsvb::IPipeline&, float)>& set,
		const std::function<float(tsvb::IPipeline&)>& get,
		void (SampleController::*notify)())
{
	if (_filter.isValid()) {
		tsvb::IPipeline& pipeline = _filter.pipeline();
		if (succeeded(set(pipeline, float(value)))) {
			state = get(pipeline);
		}
	}
	emit (this->*notify)();
}

void SampleController::setColorCorrectionParam(
		tsvb::ColorCorrectionParam param,
		double& state,
		double value,
		void (SampleController::*notify)())
{
	setChecked(
		state, value,
		[param](tsvb::IPipeline& pipeline, float v) { return pipeline.setColorCorrectionParam(param, v); },
		[param](tsvb::IPipeline& pipeline) { return pipeline.getColorCorrectionParam(param, nullptr); },
		notify
	);
}

void SampleController::setPortraitLightingParam(
		tsvb::PortraitLightingParam param,
		double& state,
		double value,
		void (SampleController::*notify)())
{
	setChecked(
		state, value,
		[param](tsvb::IPipeline& pipeline, float v) { return pipeline.setPortraitLightingParam(param, v); },
		[param](tsvb::IPipeline& pipeline) { return pipeline.getPortraitLightingParam(param, nullptr); },
		notify
	);
}

void SampleController::toggle(
		bool& state,
		bool enabled,
		const std::function<tsvb::PipelineError(tsvb::IPipeline&)>& enable,
		const std::function<void(tsvb::IPipeline&)>& disable,
		void (SampleController::*notify)(),
		const QString& name)
{
	if (!_filter.isValid() || (enabled == state)) {
		emit (this->*notify)();
		return;
	}

	if (!enabled) {
		disable(_filter.pipeline());
		state = false;
	}
	else {
		const tsvb::PipelineError error = enable(_filter.pipeline());
		if (succeeded(error)) {
			state = true;
		}
		else if (tsvb::PipelineErrorCode::notSupportedByBackend == error) {
			emit errorOccurred(QStringLiteral("%1 is not available on the %2 backend.")
				.arg(name, _gpuBackend ? QStringLiteral("GPU") : QStringLiteral("CPU")));
		}
		else {
			emit errorOccurred(QStringLiteral("Failed to enable %1: %2.").arg(name, describe(error)));
		}
	}
	emit (this->*notify)();
}

bool SampleController::loadBackground(const QString& filePath)
{
	SDKUniquePtr<tsvb::IFrame> background(_filter.frameFactory().loadImage(toSDKPath(filePath).constData()));
	if (nullptr == background) {
		return false;
	}

	if (nullptr != _replacementController) {
		_replacementController->setBackgroundImage(background.get());
	}
	_background = std::move(background);
	return true;
}

tsvb::PipelineError SampleController::applyColorMode(ColorMode mode)
{
	if (!_filter.isValid()) {
		return tsvb::PipelineErrorCode::engineInitializationError;
	}

	SDKUniquePtr<tsvb::IFrame> reference;
	QByteArray lutFilePath;
	if (ColorMode::grading == mode) {
		reference.reset(_filter.frameFactory().loadImage(toSDKPath(_colorGradingReference.toLocalFile()).constData()));
		if (nullptr == reference) {
			return tsvb::PipelineErrorCode::invalidArgument;
		}
	}
	else if (ColorMode::filter == mode) {
		lutFilePath = toSDKPath(colorFilterPath(_colorFilterIndex));
	}

	tsvb::IPipeline& pipeline = _filter.pipeline();
	tsvb::PipelineError error = tsvb::PipelineErrorCode::ok;
	switch (mode) {
	case ColorMode::none:
		// Any of the three turns the whole stage off.
		pipeline.disableColorCorrection();
		break;
	case ColorMode::correction:
		error = pipeline.enableColorCorrection();
		break;
	case ColorMode::grading:
		error = pipeline.enableColorCorrectionWithReference(reference.get());
		break;
	case ColorMode::filter:
		error = pipeline.enableColorCorrectionWithLutFile(lutFilePath.constData());
		break;
	}
	if (!succeeded(error)) {
		return error;
	}

	_colorMode = mode;
	if (ColorMode::none != mode) {
		pipeline.setColorCorrectionPower(colorModePower(mode));
	}
	return tsvb::PipelineErrorCode::ok;
}

float SampleController::colorModePower(ColorMode mode) const
{
	switch (mode) {
	case ColorMode::correction:
		return float(_colorCorrectionPower);
	case ColorMode::grading:
		return float(_colorGradingPower);
	case ColorMode::filter:
		return float(_colorFilterPower);
	case ColorMode::none:
		break;
	}

	return 1.0f;
}

QString SampleController::colorFilterPath(int index) const
{
	const QString fileName = colorFilterName(index) + QStringLiteral(".cube");

	const QString nextToApp = appDirFile(QStringLiteral("color_luts/") + fileName);
	if (QFileInfo::exists(nextToApp)) {
		return nextToApp;
	}
	return QDir::current().absoluteFilePath(QStringLiteral("color_luts/") + fileName);
}

#ifndef SAMPLE_QML_SAMPLE_CONTROLLER_H
#define SAMPLE_QML_SAMPLE_CONTROLLER_H

#include "camera_controller.h"
#include "video_filter.h"

#include <QObject>
#include <QStringList>
#include <QUrl>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <functional>

class SampleController : public QObject
{
	Q_OBJECT
	QML_ELEMENT
	QML_UNCREATABLE("Created by the application and handed to the window.")

public:
	enum class ColorMode
	{
		none,
		correction,
		grading,
		filter
	};
	Q_ENUM(ColorMode)

private:
	Q_PROPERTY(CameraController* camera READ camera CONSTANT)

	Q_PROPERTY(bool sdkAvailable READ sdkAvailable CONSTANT)
	Q_PROPERTY(bool gpuBackend READ gpuBackend WRITE setGpuBackend NOTIFY gpuBackendChanged)
	// SegmentationPreset order: quality, balanced.
	Q_PROPERTY(int preset READ preset WRITE setPreset NOTIFY presetChanged)
	Q_PROPERTY(bool appleNeuralEngineEnabled READ appleNeuralEngineEnabled WRITE setAppleNeuralEngineEnabled NOTIFY appleNeuralEngineEnabledChanged)

	Q_PROPERTY(bool blurEnabled READ blurEnabled WRITE setBlurEnabled NOTIFY blurEnabledChanged)
	Q_PROPERTY(bool replaceEnabled READ replaceEnabled WRITE setReplaceEnabled NOTIFY replaceEnabledChanged)
	Q_PROPERTY(QUrl backgroundFile READ backgroundFile WRITE setBackgroundFile NOTIFY backgroundFileChanged)
	Q_PROPERTY(bool noiseSuppressionEnabled READ noiseSuppressionEnabled WRITE setNoiseSuppressionEnabled NOTIFY noiseSuppressionEnabledChanged)

	Q_PROPERTY(bool beautificationEnabled READ beautificationEnabled WRITE setBeautificationEnabled NOTIFY beautificationEnabledChanged)
	Q_PROPERTY(double beautificationLevel MEMBER _beautificationLevel NOTIFY beautificationLevelChanged)

	Q_PROPERTY(bool smartZoomEnabled READ smartZoomEnabled WRITE setSmartZoomEnabled NOTIFY smartZoomEnabledChanged)
	Q_PROPERTY(double smartZoomLevel MEMBER _smartZoomLevel NOTIFY smartZoomLevelChanged)

	Q_PROPERTY(bool portraitLightingEnabled READ portraitLightingEnabled WRITE setPortraitLightingEnabled NOTIFY portraitLightingEnabledChanged)
	Q_PROPERTY(double portraitLightStrength READ portraitLightStrength WRITE setPortraitLightStrength NOTIFY portraitLightStrengthChanged)
	Q_PROPERTY(double portraitBackgroundDarkening READ portraitBackgroundDarkening WRITE setPortraitBackgroundDarkening NOTIFY portraitBackgroundDarkeningChanged)

	Q_PROPERTY(bool lowLightEnabled READ lowLightEnabled WRITE setLowLightEnabled NOTIFY lowLightEnabledChanged)
	Q_PROPERTY(double lowLightPower MEMBER _lowLightPower NOTIFY lowLightPowerChanged)

	Q_PROPERTY(bool sharpeningEnabled READ sharpeningEnabled WRITE setSharpeningEnabled NOTIFY sharpeningEnabledChanged)
	Q_PROPERTY(double sharpeningPower MEMBER _sharpeningPower NOTIFY sharpeningPowerChanged)

	Q_PROPERTY(bool colorCorrectionEnabled READ colorCorrectionEnabled WRITE setColorCorrectionEnabled NOTIFY colorModeChanged STORED false)
	Q_PROPERTY(bool colorCorrectionAutoMode READ colorCorrectionAutoMode WRITE setColorCorrectionAutoMode NOTIFY colorCorrectionAutoModeChanged)
	Q_PROPERTY(double colorCorrectionPower MEMBER _colorCorrectionPower NOTIFY colorCorrectionPowerChanged)
	Q_PROPERTY(double colorCorrectionWhiteBalance READ colorCorrectionWhiteBalance WRITE setColorCorrectionWhiteBalance NOTIFY colorCorrectionWhiteBalanceChanged)
	Q_PROPERTY(double colorCorrectionExposure READ colorCorrectionExposure WRITE setColorCorrectionExposure NOTIFY colorCorrectionExposureChanged)
	Q_PROPERTY(double colorCorrectionTemperature READ colorCorrectionTemperature WRITE setColorCorrectionTemperature NOTIFY colorCorrectionTemperatureChanged)
	Q_PROPERTY(double colorCorrectionTint READ colorCorrectionTint WRITE setColorCorrectionTint NOTIFY colorCorrectionTintChanged)
	Q_PROPERTY(double colorCorrectionVibrance READ colorCorrectionVibrance WRITE setColorCorrectionVibrance NOTIFY colorCorrectionVibranceChanged)
	Q_PROPERTY(double colorCorrectionContrast READ colorCorrectionContrast WRITE setColorCorrectionContrast NOTIFY colorCorrectionContrastChanged)
	Q_PROPERTY(double colorCorrectionUpdatePeriodMs READ colorCorrectionUpdatePeriodMs WRITE setColorCorrectionUpdatePeriodMs NOTIFY colorCorrectionUpdatePeriodMsChanged)

	Q_PROPERTY(bool colorGradingEnabled READ colorGradingEnabled WRITE setColorGradingEnabled NOTIFY colorModeChanged STORED false)
	Q_PROPERTY(QUrl colorGradingReference READ colorGradingReference WRITE setColorGradingReference NOTIFY colorGradingReferenceChanged)
	Q_PROPERTY(double colorGradingPower MEMBER _colorGradingPower NOTIFY colorGradingPowerChanged)

	Q_PROPERTY(bool colorFilterEnabled READ colorFilterEnabled WRITE setColorFilterEnabled NOTIFY colorModeChanged STORED false)
	Q_PROPERTY(QStringList colorFilters READ colorFilters CONSTANT)
	Q_PROPERTY(int colorFilterIndex READ colorFilterIndex WRITE setColorFilterIndex NOTIFY colorFilterIndexChanged)
	Q_PROPERTY(double colorFilterPower MEMBER _colorFilterPower NOTIFY colorFilterPowerChanged)

	Q_PROPERTY(ColorMode colorMode READ colorMode WRITE setColorMode NOTIFY colorModeChanged)

public:
	explicit SampleController(QObject* parent = nullptr);
	~SampleController() override;

	CameraController* camera();

	bool sdkAvailable() const;

	bool gpuBackend() const;
	void setGpuBackend(bool gpu);

	int preset() const;
	void setPreset(int preset);

	bool appleNeuralEngineEnabled() const;
	void setAppleNeuralEngineEnabled(bool enabled);

	bool blurEnabled() const;
	void setBlurEnabled(bool enabled);

	bool replaceEnabled() const;
	void setReplaceEnabled(bool enabled);
	QUrl backgroundFile() const;
	void setBackgroundFile(const QUrl& file);

	bool noiseSuppressionEnabled() const;
	void setNoiseSuppressionEnabled(bool enabled);

	bool beautificationEnabled() const;
	void setBeautificationEnabled(bool enabled);

	bool smartZoomEnabled() const;
	void setSmartZoomEnabled(bool enabled);

	bool portraitLightingEnabled() const;
	void setPortraitLightingEnabled(bool enabled);
	double portraitLightStrength() const;
	void setPortraitLightStrength(double strength);
	double portraitBackgroundDarkening() const;
	void setPortraitBackgroundDarkening(double darkening);

	bool lowLightEnabled() const;
	void setLowLightEnabled(bool enabled);

	bool sharpeningEnabled() const;
	void setSharpeningEnabled(bool enabled);

	bool colorCorrectionEnabled() const;
	void setColorCorrectionEnabled(bool enabled);
	bool colorCorrectionAutoMode() const;
	void setColorCorrectionAutoMode(bool autoMode);
	double colorCorrectionWhiteBalance() const;
	void setColorCorrectionWhiteBalance(double value);
	double colorCorrectionExposure() const;
	void setColorCorrectionExposure(double value);
	double colorCorrectionTemperature() const;
	void setColorCorrectionTemperature(double value);
	double colorCorrectionTint() const;
	void setColorCorrectionTint(double value);
	double colorCorrectionVibrance() const;
	void setColorCorrectionVibrance(double value);
	double colorCorrectionContrast() const;
	void setColorCorrectionContrast(double value);
	double colorCorrectionUpdatePeriodMs() const;
	void setColorCorrectionUpdatePeriodMs(double value);

	bool colorGradingEnabled() const;
	void setColorGradingEnabled(bool enabled);
	QUrl colorGradingReference() const;
	void setColorGradingReference(const QUrl& file);

	bool colorFilterEnabled() const;
	void setColorFilterEnabled(bool enabled);
	QStringList colorFilters() const;
	int colorFilterIndex() const;
	void setColorFilterIndex(int index);

	ColorMode colorMode() const;
	void setColorMode(ColorMode mode);

	Q_INVOKABLE void resetToDefaults(const QStringList& properties);

	void start();

signals:
	void gpuBackendChanged();
	void presetChanged();
	void appleNeuralEngineEnabledChanged();

	void blurEnabledChanged();
	void replaceEnabledChanged();
	void backgroundFileChanged();
	void noiseSuppressionEnabledChanged();

	void beautificationEnabledChanged();
	void beautificationLevelChanged();

	void smartZoomEnabledChanged();
	void smartZoomLevelChanged();

	void portraitLightingEnabledChanged();
	void portraitLightStrengthChanged();
	void portraitBackgroundDarkeningChanged();

	void lowLightEnabledChanged();
	void lowLightPowerChanged();

	void sharpeningEnabledChanged();
	void sharpeningPowerChanged();

	void colorModeChanged();
	void colorCorrectionAutoModeChanged();
	void colorCorrectionPowerChanged();
	void colorCorrectionWhiteBalanceChanged();
	void colorCorrectionExposureChanged();
	void colorCorrectionTemperatureChanged();
	void colorCorrectionTintChanged();
	void colorCorrectionVibranceChanged();
	void colorCorrectionContrastChanged();
	void colorCorrectionUpdatePeriodMsChanged();

	void colorGradingReferenceChanged();
	void colorGradingPowerChanged();

	void colorFilterIndexChanged();
	void colorFilterPowerChanged();

	void errorOccurred(const QString& message);

private:
	void connectParams();

	tsvb::PipelineError configure(const std::function<void(tsvb::IPipelineConfiguration&)>& change);

	void setChecked(
		double& state,
		double value,
		const std::function<tsvb::PipelineError(tsvb::IPipeline&, float)>& set,
		const std::function<float(tsvb::IPipeline&)>& get,
		void (SampleController::*notify)()
	);
	void setColorCorrectionParam(
		tsvb::ColorCorrectionParam param,
		double& state,
		double value,
		void (SampleController::*notify)()
	);
	void setPortraitLightingParam(
		tsvb::PortraitLightingParam param,
		double& state,
		double value,
		void (SampleController::*notify)()
	);

	void toggle(
		bool& state,
		bool enabled,
		const std::function<tsvb::PipelineError(tsvb::IPipeline&)>& enable,
		const std::function<void(tsvb::IPipeline&)>& disable,
		void (SampleController::*notify)(),
		const QString& name
	);

	bool loadBackground(const QString& filePath);
	tsvb::PipelineError applyColorMode(ColorMode mode);
	float colorModePower(ColorMode mode) const;
	QString colorFilterPath(int index) const;

private:
	VideoFilter _filter;

	bool _gpuBackend = true;
	int _preset = tsvb::segmentationPresetBalanced;
	bool _appleNeuralEngineEnabled = false;

	bool _blurEnabled = false;
	float _blurPower = 0.5f;
	bool _replaceEnabled = false;
	QUrl _backgroundFile;
	// After the filter, so that both go before the pipeline does.
	SDKUniquePtr<tsvb::IReplacementController> _replacementController;
	SDKUniquePtr<tsvb::IFrame> _background;
	bool _noiseSuppressionEnabled = false;

	bool _beautificationEnabled = false;
	double _beautificationLevel = 0.5;

	bool _smartZoomEnabled = false;
	double _smartZoomLevel = 0.8;

	bool _portraitLightingEnabled = false;
	double _portraitLightStrength = 0.6;
	double _portraitBackgroundDarkening = 0.4;

	bool _lowLightEnabled = false;
	double _lowLightPower = 0.8;

	bool _sharpeningEnabled = false;
	double _sharpeningPower = 0.3;

	ColorMode _colorMode = ColorMode::none;

	bool _colorCorrectionAutoMode = true;
	double _colorCorrectionPower = 1;
	double _colorCorrectionWhiteBalance = 0;
	double _colorCorrectionExposure = 0;
	double _colorCorrectionTemperature = 0;
	double _colorCorrectionTint = 0;
	double _colorCorrectionVibrance = 0;
	double _colorCorrectionContrast = 0;
	double _colorCorrectionUpdatePeriodMs = 200;

	QUrl _colorGradingReference;
	double _colorGradingPower = 0.8;

	int _colorFilterIndex = 0;
	double _colorFilterPower = 0.8;

	QVariantMap _defaults;

	// Last: its capture thread, which runs the pipeline, is joined before any SDK
	// object above goes.
	CameraController _camera;
};

#endif

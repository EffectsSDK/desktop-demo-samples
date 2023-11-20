#ifndef SAMPLE_H
#define SAMPLE_H

#include "consts.h"
#include "frame_view.h"
#include "metrics_view.h"

#include <QtWidgets>

#include <memory>

class Pipeline;
class SampleUI;

class Sample : public QWidget
{
	Q_OBJECT

public:
	Sample();
	~Sample() override;

public slots:
	void onCameraPicked(const QString& cameraName);
	void setCameraScale(const QSize& scale);
	void toggleBlurEnabled();
	void toggleDenoiseEnabled();
	void toggleDenoiseWithFaceClicked();
	void toggleReplaceEnabled();
	void toggleBeautificateEnabled();
	void toggleCorrectColorsEnabled();
	void toggleSmartZoomEnabled();
	void toggleColorGradingEnabled();
	void toggleColorFilterEnabled();
	void onColorFilterPicked(const QString& fileName);
	void openBackground();
	void openColorGradingReference();
	void onBeautificationLevelSliderMoved();
	void onDenoiseLevelSliderMoved();
	void onZoomLevelSliderMoved();
	void processFrame(const QImage& frame);
	void switchToCPU();
	void switchToGPU();
	void onPresetPicked(Preset preset);
	void updateMetrics();
	void onColorIntensitySliderMoved();
	void onColorLUTFileNotFoundError(const QString& fileName);

private:
	void updateUIState();
	bool restoreUIAndPipelineFromSettings();
	QStringList colorLutSearchPaths() const;
	QStringList enumerateColorLutFiles() const;
	QString findColorLutFilePath(const QString& fileName) const;
	QString selectedColorLutFilePath() const;
	void checkCPUPipelineAvailable();
	void checkGPUOnlyFeaturesEnabled();
	bool setCameraByName(const QString& name);

	QString stringFromNumber(double number);

	bool enableColorFilter(const QString& lutFilePath);

private:
	SampleUI* const m_ui;
	std::shared_ptr<Pipeline> m_pipeline;
	std::unique_ptr<QSettings> m_settings;

	QTimer m_updateMetricsTimer;

	QString m_colorGradingRefPath;
};

#endif

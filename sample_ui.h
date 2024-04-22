#ifndef SAMPLE_UI_H
#define SAMPLE_UI_H

#include "consts.h"

#include "frame_view.h"
#include "metrics_view.h"

class Sample;

class SampleUI : public QWidget
{
	Q_OBJECT
public:
	explicit SampleUI(Sample* sample);
	~SampleUI() override;

	void setCurrentPreset(Preset preset);
	void setColorFilters(const QStringList& fileNames);
	QString currentColorFilter() const;
	void setCurrentColorFilter(const QString& fileName);

public:
	MetricsView* metricsView = nullptr;
	FrameView* frameView = nullptr;

	QComboBox* cameraScaleComoBox = nullptr;
	QComboBox* cameraComoBox = nullptr;

	QGroupBox* virtualBackgroundBox = nullptr;
	QCheckBox* blurCheckBox = nullptr;
	QCheckBox* denoiseCheckBox = nullptr;
	QCheckBox* denoiseWithFaceCheckBox = nullptr;
	QCheckBox* replaceCheckBox = nullptr;
	QCheckBox* beautificateCheckBox = nullptr;
	QCheckBox* smartZoomCheckBox = nullptr;
	QCheckBox* lowLightAdjustmentCheckbox = nullptr;
	QCheckBox* sharpeningCheckbox = nullptr;
	QPushButton* openBackgroundButton = nullptr;
	QPushButton* openReferencedButton = nullptr;
	QLabel* beautificationLevelLabel = nullptr;
	QSlider* beautificationLevelSlider = nullptr;
	QLabel* denoisePowerLabel = nullptr;
	QSlider* denoisePowerSlider = nullptr;
	QLabel* zoomLevelLabel = nullptr;
	QSlider* zoomLevelSlider = nullptr;
	QLabel* lowLightAdjustmentPowerLabel = nullptr;
	QSlider* lowLightAdjustmentPowerSlider = nullptr;
	QLabel* sharpeningPowerLabel = nullptr;
	QSlider* sharpeningPowerSlider = nullptr;

	QGroupBox* colorBox = nullptr;
	QCheckBox* correctColorsCheckbox = nullptr;
	QCheckBox* colorGradingCheckbox = nullptr;
	QSlider* colorIntensitySlider = nullptr;
	QCheckBox* colorFilterCheckbox = nullptr;
	QComboBox* colorFilterComoBox = nullptr;

	QComboBox* presetBox = nullptr;

	QGroupBox* backendBox = nullptr;
	QRadioButton* gpuRadioButton = nullptr;
	QRadioButton* cpuRadioButton = nullptr;

	QPushButton* about = nullptr;

private:
	void createUI();

private slots:
	void openAboutDialog();

private:
	Sample* const m_sample;

	QSize m_currentVideoSurfaceSize;
};

#endif

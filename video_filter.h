#ifndef BG_REPLACER_H
#define BG_REPLACER_H

#include "consts.h"

#include <QImage>

#include <memory>

class VideoFilter {
public:
	VideoFilter();
	~VideoFilter();

	bool isValid() const;

	QImage replaceBG(const QImage& img);

	bool setBackend(Backend backend);
	Backend backend() const;

	bool setPreset(Preset preset);
	Preset preset() const;

	bool enableBlur();
	void disableBlur();
	bool isBlurEnabled() const;

	bool enableDenoise();
	void disableDenoise();
	bool isDenoiseEnabled() const;
	float denoisePower() const;
	void setDenoisePower(float power);
	bool isDenoiseWithFace() const;
	void setDenoiseWithFace(bool withFace);

	bool enableReplacement();
	void disableReplacement();
	bool isReplaceEnabled() const;
	void setBackground(const QString& filePath);

	bool enableBeautification();
	void disableBeautification();
	bool isBeautificationEnabled() const;
	void setBeautificationLevel(float level);
	float beautificationLevel() const;
	
	bool enableColorCorrection();
	void disableColorCorrection();
	bool isColorCorrectionEnabled() const;
	void setColorCorrectionPower(float power);

	bool enableSmartZoom();
	void disableSmartZoom();
	bool isSmartZoomEnabled() const;
	void setSmartZoomLevel(float level);
	float smartZoomLevel() const;

	bool enableColorGrading(const QString& refImage);
	void disableColorGrading();
	bool isColorGradingEnabled() const;
	void setColorGradingPower(float power);

	bool enableColorFilter(const QString& filePath);
	void disableColorFilter();
	bool isColorFilterEnabled() const;
	void setColorFilterPower(float power);

private:
	class Impl;
	std::unique_ptr<Impl> _impl;
};

#endif // BG_REPLACER_H

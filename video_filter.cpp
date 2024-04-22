#include "video_filter.h"

#include "vb_sdk/sdk_factory.h"

#include "sdk_library_handler.h"

#include <QtGui/QtGui>

#include <mutex>

namespace {

class Releaser
{
public:
	void operator()(tsvb::IRelease* object)
	{
		object->release();
	}
};

}

static std::map<Preset, tsvb::SegmentationPreset> makePresetMap()
{
	return {
		{ Preset::quality, tsvb::segmentationPresetQuality },
		{ Preset::balanced, tsvb::segmentationPresetBalanced },
		{ Preset::speed, tsvb::segmentationPresetSpeed },
		{ Preset::lightning, tsvb::segmentationPresetLightning }
	};
}

class VideoFilter::Impl
{
	mutable std::mutex _mutex;
	
	BGLibraryHandler _handler;
	std::unique_ptr<tsvb::IFrameFactory, Releaser> _frameFactory;
	std::unique_ptr<tsvb::IFrame, Releaser> _background;
	std::unique_ptr<tsvb::IReplacementController, Releaser> _replacementController;
	std::unique_ptr<tsvb::IPipeline, Releaser> _pipeline;

	bool _beautificationEnabled = false;
	bool _colorCorrectionEnabled = false;
	bool _smartZoomEnabled = false;
	bool _colorGradingEnabled = false;
	bool _colorFiltersEnabled = false;
	bool _lowLightEnabled = false;
	bool _sharpeningEnabled = false;

public:
	Impl()
	{ }

	bool initialize()
	{
		if (!_handler.isValid()) {
			return false;
		}

		std::unique_ptr<tsvb::ISDKFactory, Releaser> factory;
		factory.reset(_handler.createSDKFactory());
		if (nullptr == factory) {
			return false;
		}
		_frameFactory.reset(factory->createFrameFactory());
		if (nullptr == _frameFactory) {
			return false;
		}
		_pipeline.reset(factory->createPipeline());
		if (nullptr == _pipeline) {
			return false;
		}

		return true;
	}

	bool setBackend(Backend backend)
	{
		std::lock_guard<std::mutex> lockGuard(_mutex);
		std::unique_ptr<tsvb::IPipelineConfiguration, Releaser> config;

		config.reset(_pipeline->copyConfiguration());
		config->setBackend((Backend::gpu == backend) ? 
			tsvb::Backend::GPU : tsvb::Backend::CPU
		);
		
		auto error = _pipeline->setConfiguration(config.get());
		return tsvb::PipelineErrorCode::ok == error;
	}

	Backend backend() const
	{
		std::lock_guard<std::mutex> lockGuard(_mutex);
		std::unique_ptr<tsvb::IPipelineConfiguration, Releaser> config;
		config.reset(_pipeline->copyConfiguration());

		return (tsvb::Backend::GPU == config->getBackend()) ? Backend::gpu : Backend::cpu;
	}

	bool setPreset(Preset preset) 
	{
		tsvb::SegmentationPreset sdkPreset = makePresetMap()[preset];

		std::lock_guard<std::mutex> lockGuard(_mutex);
		std::unique_ptr<tsvb::IPipelineConfiguration, Releaser> config;
		config.reset(_pipeline->copyConfiguration());
		config->setSegmentationPreset(sdkPreset);

		auto error = _pipeline->setConfiguration(config.get());
		return tsvb::PipelineErrorCode::ok == error;
	}

	Preset preset() const
	{
		std::lock_guard<std::mutex> lockGuard(_mutex);
		std::unique_ptr<tsvb::IPipelineConfiguration, Releaser> config;
		config.reset(_pipeline->copyConfiguration());

		for (const auto& presetPair : makePresetMap()) {
			if (presetPair.second == config->getSegmentationPreset()) {
				return presetPair.first;
			}
		}
		return Preset::quality;
	}
	
	QImage replaceBG(const QImage& img)
	{
		if (QImage::Format_ARGB32 != img.format()) {
			return QImage();
		}

		std::unique_ptr<tsvb::IFrame, Releaser> input;
		input.reset(
			_frameFactory->createBGRA(
				const_cast<uchar*>(img.constBits()),
				img.bytesPerLine(),
				img.width(),
				img.height(),
				false
			)
		);

		std::unique_ptr<tsvb::IFrame, Releaser> output;
		int error = 0;
		{
			std::lock_guard<std::mutex> lockGuard(_mutex);
			output.reset(_pipeline->process(input.get(), &error));
		}

		if (nullptr == output) {
			return QImage();
		}

		std::unique_ptr<tsvb::ILockedFrameData, Releaser> lockedData(
			output->lock(tsvb::FrameLock::read)
		);
		if (nullptr != lockedData) {
			return QImage(
				reinterpret_cast<const uchar*>(lockedData->dataPointer(0)),
				output->width(),
				output->height(),
				lockedData->bytesPerLine(0),
				QImage::Format_ARGB32
			).copy();
		}

		return QImage();
	}

	bool enableBlur()
	{
		std::lock_guard<std::mutex> lockGuard(_mutex);
		tsvb::PipelineError error = _pipeline->enableBlurBackground(0.5);
		return (tsvb::PipelineErrorCode::ok == error);
	}

	void disableBlur()
	{
		std::lock_guard<std::mutex> lockGuard(_mutex);
		_pipeline->disableBackgroundBlur();
	}

	bool isBlurEnabled() const
	{
		std::lock_guard<std::mutex> lockGuard(_mutex);
		return _pipeline->getBlurBackgroundState(nullptr);
	}

	bool enableDenoise()
	{
		std::lock_guard<std::mutex> lockGuard(_mutex);
		tsvb::PipelineError error = _pipeline->enableDenoiseBackground();
		return (tsvb::PipelineErrorCode::ok == error);
	}

	void disableDenoise()
	{
		std::lock_guard<std::mutex> lockGuard(_mutex);
		_pipeline->disableBackgroundDenoise();
	}

	bool isDenoiseEnabled() const
	{
		std::lock_guard<std::mutex> lockGuard(_mutex);
		return _pipeline->getDenoiseBackgroundState();
	}

	float denoisePower() const
	{
		return _pipeline->getDenoisePower();
	}

	void setDenoisePower(float power)
	{
		std::lock_guard<std::mutex> lockGuard(_mutex);
		_pipeline->setDenoisePower(power);
	}

	bool isDenoiseWithFace() const
	{
		return _pipeline->getDenoiseWithFace();
	}

	void setDenoiseWithFace(bool withFace)
	{
		_pipeline->setDenoiseWithFace(withFace);
	}

	bool enableReplacement()
	{
		std::lock_guard<std::mutex> lockGuard(_mutex);
		if (nullptr != _replacementController) {
			return true;
		}

		tsvb::IReplacementController* controller = nullptr;
		tsvb::PipelineError error = _pipeline->enableReplaceBackground(&controller);
		if (tsvb::PipelineErrorCode::ok != error) {
			return false;
		}

		_replacementController.reset(controller);
		if (nullptr != _background) {
			_replacementController->setBackgroundImage(_background.get());
		}
		return true;
	}

	void disableReplacement()
	{
		std::lock_guard<std::mutex> lockGuard(_mutex);
		_pipeline->disableReplaceBackground();
		_replacementController = nullptr;
	}

	bool isReplaceEnabled() const
	{
		std::lock_guard<std::mutex> lockGuard(_mutex);
		return _pipeline->getReplaceBackgroundState();
	}

	bool setBackground(const QString& filePath)
	{
		std::unique_ptr<tsvb::IFrame, Releaser> bgFrame;
		bgFrame.reset(
			_frameFactory->loadImage(filePath.toUtf8().constData())
		);
		if (nullptr == bgFrame) { 
			return false;
		}
		std::lock_guard<std::mutex> lockGuard(_mutex);
		if (nullptr != _replacementController) {
			_replacementController->setBackgroundImage(bgFrame.get());
		}
		std::swap(_background, bgFrame);
		return true;
	}

	bool enableBeautification()
	{
		std::lock_guard<std::mutex> lockGuard(_mutex);
		tsvb::PipelineError error = _pipeline->enableBeautification();
		if (tsvb::PipelineErrorCode::ok != error) {
			return false;
		}

		_beautificationEnabled = true;
		return true;
	}

	void disableBeautification()
	{
		std::lock_guard<std::mutex> lockGuard(_mutex);
		_pipeline->disableBeautification();
		_beautificationEnabled = false;
	}

	bool isBeautificationEnabled() const
	{
		std::lock_guard<std::mutex> lockGuard(_mutex);
		return _beautificationEnabled;
	}

	void setBeautificationLevel(float level)
	{
		std::lock_guard<std::mutex> lockGuard(_mutex);
		_pipeline->setBeautificationLevel(level);
	}

	float beautificationLevel() const
	{
		std::lock_guard<std::mutex> lockGuard(_mutex);
		return _pipeline->getBeautificationLevel();
	}
	
	bool enableColorCorrection()
	{
		std::lock_guard<std::mutex> lockGuard(_mutex);
		tsvb::PipelineError error = _pipeline->enableColorCorrection();
		if (tsvb::PipelineErrorCode::ok != error) {
			return false;
		}

		_colorCorrectionEnabled = true;
		return true;
	}

	void disableColorCorrection()
	{
		std::lock_guard<std::mutex> lockGuard(_mutex);
		_pipeline->disableColorCorrection();
		_colorCorrectionEnabled = false;
	}

	bool isColorCorrectionEnabled() const
	{
		std::lock_guard<std::mutex> lockGuard(_mutex);
		return _colorCorrectionEnabled;
	}

	void setColorCorrectionPower(float power)
	{
		std::lock_guard<std::mutex> lockGuard(_mutex);
		_pipeline->setColorCorrectionPower(power);
	}

	bool enableSmartZoom()
	{
		std::lock_guard<std::mutex> lockGuard(_mutex);
		tsvb::PipelineError error = _pipeline->enableSmartZoom();
		if (tsvb::PipelineErrorCode::ok != error) {
			return false;
		}

		_smartZoomEnabled = true;
		return true;
	}

	void disableSmartZoom()
	{
		std::lock_guard<std::mutex> lockGuard(_mutex);
		_pipeline->disableSmartZoom();
		_smartZoomEnabled = false;
	}

	bool isSmartZoomEnabled() const
	{
		std::lock_guard<std::mutex> lockGuard(_mutex);
		return _smartZoomEnabled;
	}

	void setSmartZoomLevel(float level)
	{
		std::lock_guard<std::mutex> lockGuard(_mutex);
		_pipeline->setSmartZoomLevel(level);
	}

	float smartZoomLevel() const
	{
		std::lock_guard<std::mutex> lockGuard(_mutex);
		return _pipeline->getSmartZoomLevel();
	}

	bool enableColorGrading(const QString& refImage)
	{
		QByteArray utf8Path = QDir::toNativeSeparators(refImage).toUtf8();
		utf8Path.append('\0');
		std::unique_ptr<tsvb::IFrame, Releaser> referenceFrame;
		referenceFrame.reset(_frameFactory->loadImage(utf8Path));
		if (nullptr == referenceFrame) {
			return false;
		}

		std::lock_guard<std::mutex> lockGuard(_mutex);
		tsvb::PipelineError error = 
			_pipeline->enableColorCorrectionWithReference(referenceFrame.get());
		if (tsvb::PipelineErrorCode::ok != error) {
			return false;
		}
		_colorGradingEnabled = true;
		return true;
	}

	void disableColorGrading()
	{
		std::lock_guard<std::mutex> lockGuard(_mutex);
		_pipeline->disableColorCorrection();
		_colorGradingEnabled = false;
	}

	bool isColorGradingEnabled() const
	{
		std::lock_guard<std::mutex> lockGuard(_mutex);
		return _colorGradingEnabled;
	}

	void setColorGradingPower(float power)
	{
		std::lock_guard<std::mutex> lockGuard(_mutex);
		_pipeline->setColorCorrectionPower(power);
	}

	bool enableColorFilter(const QString& filePath)
	{
		auto utf8FilePath = QDir::toNativeSeparators(filePath).toUtf8();
		utf8FilePath.append('\0');
		std::lock_guard<std::mutex> lockGuard(_mutex);

		tsvb::PipelineError error = 
			_pipeline->enableColorCorrectionWithLutFile(utf8FilePath.data());
		if (tsvb::PipelineErrorCode::ok != error) {
			return false;
		}
		_colorFiltersEnabled = true;
		return true;
	}

	void disableColorFilter()
	{
		std::lock_guard<std::mutex> lockGuard(_mutex);
		_pipeline->disableColorCorrection();
		_colorFiltersEnabled = false;
	}

	bool isColorFilterEnabled() const
	{
		std::lock_guard<std::mutex> lockGuard(_mutex);
		return _colorFiltersEnabled;
	}

	void setColorFilterPower(float power)
	{
		std::lock_guard<std::mutex> lockGuard(_mutex);
		_pipeline->setColorCorrectionPower(power);
	}

	bool enableLowLightAdjustment()
	{
		std::lock_guard<std::mutex> lockGuard(_mutex);
		auto error = _pipeline->enableLowLightAdjustment();
		_lowLightEnabled = tsvb::PipelineErrorCode::ok == error;
		return _lowLightEnabled;
	}

	void disableLowLightAdjustment()
	{
		std::lock_guard<std::mutex> lockGuard(_mutex);
		_pipeline->disableLowLightAdjustment();
		_lowLightEnabled = false;
	}

	bool isLowLightAdjustmentEnabled() const
	{
		std::lock_guard<std::mutex> lockGuard(_mutex);
		return _lowLightEnabled;
	}


	void setLowLightAdjustmentPower(float power)
	{
		std::lock_guard<std::mutex> lockGuard(_mutex);
		_pipeline->setLowLightAdjustmentPower(power);
	}

	float getLowLightAdjustmentPower() const
	{
		std::lock_guard<std::mutex> lockGuard(_mutex);
		return _pipeline->getLowLightAdjustmentPower();
	}

	bool enableSharpening()
	{
		std::lock_guard<std::mutex> lockGuard(_mutex);
		tsvb::PipelineError error = _pipeline->enableSharpening();
		_sharpeningEnabled = (tsvb::PipelineErrorCode::ok == error);
		return _sharpeningEnabled;
	}

	void disableSharpening()
	{
		std::lock_guard<std::mutex> lockGuard(_mutex);
		_pipeline->disableSharpening();
		_sharpeningEnabled = false;
	}

	bool isSharpeningEnabled() const
	{
		std::lock_guard<std::mutex> lockGuard(_mutex);
		return _sharpeningEnabled;
	}

	float sharpeningPower() const
	{
		std::lock_guard<std::mutex> lockGuard(_mutex);
		return _pipeline->getSharpeningPower();
	}

	void setSharpeningPower(float power)
	{
		std::lock_guard<std::mutex> lockGuard(_mutex);
		_pipeline->setSharpeningPower(power);
	}
};

VideoFilter::VideoFilter()
{
	std::unique_ptr<Impl> impl(new Impl);
	if (impl->initialize()) {
		_impl = std::move(impl);
	}
}

VideoFilter::~VideoFilter() = default;

bool VideoFilter::isValid() const
{
	return (nullptr != _impl);
}

QImage VideoFilter::replaceBG(const QImage& img)
{
	return _impl->replaceBG(img);
}

bool VideoFilter::setBackend(Backend backend)
{
	return _impl->setBackend(backend);
}

Backend VideoFilter::backend() const
{
	return _impl->backend();
}

bool VideoFilter::setPreset(Preset preset)
{
	return _impl->setPreset(preset);
}

Preset VideoFilter::preset() const
{
	return _impl->preset();
}

bool VideoFilter::enableBlur()
{
	return _impl->enableBlur();
}

void VideoFilter::disableBlur()
{
	_impl->disableBlur();
}

bool VideoFilter::isBlurEnabled() const
{
	return _impl->isBlurEnabled();
}

bool VideoFilter::enableDenoise()
{
	return _impl->enableDenoise();
}

void VideoFilter::disableDenoise()
{
	_impl->disableDenoise();
}

bool VideoFilter::isDenoiseEnabled() const
{
	return _impl->isDenoiseEnabled();
}

float VideoFilter::denoisePower() const
{
	return _impl->denoisePower();
}

void VideoFilter::setDenoisePower(float power)
{
	return _impl->setDenoisePower(power);
}

bool VideoFilter::isDenoiseWithFace() const
{
	return _impl->isDenoiseWithFace();
}

void VideoFilter::setDenoiseWithFace(bool withFace)
{
	_impl->setDenoiseWithFace(withFace);
}

bool VideoFilter::enableReplacement()
{
	return _impl->enableReplacement();
}

void VideoFilter::disableReplacement()
{
	_impl->disableReplacement();
}

bool VideoFilter::isReplaceEnabled() const
{
	return _impl->isReplaceEnabled();
}

void VideoFilter::setBackground(const QString& filePath)
{
	_impl->setBackground(filePath);
}

bool VideoFilter::enableBeautification()
{
	return _impl->enableBeautification();
}

void VideoFilter::disableBeautification()
{
	_impl->disableBeautification();
}

bool VideoFilter::isBeautificationEnabled() const
{
	return _impl->isBeautificationEnabled();
}

void VideoFilter::setBeautificationLevel(float level)
{
	_impl->setBeautificationLevel(level);
}

float VideoFilter::beautificationLevel() const
{
	return _impl->beautificationLevel();
}

bool VideoFilter::enableColorCorrection()
{
	return _impl->enableColorCorrection();
}

void VideoFilter::disableColorCorrection()
{
	_impl->disableColorCorrection();
}

bool VideoFilter::isColorCorrectionEnabled() const
{
	return _impl->isColorCorrectionEnabled();
}

void VideoFilter::setColorCorrectionPower(float power)
{
	_impl->setColorCorrectionPower(power);
}

bool VideoFilter::enableSmartZoom()
{
	return _impl->enableSmartZoom();
}

void VideoFilter::disableSmartZoom()
{
	_impl->disableSmartZoom();
}

bool VideoFilter::isSmartZoomEnabled() const
{
	return _impl->isSmartZoomEnabled();
}

void VideoFilter::setSmartZoomLevel(float level)
{
	return _impl->setSmartZoomLevel(level);
}

float VideoFilter::smartZoomLevel() const
{
	return _impl->smartZoomLevel();
}

bool VideoFilter::enableColorGrading(const QString& refImage)
{
	return _impl->enableColorGrading(refImage);
}

void VideoFilter::disableColorGrading()
{
	_impl->disableColorGrading();
}

bool VideoFilter::isColorGradingEnabled() const
{
	return _impl->isColorGradingEnabled();
}

void VideoFilter::setColorGradingPower(float power)
{
	_impl->setColorGradingPower(power);
}

bool VideoFilter::enableColorFilter(const QString& filePath)
{
	return _impl->enableColorFilter(filePath);
}

void VideoFilter::disableColorFilter()
{
	_impl->disableColorFilter();
}

bool VideoFilter::isColorFilterEnabled() const
{
	return _impl->isColorFilterEnabled();
}

void VideoFilter::setColorFilterPower(float power)
{
	_impl->setColorFilterPower(power);
}

bool VideoFilter::enableLowLightAdjustment()
{
	return _impl->enableLowLightAdjustment();
}

void VideoFilter::disableLowLightAdjustment()
{
	_impl->disableLowLightAdjustment();
}

bool VideoFilter::isLowLightAdjustmentEnabled() const
{
	return _impl->isLowLightAdjustmentEnabled();
}

void VideoFilter::setLowLightAdjustmentPower(float power)
{
	_impl->setLowLightAdjustmentPower(power);
}

float VideoFilter::getLowLightAdjustmentPower() const
{
	return _impl->getLowLightAdjustmentPower();
}

bool VideoFilter::enableSharpening()
{
	return _impl->enableSharpening();
}

void VideoFilter::disableSharpening()
{
	_impl->disableSharpening();
}

bool VideoFilter::isSharpeningEnabled() const
{
	return _impl->isSharpeningEnabled();
}

float VideoFilter::sharpeningPower() const
{
	return _impl->sharpeningPower();
}

void VideoFilter::setSharpeningPower(float power)
{
	_impl->setSharpeningPower(power);
}


# Video Effects SDK - Windows/macOS trial SDK and Demo samples

Add real-time AI video enhancement that makes video meeting experience more effective and comfortable to your application in a few hours.

This repository contains the trial version of C++ versions of Effects SDK that you can integrate into your project/product to see how it will work in real conditions.

Also, there is the Demo Applications with Effects SDK integration, so you can just run it right away and see SDK in action.

## Obtaining Effects SDK License

To receive a Effects SDK license please fill in the contact form on [effectssdk.com](https://effectssdk.com/contacts) website.

## Techical Details

- SDK available for macOS x64 and m1 platforms
- SDK available for Windows 10/11 x64 platform
- Also supported x86 applications which are running on x64 platform
- Frames preprocessing/postprocessing could be run on CPU or GPU.
- ML inference could be run only on CPU or Apple Neural Engine(For apple platforms only).

## Features

- Virtual backgrounds (put image as a background) - **implemented**
- Background blur - **implemented**
- Beautification/Touch up my appearance - **implemented**
- Auto framing/Smart Zoom - **implemented**
- Auto color correction - **implemented**
- Color grading - **implemented**
- Background denoise - **in progress**

## Usage details

The entry point of the SDK is the instance of (ISDKFactory)(#isdkfactory). 
Using an **ISDKFactory** instance you will be able to prepare frames for processing and configure the pipeline of processing (enable transparency, blur, replace background etc).

### How to obtain a tsvb::ISDKFactory instance

- Load the tsvb.dll with the usage of **LoadLibrary()** for Windows or **dlopen** for macOS/linux.
- Get the address of **createSDKFactory()** function from the dll. Cast it to **::tsvb::pfnCreateSDKFactory** type.
- Call the **createSDKFactory()** function to instantiate a **::tsvb::ISDKFactory** object.
- Authorize instance to use it. Call [ISDKFactory::auth](#isdkfactory-auth) method to perform authorization.

```cpp
::tsvb::ISDKFactory* createFactory()
{
    HMODULE hModule = ::LoadLibrary("tsvb");
    auto createSDKFactory = reinterpret_cast<::tsvb::pfnCreateSDKFactory>(
        GetProcAddress(hModule, "createSDKFactory")
    );
    return createSDKFactory();
}
```

### Memory management

All classes created by the SDK implements **IRelease**. **IRelease** interface provides a **release()** method which releases the memory that was allocated for the object. The **release()** method should be called explicitly when such an object must be destroyed.

```cpp
::tsvb::ISDKFactory* sdkFactory = createFactory();
//some code
sdkFactory->release();
```

Don’t use operator **delete** for the SDK objects.

### Library Usage

Preparation:
- Create an instance of **IFrameFactory**.
- Create an instance of **IPipeline**.
- Enable background blur using **IPipeline::enableBlurBackground()** or background replacement using **IPipeline::enableReplaceBackground()**.
- When the background replacement is enabled you need to pass image which will be used as a background: **IReplacementController::setBackgroundImage()**

Frame processing:
- Put your frame to **IFrame** using **IFrameFactory::create()**.
- Process it through **IPipeline::process()**.

For apple platforms you can use **CVPixelBuffer** for better performance. See [IAppleCompatFrameFactory](#iapplecompatframefactory).

Use separate **IPipeline** instances per video stream.

```cpp
void initialize()
{
    ::tsvb::ISDKFactory* factory = createFactory();
    frameFactory = factory->createFrameFactory();
    pipeline = factory->createPipeline();
    pipeline->enableReplaceBackground(&replacementController);

    factory->release();
}

void release()
{
    frameFactory->release();
    frameFactory = nullptr;
    replacementController->release();
    replacementController = nullptr;
    pipeline->release();
    pipeline = nullptr;
}
```

More usage details see in: **Sample/BGReplacer.cpp**.

## How to build the sample

### Windows

Requirements:
- Windows 10
- CMake >= 3.16
- Visual Studio 2019
- OpenCV >= 4.5
- Qt 6.5

Build with cmake
```
mkdir build && cd build
cmake -DEFFECTS_SDK_PATH=<Path to the unpacked SDK> ..
cmake --build .. --config Release
```

### Mac OS

Requirements
- Xcode >= 13.4
- CMake >= 3.16
- OpenCV >= 4.5
- Qt 5.15

Build with cmake
```sh
mkdir build && cd build
cmake -G Xcode -DEFFECTS_SDK_PATH=/path/to/unpacked_sdk ..
cmake --build .. --config Release
```

### Linux Ubuntu

How to build with Ubuntu 20.04

To build the sample, needed to install deps from install_build_deps_astra.sh
```sh
sudo /path/to/install_build_deps_astra.sh
```

```sh
mkdir build && cd build
cmake -DEFFECTS_SDK_PATH=/path/to/unpacked_sdk /path/to/sample/source
cmake --build . --config Release
```

How to run
```sh
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/path/to/libtsvb.so
./VideoEffectsSDK
```

## Class Reference

### ISDKFactory

<a id="isdkfactory-auth"></a>

**ISDKFactory::auth()** - authorize the instance. The SDK can be used until authorized.  
Parameters:
- **const char\* customerID** - Customer ID string, unique identifier.
- **pfnOnAuthCompletedCallback callback** - \[Optional\] Callback to be called on async authorization completed.
- **void\* ctx** - Opaque callback context, will be passed as it is into callback.

Returns: [IAuthResult](#iauthresult)

Authorization performs web request to check passed customer ID and get license info.  

Can be used synchronously and asynchronously.  
To use synchronously, pass a null pointer at callback. In this case, method return pointer to [IAuthResult]() after authorization finished.  
To use asynchronously, pass nonnull pointer at callback. In this case, method perform authorization asynchronously.  
In asynchronous mode, the method returns null pointer. A valid pointer to **IAuthResult** will be passed into the callback.  

Callback signature  
```cpp
void TSVB_CALLBACK authCompletionCallback(IAuthResult* result, void* ctx);
```
Don't call the **IAuthResult::release** instance received within the callback in asynchronous mode, result will be deleted after callback finished.  
**IAuthResult** instance received in synchronous mode is owned by the caller and must be released when it isn't needed anymore.  

Example of usage in synchronous mode.
```cpp
IAuthResult* authResult = sdkFactory->auth(customerID, nullptr, nullptr);
if (authResult->status() == AuthStatus::active) {
    // SDK can be used.
}
else {
    // Authorization failed.
}
authResult->release();
```

Example of usage in asynchronous mode.
```cpp
// Note: Don't call IAuthResult::release within the callback.
void TSVB_CALLBACK authCompletionCallback(IAuthResult* result, void* ctx)
{
    HandlerClass* handler = reinterpret_cast<HandlerClass*>(ctx);
    handler->handleAuthResult(result);
}

// ...

// Ignore returned value. In asynchronous mode auth returns null pointer.
sdkFactory->auth(customerID, &authCompletionCallback, handler);
```

**ISDKFactory::waitUntilAuthFinished()** - Wait until async authorization is finished.  

If **ISDKFactory::auth()** called in synchronous mode, then returns immediately. 

Also waits until the callback is finished. Don't call **ISDKFactory::waitUntilAuthFinished()** within the callback.
Don't unload SDK's library until async authorization is finished. This method can be used to be sure async authorization is finished before unloading dynamic library.

**ISDKFactory::createFrameFactory()** - create instance of IFrameFactory.  
**ISDKFactory::createPipeline()** - create instance of IPipeline.  

### IAuthResult

**IAuthResult::status()** - returns authorization status.  

**AuthStatus**
- **error** - An error occurred during web request. 
- **active** - Authorization is succeeded and the license is active. SDK can be used to enhance your video.
- **inactive** - Authorization is failed because the license is deactivated or no such license.
- **expired** - Authorization is failed because the license is expired. Contact us to update it.

**IAuthResult::obtainError()** - returns error object. See [IError](#ierror).    

### IFrameFactory

**IFrameFactory** can be created by calling **ISDKFactory::createFrameFactory()**.

**IFrameFactory::createBGRA()** - create **IFrame** from the raw BGRA data.
Parameters:
- **void\* data** - pointer to BGRA data.
- **unsigned int bytesPerLine** - number of bytes per line of frame.
- **unsigned int width** - number of pixels in horizontal direction.
- **unsigned int height** - number of pixels in vertical direction.
- **bool makeCopy** - if set to true - **IFrame** will copy the data, other way **IFrame** will keep the pointer to data **(DON'T release the data while its processing)**

**IFrameFactory::createNV12()** - create **IFrame** from the raw NV12 data.
Parameters:
- **void\* yData** - pointer to Y component data.
- **unsigned int yBytesPerLine** - number of bytes in one line of Y component matrix. 
- **void\* uvData** - pointer to UV component data.
- **unsigned int uvBytesPerLine** - number of bytes in one line of UV component matrix
- **unsigned int width** - number of pixels in horizontal direction.
- **unsigned int height** - number of pixels in vertical direction.
- **bool makeCopy**  - the same behaviour as for **IFrameFactory::createBGRA()**.


**IFrameFactory::loadImage()** - load data from the image and create **IFrame**. Return the NULL if the instance is not created.
Parameters:
- **const char\* utf8FilePath** - path to the image file. Path should be in UTF-8.

**IFrameFactory::getInterface()** - Returns the specific interface to get access to platform-specific APIs.  
Parameters:
- **InterfaceTypeID interfaceTypeID** - See [InterfaceTypeID](#interfacetypeid)

Use reinterpret_cast\<T\> to cast returned pointer to decided interface.

**IFrameFactory** supports **IAppleCompatFrameFactory** interface to wrap **CVPixelBuffer** in **IFrame** to use with the pipeline.

To get **IAppleCompatFrameFactory** pass **InterfaceTypeID::appleCompatFrameFactory**.

### IAppleCompatFrameFactory

**IAppleCompatFrameFactory::createFrameWithCVPixelBuffer()** - Creates **IFrame** with CVPixelBuffer.  
Parameters:
- **CVPixelBufferRef pixelBuffer** - [CVPixelBufferRef](https://developer.apple.com/documentation/corevideo/cvpixelbuffer-q2e)  
- **bool metalCompatible** - Set true when **CVPixelBuffer** can be used in metal directly. 

Recommended to use metal compatible **CVPixelBuffer**. The argument has not effect if CPU pipeline.
If you does not know *pixelBuffer* is metal compatible or not then set *metalCompatible* to false.
**CVPixelBuffer** received within [AVCaptureVideoDataOutputSampleBufferDelegate.captureOutput(_:didOutput:from:)](https://developer.apple.com/documentation/avfoundation/avcapturevideodataoutputsamplebufferdelegate/captureoutput(_:didoutput:from:)) from [AVCaptureSession](https://developer.apple.com/documentation/avfoundation/avcapturesession) is metal compatible.

For manual creating, to create metal compatible **CVPixelBuffer**, set [kCVPixelBufferMetalCompatibilityKey](https://developer.apple.com/documentation/corevideo/kcvpixelbuffermetalcompatibilitykey) attribute.

 Supported formats:
- kCVPixelFormatType_32BGRA 
- kCVPixelFormatType_32RGBA
- kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange
- kCVPixelFormatType_420YpCbCr8BiPlanarFullRange

Example
```cpp
auto appleCompatFrameFactory = reinterpret_cast<IAppleCompatFrameFactory*>(
    frameFactory->getInterface(InterfaceTypeID::appleCompatFrameFactory)
);

IFrame* frame = 
    appleCompatFrameFactory->createFrameWithCVPixelBuffer(cvPixelBuffer, true);
```

### IFrame

**FrameFormat** - format of data representation.
- **bgra32** - format with 8 bit per channel (32 bits per pixel)
- **nv12** - NV12 format.

**IFrame::frameFormat()** - return format of data.

**IFrame::width()** - return number of pixels in horizontal direction.
**IFrame::height()** - return number of pixels in vertical direction.

**IFrameFactory::getInterface()** - Returns the specific interface to get access to platform-specific APIs.  
Parameters:
- **InterfaceTypeID interfaceTypeID** - See [InterfaceTypeID](#interfacetypeid)

Use reinterpret_cast\<T\> to cast returned pointer to decided interface.  
**IFrame** supports **IAppleCompatFrame** interface to convert/obtain **CVPixelBuffer** from **IFrame**.  
To get **IAppleCompatFrameFactory** pass **InterfaceTypeID::appleCompatFrame**.  

**IFrame::lock()** - get access to virtual memory of process. Return ILockedFrameData interface which provides ability to get pointers to internal data of IFrame **(DON’T use the IFrame until ILockedFrameData wasn’t released)**.

Parameters:
- **int frameLock** - could be **FrameLock::read**, **FrameLock::write** or **FrameLock::readWrite**. 

**ILockedFrameData** - keep access to the data inside IFrame and return pointers to that data.
If it was obtained with **IFrame::lock()** with param **FrameLock::write** or **FrameLock::readWrite**, then the changes will be applied after **ILockedFrameData** will be released.  
If it was obtained with **IFrame::lock()** with param **FrameLock::read**, then don’t change the data.


**ILockedFrameData::dataPointer()** - return pointer to component data.
Parameters:
- **int planarIndex** - depend on frame format. For FrameFormat::bgra32 always should be used 0. For FrameFormat::nv12  - 0 return pointer to Y component, 1 return pointer  to UV component. 

**ILockedFrameData::bytesPerLine()** - return number of bytes per line.
Parameters:
- **int planarIndex** - see **ILockedFrameData::dataPointer()**.

### IAppleCompatFrame

**IAppleCompatFrame::toCVPixelBuffer()** - Convert internal storage to **CVPixelBuffer**.  

**IFrame** does not be used in pipeline after this method called.  
If you want to use **CVPixelBuffer** after **IFrame** released, increase **CVPixelBuffer** reference count by using [CVPixelBufferRetain](https://developer.apple.com/documentation/corevideo/1563590-cvpixelbufferretain).

Example
```cpp
auto appleCompatFrame = reinterpret_cast<IAppleCompatFrame>(
    frame->getInterface(InterfaceTypeID::appleCompatFrame)
);

CVPixelBufferRef pixelBuffer = appleCompatFrame->toCVPixelBuffer();
```

### IPipeline

**PipelineErrorCode** - error codes for **IPipeline**.
- **ok** - success
- **invalidArguemnt** - one or more arguments are incorrect.
- **noFeaturesEnabled** - processing pipeline is not configured.
- **engineInitializationError** - can’t initialize OpenVINO, the hardware/software is not supported.
- **resourceAllocationError** -  not enough memory, disc space etc.

**IPipeline** - configuration of effects and frame processor. Use separate instances for each video stream.

**IPipeline::setConfiguration()** - Configure pipeline, determine what to use for image processing (see **IPipelineConfiguration**), for example, GPU or CPU pipeline. This method is optional. 
Parameters:
- **const IPipelineConfiguration\* config** - configuration to apply.

**IPipeline::copyConfiguration()** - Return a copy of current configuration. The caller is responsible for releasing the returned object. 

**IPipeline::copyDefaultConfiguration()** - Return a copy of the default configuration. The caller is responsible for releasing the returned object. 

**IPipeline::enableBlurBackground()** - enable background blur.
Parameters:
- **float blurPower** - power of blur from 0 to 1.

**IPipeline::disableBackgroundBlur()** - disable background blur. 

**IPipeline::getBlurBackgroundState()** - return true if background blur is enabled, otherway false.
Parameters:
- **float* blurPower** - if not NULL then blur power.

**IPipeline::enableReplaceBackground()** - enable background replacement, by default background is transparent. The custom image for the background could be set using
**IReplacementController::setBackgroundImage()**. If background blur is enabled, then the custom image will be also blurred.

Parameters:
- **IReplacementController\*\* controller** - if not NULL then will be a new instance of **IReplacementController**. Caller is responsible to release the obtained instance when it is not needed anymore.

**IPipeline::disableReplaceBackground()** - disable background replacement.

**IPipeline::getReplaceBackgroundState()** - return true if background replacement is enabled, otherwise false.

**IPipeline::enableBeautification()** - enable face beautification. 
Note: For macOS only GPU pipeline is available with beautification.

**IPipeline::disableBeautification()** - disable face beautification.

**IPipeline::setBeautitifcationLevel()** - set face beautification level.
Parameters:
- **float level** - level could be from 0 to 1. Higher number -> more visible effect of beautification.

**IPipeline::getBeautitifcationLevel()** - return current beautification level.

**IPipeline::enableColorCorrection()** - enable automatic color correction. Improves colors with the help of ML.
Disables another enabled color correction effect.
Note: Preparation starts asynchronously after a frame process, the effect may be delayed.

**IPipeline::enableColorCorrectionWithReference()** - enable color grading. Generate a color palette from an image and apply it to the video.
Parameters:
- **IFrame\* referenceFrame** - The reference to generate a color palette.
If enabled, generates a new color palette with referenceFrame.
Disables another enabled color correction effect.

**IPipeline::enableColorCorrectionWithLutFile()** - enable color filtering with a Lookup Table (Lut).
Parameters:
- **const char* utf8FilePath** - path to .cube file. Supports only 3D Lut with maximum size 65 (65x65x65).
If enabled, switches Lut.
Disables another enabled color correction effect.

**IPipeline::disableColorCorrection()** - disable color correction.

**IPipeline::setColorCorrectionPower(float power)** - set visibility of the color correction effect. 
Parameters:
- **float power** - power could be from 0 to 1. Higher number -> more visible effect of color correction;

**IPipeline::enableSmartZoom()** - enable smart zoom.
Smart Zoom crops around the face.

**IPipeline::disableSmartZoom()** - disable smart zoom.

**IPipeline::setSmartZoomLevel()** - set smart zoom level.
Parameters:
- **float level** - level could be from 0 to 1. Defines how much area should be filled by a face. Higher number -> More area. 

**IPipeline::enableDenoiseBackground()** - enable denoising effect.
By default this effect denoises only background. To denoise foreground too, pass true to IPipeline::setDenoiseWithFace.

**IPipeline::disableBackgroundDenoise()** - disable denoising effect.

**IPipeline::setDenoisePower()** - set denoise filter power.
Parameters:
- **float power** - power could be from 0 to 1. Higher number -> stronger filtering. Default is 0.8.

**IPipeline::getDenoisePower()** - return current denoise power.

**IPipeline::setDenoiseWithFace()** 
Parameters:
- **bool withFace** - if true then the filter will denoise foreground too, otherwise background only. Default is false.

**IPipeline::getDenoiseWithFace()** - return denoiseWithFace state.

**IPipeline::getDenoiseBackgroundState()** - return true if denoise is enabled, otherwise false.

**IPipeline::enableLowLightAdjustment()** - enable the brightening effect.  
Low Light Adjustment enhances the brightness of a dark video. It is useful when the video has a darker environment.  
It's recommended to use together with Color Correction.

**IPipeline::disableLowLightAdjustment()** - disable the brightening effect.  

**IPipeline::setLowLightAdjustmentPower()** - set the power of the brightening effect.  
Parameters:  
- **float power** - power could be from 0 to 1. Higher number -> brighter result. Default is 0.6.

**IPipeline::getLowLightAdjustmentPower()** - get the power of the brightening effect.  

**IPipeline::enableSharpening()** - enable sharpening effect.  
Sharpening makes the video look better by enhancing its clarity. It reduces blurriness in the video.  

**IPipeline::disableSharpening()** - disable sharpening effect.  

**IPipeline::getSharpeningPower()** - get current power of the sharpening effect.  

**IPipeline::setSharpeningPower()** - set current power of the sharpening effect.  
Parameters:  
- **float power** - power could be from 0 to 1. Higher number -> sharper result.

**IPipeline::process()** - return processed frame the same format with input (with all effects applied). In case of error, return NULL.
Parameters:
- **const IFrame\* input** - frame for processing.
- **PipelineError\* error** - NULL or error code.

**IReplacementController::setBackgroundImage()** - set new custom background image.
Parameters: 
- **const IFrame\* image** - custom image for the background, don’t release while processing.

**IReplacementController::clearBackgroundImage()** - clear custom background image, the background will be transparent.

### IPipelineConfiguration

**Backend** - backend of pipeline.
- **CPU** - CPU-based pipeline.
- **GPU** - GPU-based pipeline.

**IPipelineConfiguration::setBackend()** - Set backend parameter.
Parameters:
- **int backend** - must be one of Backend values.
Default is GPU

**IPipelineConfiguration::backend()** - return backend parameter.

**SegmentationPreset** - Segmentation mode allow to choose combination of quality and speed of segmentation.
- **segmentationPresetQuality** - Quality is preferred.
- **segmentationPresetBalanced** - Balanced quality and speed.
- **segmentationPresetSpeed** - Speed is preferred.
- **segmentationPresetLightning** - Speed is prioritized.  

*Note*: On Apple systems, **segmentationPresetSpeed** requires macOS 11.0 or iOS 14.

**IPipelineConfiguration::setSegmentationPreset()** - set segmentation preset.
Parameters:
- **int preset** - must be one of SegmentationPreset values.
Default is segmentationPresetBalanced.

**IPipelineConfiguration::getSegmentationPreset()** - return current segmentation preset.

**MLBackend**
- **mlBackendCPU**
- **mlBackendAppleNeuralEngine** - has effects on apple platforms only.

**IPipelineConfiguration::getSegmentationMLBackends()** - returns mask with enabled **MLBackends**.

**IPipelineConfiguration::setSegmentationMLBackends()** - Set mask with enabled **MLBackends**.
Parameters:
- **int mlBackends** - Enabled backends mask.

Example
```cpp
int backends = MLBackend::mlBackendCPU;
if (neuralEngineEnabled) {
    backends |= MLBackend::mlBackendAppleNeuralEngine;
}
config->setSegmentationMLBackends(backends);
```

### IError

**IError::description()** - returns null terminated utf8 string with description of error.  

**IError::nativeErrorType()** - returns the type of contained native error.  
**IError::nativeError()** - returns contained native error, see NativeErrorType.  
 
**NativeErrorType**
- **none** - No native error contained.
- **nsError** - Contained an instance of [NSError](https://developer.apple.com/documentation/foundation/nserror).

```objective-c
IError* error = authResult->obtainError();
NSError* nsError = (__bridge NSError*)error->nativeError();
// If ARC is not enabled. [nsError retain];
error->release();
```

### InterfaceTypeID

- **appleCompatFrame** - [IAppleCompatFrame](#iapplecompatframe).
- **appleCompatFrameFactory** - [IAppleCompatFrameFactory](#iapplecompatframefactory).

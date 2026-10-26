# Video Effects SDK - Windows and macOS sample

Add real-time AI video enhancement that makes video meeting experience more effective and comfortable to your application in a few hours.

This repository contains the sample application of Video Effects SDK for Windows and macOS: a Qt QML application that
applies the effects of the SDK to the video of a camera. Build it with the SDK package to see the SDK in action and to
see how to integrate it into your project.

## Trial Evaluation

A Customer ID is required for the Effects SDK.

To create a Customer ID, view pricing, and activate trials, please register on the developer portal here [effectssdk.ai](https://effectssdk.ai/cp/registration) website.

## Technical Details

- Windows 10/11 x64. x86 applications are supported on x64 Windows.
- macOS 11 and newer, Apple silicon and Intel. The sample itself needs macOS 12, as Qt 6.8 does.
- Frames are processed on the GPU (DirectX 11 on Windows, Metal on macOS) or on the CPU.
- ML inference runs on the CPU on Windows; on macOS through Core ML, on the CPU, the GPU or the Apple Neural Engine.
- 2 segmentation presets: Quality and Balanced.

## Features

- Background blur and background replacement with an image or transparency.
- Beautification.
- Smart zoom, which crops the frame around the face.
- Color correction, automatic and manual; color grading by a reference image; color filtering with a lookup table.
- Low light adjustment.
- Sharpening.
- Noise suppression.
- Portrait lighting, which brightens the person and darkens the background.

## Documentation

- [API Reference](https://effectssdk.ai/sdk/desktop/docs/)

## Usage

The entry point of the SDK is **tsvb::ISDKFactory**. On macOS link `libtsvb.dylib` and call
**tsvb::createSDKFactory()**. On Windows either link `tsvb.lib` and call it the same way, or load `tsvb.dll` at run
time:

```cpp
::tsvb::ISDKFactory* createFactory()
{
    HMODULE hModule = ::LoadLibraryA("tsvb");
    auto createSDKFactory = reinterpret_cast<::tsvb::pfnCreateSDKFactory>(
        GetProcAddress(hModule, "createSDKFactory")
    );
    return createSDKFactory();
}
```

On macOS, linked with libtsvb.dylib, and on Windows, linked with tsvb.lib:

```cpp
::tsvb::ISDKFactory* sdkFactory = ::tsvb::createSDKFactory();
```

Authorize the factory, create an **IFrameFactory** and an **IPipeline** for each video stream, enable the effects and pass the frames of the stream to **IPipeline::process()** one after another, in their order. Release every object created by the SDK with **release()**, don't use operator **delete** for them.

```cpp
bool initialize(const char* customerID, const char* backgroundPath)
{
    ::tsvb::ISDKFactory* factory = createFactory();
    ::tsvb::IAuthResult* authResult = factory->auth(customerID, nullptr, nullptr);
    const ::tsvb::AuthStatus status = authResult->status();
    authResult->release();
    if (::tsvb::AuthStatus::active != status) {
        // Authorization failed: report the status, the SDK can't be used.
        factory->release();
        return false;
    }

    frameFactory = factory->createFrameFactory();
    pipeline = factory->createPipeline();
    factory->release();

    pipeline->enableNoiseSuppression();
    pipeline->enableReplaceBackground(&replacementController);

    ::tsvb::IFrame* background = frameFactory->loadImage(backgroundPath);
    if (nullptr != background) {
        replacementController->setBackgroundImage(background);
        background->release();
    }
    return true;
}

// For each frame of the stream:
::tsvb::IFrame* processFrame(void* bgra, unsigned int bytesPerLine, unsigned int width, unsigned int height)
{
    ::tsvb::IFrame* input = frameFactory->createBGRA(bgra, bytesPerLine, width, height, false);
    ::tsvb::IFrame* output = pipeline->process(input, nullptr);
    input->release();
    return output;
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

## The SDK in this sample

- **video_filter.cpp** - creating and authorizing the SDK factory, creating the pipeline, processing a camera frame.
- **sample_controller.cpp** - enabling the effects and changing their settings.
- **sdk_library_handler_win.cpp** - loading `tsvb.dll` at run time on Windows; **sdk_library_handler_linked.cpp** -
  the SDK linked instead, on macOS and on Windows with `EFFECTS_SDK_LINK`.
- **capture_loop_win.cpp**, **capture_loop_mac.cpp** and **camera/** - camera frames into the SDK without copies on the
  CPU: NV12 from Media Foundation on Windows, `CVPixelBuffer` from AVFoundation on macOS.

## Authorization

Replace `CUSTOMER_ID` in **video_filter.cpp** with your customer ID before you build the sample. Without it the
sample starts but reports that the authorization failed, and the effects are unavailable.
[`ISDKFactory`](https://effectssdk.ai/sdk/desktop/docs/classtsvb_1_1_i_s_d_k_factory.html) describes the
authorization, the offline one with a key too.

## How to build the sample

The sample is a CMake project. `EFFECTS_SDK_PATH` points at the unpacked SDK package.

### Windows

Requirements:
- Windows 10/11
- Visual Studio 2022 with the "Desktop development with C++" workload
- CMake 3.26 or newer
- Qt 6.8.4 or newer, the MSVC 2022 64-bit kit with the Qt Multimedia module

`EFFECTS_SDK_PATH` is the directory holding `include`, `bin_64`, `lib_64`, `bin_32` and `lib_32`.

```sh
cmake -S . -B build -DEFFECTS_SDK_PATH=C:/path/to/sdk -DQt6_DIR=C:/Qt/6.8.4/msvc2022_64/lib/cmake/Qt6
cmake --build build --config Release
cmake --build build --config Release --target DeployQt
```

The build copies the SDK binaries next to `build\Release\Video Effects SDK.exe`; the `DeployQt` target puts the Qt
libraries there with `windeployqt`. Then run the executable.

For an x86 build, configure with `-A Win32` and a 32-bit MSVC 2022 Qt. Qt 6 has no prebuilt 32-bit kits, so build
it from the source. The x86 SDK delegates the ML inference to its 64-bit helper process, `x64\TSVBEngineProc.exe`
next to `tsvb.dll`; the build copies it too.

To link `tsvb.lib` instead of loading `tsvb.dll` at run time, add `-DEFFECTS_SDK_LINK=ON`.

### macOS

Requirements:
- macOS 12 or newer
- Xcode with the command line tools
- CMake 3.26 or newer
- Qt 6.8.4 or newer for macOS with the Qt Multimedia module

`EFFECTS_SDK_PATH` is the directory holding `include` and `libtsvb.dylib`.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DEFFECTS_SDK_PATH=/path/to/sdk -DQt6_DIR=$HOME/Qt/6.8.4/macos/lib/cmake/Qt6
cmake --build build --parallel
open "build/Video Effects SDK.app"
```

The build copies `libtsvb.dylib` into the bundle. The sample asks for access to the camera on the first start.

The bundle uses the Qt it was built with. To run it on another Mac, put Qt into the bundle and sign it again, ad hoc
here:

```sh
$HOME/Qt/6.8.4/macos/bin/macdeployqt "build/Video Effects SDK.app" -qmldir=qml
codesign --force --deep --sign - --entitlements Sample.entitlements "build/Video Effects SDK.app"
```

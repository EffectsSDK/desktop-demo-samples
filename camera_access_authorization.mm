#include "camera_access_authorization.h"

#import <AVFoundation/AVCaptureDevice.h>

CaptureAuthorizationStatus videoCaptureAuthorizationStatus()
{
    auto status = [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeVideo];
    switch(status) {
        case AVAuthorizationStatusRestricted:
            return CaptureAuthorizationStatus::restricted;
        case AVAuthorizationStatusDenied:
            return CaptureAuthorizationStatus::denied;
        case AVAuthorizationStatusAuthorized:
            return CaptureAuthorizationStatus::authorized;
            
        default:
            break;
    }
    
    return CaptureAuthorizationStatus::notDeterminated;
}

// Use in main thread
void requestVideoCaptureAccess(std::function<void(bool)> handler)
{
    [AVCaptureDevice requestAccessForMediaType:AVMediaTypeVideo
                             completionHandler:^(BOOL granted) {
        handler(granted);
    }];
}

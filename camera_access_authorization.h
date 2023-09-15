#ifndef CAMERA_ACCESS_AUTHORIZATION_H
#define CAMERA_ACCESS_AUTHORIZATION_H

#include <functional>

enum class CaptureAuthorizationStatus
{
    notDeterminated,
    restricted,
    denied,
    authorized
};

CaptureAuthorizationStatus videoCaptureAuthorizationStatus();

// Use in main thread
void requestVideoCaptureAccess(std::function<void(bool)> handler);

#endif

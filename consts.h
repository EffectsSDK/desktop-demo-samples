#ifndef CONSTS_H
#define CONSTS_H

static const char DEFAULT_CAMERA_SCALE[] = "720p";
static const int DEFAULT_SCALE_WIDTH = 1280;
static const int DEFAULT_SCALE_HEIGHT = 720;

enum class Backend
{
	gpu,
	cpu
};

enum class Preset : int
{
	quality = 1,
	balanced = 2,
	speed = 3,
	lightning = 4
};

#endif
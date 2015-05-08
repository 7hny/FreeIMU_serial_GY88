#ifndef FREEIMU_SETTINGS_H
#define FREEIMU_SETTINGS_H

#include "Arduino.h"

#define HAS_GPS 1

#define gpsSerial Serial1
#define outSerial Serial

static const long GPSBaud = 9600;
static const long outBaudRate = 115200;

#endif
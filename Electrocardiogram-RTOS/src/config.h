#include <../lib/TFT_eSPI-master/TFT_eSPI.h>  // Hardware-specific library
#include <Arduino.h>
#include <SPI.h>
#include <WiFi.h>
#include <string.h>

#include <cstdlib>
#include <deque>
#include <iostream>
#include <random>
#include <cmath> // for std::sin and M_PI

#include "../lib/ESP32AnalogRead/ESP32AnalogRead.h"
#include "FreeRTOS.h"
#include "SPIFFS.h"
#include "lwip/api.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"

#define WEBSERVER_TASK_STACKSIZE (8192)
#define HTTP_PORT 80
#define MAX_CONN 5

#define MAX_BUFFER_SIZE 1800  // 5 seconds of data recorded at 360 Hz

TFT_eSPI tft = TFT_eSPI();  // Invoke custom library

#define TFT_GREY 0x5AEB

#define LOOP_PERIOD 35  // Display updates every 35 ms
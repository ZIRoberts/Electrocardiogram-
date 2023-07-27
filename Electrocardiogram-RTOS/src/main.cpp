/*
 *  Electrocardiogram RTOS
 *
 *  Created on: 16 June, 2023
 *      Author: Zachary Roberts
 *
 *      This program is used to acquire and process the signals acquired by the
 *      electodecaridogram project done in collaboration with
 *      York College of Pennsylvania Embedded System's Course course.
 */

#include <../lib/ESP32AnalogRead/ESP32AnalogRead.h>
#include <Arduino.h>
#include <freertos/queue.h>

#include <cstdlib>
ESP32AnalogRead senseECG;
double ECGVoltage = 0;
long timeECG = 0;
/**
 * @brief Updates the position of the servo motors based on the result of the
 *        signal processing. Currently only a simple threshold comparision
 *        is implemented.
 *
 * @param pvParameter Void Pointer
 */
void acquireECG(void *pvParameter) {
  while (1) {
    ECGVoltage = senseECG.readMilliVolts();
    // timeECG = millis();
    // Serial.print(timeECG);
    // Serial.print(',');
    Serial.println(ECGVoltage);
    // Updates servo position every 2.777 ms (360 Hz)
    vTaskDelay(2.777 * portTICK_PERIOD_MS);
  }
}

/**
 * @brief Sets up, configures, and initializes all necessary objects and tasks
 *
 */
void setup() {
  // Starts serial communication at 115200 baud rate
  Serial.begin(115200);
  while (!Serial)
    ;
  // Set CPU clock to 80MHz
  setCpuFrequencyMhz(80);  // Can be set to 80, 160, or 240 MHZ
  esp_log_level_set("*", ESP_LOG_DEBUG);
  senseECG.attach(9);
  // Create RTOS Tasks
  TaskHandle_t xHandle = NULL;

  xTaskCreatePinnedToCore(   // Use xTaskCreate() in vanilla FreeRTOS
      acquireECG,            // Function to be called
      "Acquire ECG Signal",  // Name of task
      1024,                  // Stack size (bytes in ESP32, words in FreeRTOS)
      NULL,                  // Parameter to pass to function
      0,                     // Task priority (0 to configMAX_PRIORITIES - 1)
      &xHandle,              // Task handle
      1);                    // Run on core 1
}

void loop() {}

#include "FS.h"
#include "config.h"

// put function declarations here:
void vWebServerTask(void*);
int getData();
void connectToWiFi();
void createWifiNetwork();
void serveHTML(int);
void serveJS(int);
const char* getHeartPrediction();
std::vector<int> getEcgWebGraphData();


static const int led_pin = LED_BUILTIN;
static const BaseType_t wifi_cpu = 0;  // use CPU 0
static const BaseType_t app_cpu = 1;   // use CPU 1

ESP32AnalogRead senseECG;
int ECGVoltage = 0;

// Buffer defintion for EMG Sensor
static std::deque<int> ecgBuffer;
static std::deque<int> ecgTrashBuffer;

// Random Number 1 through 10
std::random_device rd;
std::mt19937 gen(rd());
std::uniform_int_distribution<int> dis(1, 10);

// TFT example data
float ltx = 0;                  // Saved x coord of bottom of needle
uint16_t osx = 120, osy = 120;  // Saved x & y coords
uint32_t updateTime = 0;        // time for next update

int old_analog = -999;   // Value last displayed
int old_digital = -999;  // Value last displayed

int value[6] = {0, 0, 0, 0, 0, 0};
int old_value[6] = {-1, -1, -1, -1, -1, -1};
int d = 0;

std::vector<int> getEcgWebGraphData() {
  std::vector<int> ecgWebGraphData;

  // Copy the last 60 elements from the deque into the vector
  int numDatapoints = std::min(static_cast<int>(ecgBuffer.size()), 60);
  std::deque<int>::reverse_iterator rit = ecgBuffer.rbegin();

  for (int i = 0; i < numDatapoints; ++i) {
      ecgWebGraphData.insert(ecgWebGraphData.begin(), *rit);
      ++rit;
  }
  
  return ecgWebGraphData;
}

int getData() {
  // ECG DATA
  int randomNumber = dis(gen);
  return randomNumber;
}

const char* getHeartPrediction() { return "Heart Status: Looking Good"; }

void vWebServerTask(void* pvParameters) {
  int tcp_socket, received_connection;
  struct sockaddr_in addr;
  char buffer[1024];

  tcp_socket = lwip_socket(AF_INET, SOCK_STREAM, 0);
  if (tcp_socket < 0) {
    vTaskDelete(NULL);
  }

  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(HTTP_PORT);
  addr.sin_addr.s_addr = INADDR_ANY;

  if (lwip_bind(tcp_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
    lwip_close(tcp_socket);
    vTaskDelete(NULL);
  }

  // Begin Listening for connections. Can queue up MAX_CONN connections
  if (lwip_listen(tcp_socket, MAX_CONN) < 0) {
    lwip_close(tcp_socket);
    vTaskDelete(NULL);
  }

  // Infinite loop to accept incoming connections
  while (1) {
    // Accept a connection on the socket
    received_connection = lwip_accept(tcp_socket, NULL, NULL);

    // If a valid connection is established
    if (received_connection > 0) {
      // Clear the buffer
      memset(buffer, 0, sizeof(buffer));

      // Read the incoming request into the buffer (What is the request asking
      // for, a specific page?)
      lwip_read(received_connection, buffer, sizeof(buffer));

      // Checking if the request starts with "Get /data", if it does, it is from
      // our javascript.
      if (strstr(buffer, "GET /data ") == buffer) {
        std::vector<int> ecgData = getEcgWebGraphData();

        std::string jsonResponse = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n[";

        for (size_t i = 0; i < ecgData.size(); ++i) {
            if (i != 0) jsonResponse += ',';
            jsonResponse += std::to_string(ecgData[i]);
        }

        jsonResponse += "]";

        const char *data_response = jsonResponse.c_str();

        lwip_write(received_connection, data_response, strlen(data_response));

        // char data_response[128];
        // snprintf(data_response, sizeof(data_response),
        //          "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n%d",
        //          getEcgWebGraphData());
        // lwip_write(received_connection, data_response, strlen(data_response));

      } else if (strstr(buffer, "GET /heartPrediction ") == buffer) {
        char data_response[128];
        snprintf(data_response, sizeof(data_response),
                 "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n%s",
                 getHeartPrediction());
        lwip_write(received_connection, data_response, strlen(data_response));
      } else if (strstr(buffer, "GET /chart.min.js ") == buffer) {
        Serial.println("Serving JavaScript");
        char js_header[] =
            "HTTP/1.1 200 OK\r\nContent-Type: application/javascript\r\n\r\n";
        lwip_write(received_connection, js_header, strlen(js_header));
        serveJS(received_connection);
      } else {
        Serial.println("Serving HTML");
        char html_header[] =
            "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";
        lwip_write(received_connection, html_header, strlen(html_header));
        serveHTML(received_connection);
      }

      // Close the connection socket
      lwip_close(received_connection);
    }
  }
}

// NOTE: The HTML file should be served in chunks like this
// because if the file might be too big to fit in memory all at once.
void serveHTML(int received_connection) {
  // Write the HTML file as the response
  File file = SPIFFS.open("/index.html");
  if (file) {
    while (file.available()) {
      char html_line[256];
      int i = 0;
      while (file.available() && i < sizeof(html_line)) {
        html_line[i++] = file.read();
      }
      lwip_write(received_connection, html_line, i);
    }
    file.close();
  }
}

void serveJS(int received_connection) {
  File file = SPIFFS.open("/chart.min.js");
  if (file) {
    while (file.available()) {
      char line[256];
      int i = 0;
      while (file.available() && i < sizeof(line)) {
        line[i++] = file.read();
      }
      lwip_write(received_connection, line, i);
    }
    file.close();
  }
}

void connectToWiFi() {
  // Leaking my network password :)
  const char* ssid = "NETGEAR46";  // Replace with your network SSID (name)
  const char* password = "royalfinch897";  // Replace with your network password

  vTaskDelay(1000 / portTICK_PERIOD_MS);  // Delay for a second
  WiFi.begin(ssid, password);             // Connect to the network

  while (WiFi.status() != WL_CONNECTED) {  // Wait for the Wi-Fi to connect
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    Serial.println("Connecting to WiFi...");
  }

  Serial.println("Connected to WiFi");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());  // Print the IP address
}

void createWifiNetwork() {
  const char* ssid =
      "ESP32-ECG";  // The name of the Wi-Fi network that will be created
  const char* password = NULL;  // The password for the Wi-Fi network

  WiFi.softAP(ssid, password);  // Create a Wi-Fi network

  Serial.println("WiFi Access Point (AP) Created");
  Serial.print("IP address: ");
  Serial.println(WiFi.softAPIP());  // Print the IP address
  Serial.println("WiFi Port: ");
  Serial.println(HTTP_PORT);  // Print the IP Port
}

// Verify that the HTML file can be accessed by SPIFFS.
void verifyFileSPIFFS() {
  if (!SPIFFS.begin(true)) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  File file = SPIFFS.open("/index.html");
  if (!file) {
    Serial.println("Failed to open file for reading");
    return;
  }
  Serial.println("File Successfully Opened");
  // Serial.println("File Content:");
  // while(file.available()){
  //   Serial.write(file.read());
  // }
  file.close();
}

/**
 * @brief Updates the position of the servo motors based on the result of the
 *        signal processing. Currently only a simple threshold comparision
 *        is implemented.
 *
 * @param pvParameter Void Pointer
 */
void acquireECG(void* pvParameter) {
  while (1) {
    // Reads tge ECG signal
    ECGVoltage = senseECG.readMilliVolts();

    // Print output to serial
    // Serial.println(ECGVoltage);

    // Removes 1500 mV offset and oppends onto end of buffer
    // Newest signal will be at end of buffer, oldest will be at start
    ECGVoltage -= 1500;
    ecgBuffer.push_back(ECGVoltage);

    // *** FOR TESTING ***
    int randomNumber = dis(gen);
    ecgTrashBuffer.push_back(randomNumber);
    if (ecgTrashBuffer.size() >= MAX_BUFFER_SIZE) {
      ecgTrashBuffer.pop_front();
    }
    Serial.println("Trash Buffer");

    // *** END TESTING *** 

    // Checks if buffer is full and removes oldest data if necessary
    if (ecgBuffer.size() >= MAX_BUFFER_SIZE) {
      ecgBuffer.pop_front();
    }


    vTaskDelay(2.777 * portTICK_PERIOD_MS);
  }
}

void updateLCD(void* pvParameter) {
  while (1) {
    vTaskDelay(35 * portTICK_PERIOD_MS);
  }
}

void setup() {
  tft.init();
  tft.setRotation(0);
  Serial.begin(115200);
  while (!Serial)
    ;
  // Set CPU clock to 80MHz
  setCpuFrequencyMhz(80);  // Can be set to 80, 160, or 240 MHZ
  esp_log_level_set("*", ESP_LOG_DEBUG);
  senseECG.attach(2);  // Set to 2,3, or 4 (need to short R15&R16 for GPIO 3&4)

  createWifiNetwork();
  // connectToWiFi();

  // Verify that the HTML File(s) can be accessed.
  verifyFileSPIFFS();

  TaskHandle_t xHandle = NULL;
  // Run Webserver
  xTaskCreatePinnedToCore(
      vWebServerTask,            // Function to be called
      "Webserver",               // Name of task
      WEBSERVER_TASK_STACKSIZE,  // Stack Size to use (bytes in ESP32)
      NULL,                      // parameter to pass to function
      1,                         // Task Priority (0 to configMAX_PRIORITIES)
      &xHandle,                  // Task Handle
      wifi_cpu);                 // Run on core 0

  xTaskCreatePinnedToCore(   // Use xTaskCreate() in vanilla FreeRTOS
      acquireECG,            // Function to be called
      "Acquire ECG Signal",  // Name of task
      2048,                  // Stack size (bytes in ESP32, words in FreeRTOS)
      NULL,                  // Parameter to pass to function
      0,                     // Task priority (0 to configMAX_PRIORITIES - 1)
      &xHandle,              // Task handle
      app_cpu);              // Run on core 1

  xTaskCreatePinnedToCore(   // Use xTaskCreate() in vanilla FreeRTOS
      updateLCD,             // Function to be called
      "Update LCD Display",  // Name of task
      4096,                  // Stack size (bytes in ESP32, words in FreeRTOS)
      NULL,                  // Parameter to pass to function
      0,                     // Task priority (0 to configMAX_PRIORITIES - 1)
      &xHandle,              // Task handle
      app_cpu);              // Run on core 1
}

void loop() {
  // put your main code here, to run repeatedly:
}

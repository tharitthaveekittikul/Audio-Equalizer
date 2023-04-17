#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <WebServer.h>

// Create an instance of the TFT_eSPI library
TFT_eSPI tft = TFT_eSPI();

// Wifi setup
const char* ssid = "paotharit";
const char* password = "pppppppp";

// Web Server
int port = 80;

WebServer server(port);

TaskHandle_t serverTaskHandle;

void serverTask(void* pvParameters) {
  for (;;) {
    server.handleClient();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// Function to update TFT display
void updateDisplay(void* parameter) {
  uint16_t color = 0;

  for (;;) {
    // Clear the screen with a random color
    color = random(0xFFFF);
    tft.fillScreen(color);

    // Draw text
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(2);
    tft.setCursor(60, 100);
    tft.print("Hello, ESP32 & FreeRTOS!");

    // Delay for 3 seconds
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void setup() {
  Serial.begin(115200);
  // Initialize TFT display
  tft.init();
  tft.setRotation(3);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  server.on("/", []() {
    server.send(200, "text/html", "<html><body><h1>Hello, world!</h1></body></html>");
  });

  server.begin();
  Serial.println("Web server started");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  xTaskCreate(updateDisplay, "Update Display", 2048, NULL, 1, NULL);
  xTaskCreate(serverTask, "serverTask", 4096, NULL, 1, &serverTaskHandle);
}

void loop() {
  // Do nothing
}

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <arduinoFFT.h>
#include "webpage.h"
// #include "FFT.h"
// #include "Settings.h"
// #include "I2SPLUGIN.h"

// Potentiometers
#define NUM_POTENTIOMETERS 4
const int potPins[NUM_POTENTIOMETERS] = { 34, 35, 32 };
const String potNames[NUM_POTENTIOMETERS] = { "Treble", "Bass", "Volumn" };
const int changeThreshold = 50;

int numBands = 64;

struct DisplayData {
  int pwmValue;
  int potentiometerNumber;
};

QueueHandle_t displayQueue;

// Create an instance of the TFT_eSPI library
TFT_eSPI tft = TFT_eSPI();

// Wifi setup
const char* ssid = "paotharit";
const char* password = "pppppppp";

// Web Server
int port = 80;

WebServer server(port);
WebSocketsServer webSocket = WebSocketsServer(81);

TaskHandle_t serverTaskHandle;

// void SendData() {
//   String json = "[";
//   for (int i = 0; i < numBands; i++) {
//     if (i > 0) {
//       json += ", ";
//     }
//     json += "{\"bin\":";
//     json += "\"" + labels[i] + "\"";
//     json += ", \"value\":";
//     json += String(FreqBins[i]);
//     json += "}";
//   }
//   json += "]";
//   webSocket.broadcastTXT(json.c_str(), json.length());
// }

void serverTask(void* pvParameters) {
  for (;;) {
    server.handleClient();
    webSocket.loop();
    // SendData();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void updateDisplay(void* parameter) {
  // Initialize TFT display
  tft.begin();
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0, 0);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.println("Potentiometer");

  TickType_t lastUpdate = xTaskGetTickCount();

  for (;;) {
    DisplayData data;

    if (xQueueReceive(displayQueue, &data, portMAX_DELAY) == pdTRUE) {
      lastUpdate = xTaskGetTickCount();

      tft.fillRect(0, 0, 200, 40, TFT_BLACK);
      tft.setTextSize(2);
      tft.setCursor(0, 0);
      tft.print(potNames[data.potentiometerNumber]);
      tft.setCursor(0, 20);
      tft.print("Value: ");
      tft.print(data.pwmValue * 100 / 255);
      tft.print(" %");

      // Draw progress bar
      uint16_t barHeight = map(data.pwmValue, 0, 255, 0, tft.height() - 40);
      // x,y,width, height
      tft.fillRect(tft.width() - 50, 0, 50, tft.height() - 40, TFT_BLACK);
      tft.drawRect(tft.width() - 50, 0, 50, tft.height() - 40, TFT_WHITE);
      tft.fillRect(tft.width() - 50, tft.height() - barHeight - 40, 50, barHeight, ILI9341_GREEN);
    }

    // Check if the display needs to be updated
    if ((xTaskGetTickCount() - lastUpdate) >= pdMS_TO_TICKS(50)) {
      tft.fillRect(0, 0, 200, 40, ILI9341_BLACK);
      tft.setTextSize(2);
      tft.setCursor(0, 0);
      tft.print("Potentiometer");
      tft.setCursor(0, 20);
      tft.print("not updated");
      tft.fillRect(tft.width() - 20, 0, 20, tft.height(), ILI9341_BLACK);
      // lastUpdate = xTaskGetTickCount();
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void readPotentiometers(void* parameter) {
  int previousValues[NUM_POTENTIOMETERS] = { 0 };

  for (int i = 0; i < NUM_POTENTIOMETERS; i++) {
    previousValues[i] = analogRead(potPins[i]);
  }

  for (;;) {
    for (int i = 0; i < NUM_POTENTIOMETERS; i++) {
      int value = analogRead(potPins[i]);
      int pwmValue = map(value, 0, 4095, 0, 255);

      if (abs(value - previousValues[i]) > changeThreshold) {
        DisplayData data = { pwmValue, i };
        xQueueOverwrite(displayQueue, &data);
        previousValues[i] = value;
      }
    }

    // Delay to avoid excessive CPU usage
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}



void setup() {
  Serial.begin(115200);

  // Pinmode
  for (int i = 0; i < NUM_POTENTIOMETERS; i++) {
    pinMode(potPins[i], INPUT);
  }

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
    server.send(200, "text/html", webpage);
  });

  server.begin();
  Serial.println("Web server started");
  webSocket.begin();
  Serial.println("Web socket started");

  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  displayQueue = xQueueCreate(1, sizeof(DisplayData));
  xTaskCreate(readPotentiometers, "Read Potentiometers", 4096, NULL, 1, NULL);
  xTaskCreate(updateDisplay, "Update Display", 4096, NULL, 1, NULL);
  // xTaskCreate(serverTask, "serverTask", 4096, NULL, 1, &serverTaskHandle);
}

void loop() {
  // Do nothing
}

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <arduinoFFT.h>
#include "webpage.h"
#include <FastLED.h>
#include <JC_Button.h>
// #include "FFT.h"
// #include "Settings.h"
// #include "I2SPLUGIN.h"

// Potentiometers
#define NUM_POTENTIOMETERS 3
const int potPins[NUM_POTENTIOMETERS] = { 34, 35, 32 };
const String potNames[NUM_POTENTIOMETERS] = { "Treble", "Bass", "Volume" };
const int changeThreshold = 40;

// LED BAR
#define NUM_LEDS 8
#define LED_PIN 12
CRGB leds[NUM_LEDS];

// Button
#define BUTTON_PIN 33
Button button(BUTTON_PIN, 25, true, true);
int buttonState = 0;

// int numBands = 64;

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
  DisplayData displayData[NUM_POTENTIOMETERS];
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);

  for (;;) {
    if (!buttonState) {
      if (xQueueReceive(displayQueue, &displayData, portMAX_DELAY) == pdPASS) {
        for (int i = 0; i < NUM_POTENTIOMETERS; i++) {
          int xPos = i * 80;
          // Potentiometer Name
          tft.setCursor(xPos, 20);
          tft.print(potNames[displayData[i].potentiometerNumber]);

          // Potentiometer Value
          tft.setCursor(xPos, 40);
          char formattedValue[10];
          sprintf(formattedValue, "%3d %%", displayData[i].pwmValue * 100 / 255);
          tft.print(formattedValue);

          // Draw progress bar
          uint16_t barHeight = map(displayData[i].pwmValue, 0, 255, 0, tft.height() - 60);
          tft.fillRect(xPos, 60, 50, tft.height() - 60, TFT_BLACK);
          tft.fillRect(xPos, tft.height() - barHeight, 50, barHeight, ILI9341_GREEN);
        }
      }
    } else {
      tft.fillScreen(TFT_BLACK);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.setTextSize(2);
      tft.setCursor(0, 20);
      tft.print("Frequency");
    }
  }
}

void readPotentiometers(void* parameter) {
  int previousValues[NUM_POTENTIOMETERS] = { 0 };

  for (int i = 0; i < NUM_POTENTIOMETERS; i++) {
    previousValues[i] = analogRead(potPins[i]);
  }

  for (;;) {
    bool updateNeeded = false;
    DisplayData displayData[NUM_POTENTIOMETERS];

    for (int i = 0; i < NUM_POTENTIOMETERS; i++) {
      int value = analogRead(potPins[i]);
      int pwmValue = map(value, 0, 4095, 0, 255);

      if (abs(value - previousValues[i]) > changeThreshold) {
        displayData[i] = { pwmValue, i };
        previousValues[i] = value;
        updateNeeded = true;
        uint8_t ledCount = map(pwmValue, 0, 255, 0, NUM_LEDS);

        for (int j = 0; j < NUM_LEDS; j++) {
          leds[j] = (j < ledCount) ? CRGB::White : CRGB::Black;
        }

        FastLED.show();
      }
    }

    if (updateNeeded) {
      xQueueSend(displayQueue, &displayData, portMAX_DELAY);
    }

    // Delay to avoid excessive CPU usage
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void stateChange(void* parameter) {
  for (;;) {
    button.read();

    if (button.wasPressed()) {
      buttonState = !buttonState;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}


void setup() {
  Serial.begin(115200);

  // Pinmode
  for (int i = 0; i < NUM_POTENTIOMETERS; i++) {
    pinMode(potPins[i], INPUT);
  }
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(10);

  // Initialize TFT display
  tft.init();
  tft.setRotation(0);
  // tft.fillScreen(TFT_BLACK);

  // Button Setup
  button.begin();

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


  // Create the display data queue
  displayQueue = xQueueCreate(1, sizeof(DisplayData[NUM_POTENTIOMETERS]));

  // Create the read potentiometers task
  xTaskCreate(readPotentiometers, "ReadPotentiometers", 4096, NULL, 1, NULL);

  // Create the update display task
  xTaskCreate(updateDisplay, "UpdateDisplay", 4096, NULL, 2, NULL);

  // xTaskCreate(stateChange, "Change state button", 2048, NULL, 2, NULL);
  // xTaskCreate(updateLED, "UpdateLED", 4096, NULL, 2, NULL);

  // xTaskCreate(updateDisplay, "Update Display", 4096, NULL, 1, NULL);
  // xTaskCreate(serverTask, "serverTask", 4096, NULL, 1, &serverTaskHandle);
}

void loop() {
  // Do nothing
}

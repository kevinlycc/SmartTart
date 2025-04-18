#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_LSM6DSOX.h>
#include "pitches.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <ArduinoJson.h>

const char* ssid = "Michael";
const char* password = "Ipisbest";
//const char* serverName = "http://169.234.12.169:5000/stats";
const char* serverName = "http://192.168.53.72:5000/stats";
WebServer server(80);

Adafruit_LSM6DSOX sox;

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Buzzer
const int buzzerPin = 10;
bool preEndBeeped = false;

// Temp Sensor - ZTP-115M
const int sensorPin = 6;  // Analog pin
float voltage, tempC, tempF;

// Relay
const int relayPin = 5;
bool relayState = false;

// Pushbutton
const int Pushbutton = 17;
bool lastButtonState = HIGH;
bool currentButtonState;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;

// Custom variables
int steps = 0;
int toasts = 0;

// Uploading to server
unsigned long lastUploadTime = 0;
const unsigned long uploadInterval = 10000; // upload every 10 seconds

// Step timing
unsigned long lastStepTime = 0;
const unsigned long stepCooldown = 300; // Minimum time between steps (ms)

// Toast Countdown
bool toasting = false;
unsigned long toastStartTime = 0;
unsigned long toastDuration = 60000; // 60 seconds by default

// Jeopardy
int jeopardyMelody[] = {
  NOTE_A4, NOTE_F5, NOTE_C5, NOTE_A4,
  NOTE_C5, NOTE_F5, NOTE_C5,
  NOTE_C5, NOTE_F5, NOTE_C5, NOTE_F5,
  NOTE_AS5, NOTE_G5, NOTE_F5, NOTE_E5, NOTE_D5, NOTE_CS5,
  NOTE_C5, NOTE_F5, NOTE_C5, NOTE_A4,
  NOTE_C5, NOTE_F5, NOTE_C5,
  NOTE_AS5, 0, NOTE_G5, NOTE_F5,
  NOTE_E5, NOTE_D5, NOTE_CS5, 0,

  // 2nd loop
  NOTE_A4, NOTE_F5, NOTE_C5, NOTE_A4,
  NOTE_C5, NOTE_F5, NOTE_C5, 0,
  NOTE_C5, NOTE_F5, NOTE_C5, NOTE_F5,
  NOTE_AS5, NOTE_G5, NOTE_F5, NOTE_E5, NOTE_D5, NOTE_CS5,
  NOTE_C5, NOTE_F5, NOTE_C5, NOTE_A4,
  NOTE_C5, NOTE_F5, NOTE_C5,
  NOTE_AS5, 0, NOTE_G5, NOTE_F5,
  NOTE_E5, NOTE_D5, NOTE_CS5, 0, 

  // 3rd loop
  NOTE_A4, NOTE_F5, NOTE_C5, NOTE_A4,
  NOTE_C5, NOTE_F5, NOTE_C5,
  NOTE_C5, NOTE_F5, NOTE_C5, NOTE_F5,
  NOTE_AS5, NOTE_G5, NOTE_F5, NOTE_E5, NOTE_D5, NOTE_CS5,
  NOTE_C5, NOTE_F5, NOTE_C5, NOTE_A4,
  NOTE_C5, NOTE_F5, NOTE_C5,
  NOTE_AS5, 0, NOTE_G5, NOTE_F5,
  NOTE_E5, NOTE_D5, NOTE_CS5, 0,

  // 4th loop
  NOTE_A4, NOTE_F5, NOTE_C5, NOTE_A4,
  NOTE_C5, NOTE_F5, NOTE_C5, 0,
  NOTE_C5, NOTE_F5, NOTE_C5, NOTE_F5,
  NOTE_AS5, NOTE_G5, NOTE_F5, NOTE_E5, NOTE_D5, NOTE_CS5,
  NOTE_C5, NOTE_F5, NOTE_C5, NOTE_A4,
  NOTE_C5, NOTE_F5, NOTE_C5,
  NOTE_AS5, 0, NOTE_G5, NOTE_F5,
  NOTE_E5, NOTE_D5, NOTE_CS5, 0, 
};

int jeopardyDurations[] = {
  4,    4,    4,    4,
  4,    4,          2,
  4,    4,    4,    4,
  3,   8, 8, 8, 8, 8,
  4,    4,    4,    4,
  4,    4,          2,
  4, 8, 8,    4,    4,
  4,    4,    4,    4,

// 2nd loop
  4,    4,    4,    4,
  4,    4,          2,
  4,    4,    4,    4,
  3,   8, 8, 8, 8, 8,
  4,    4,    4,    4,
  4,    4,          2,
  4, 8, 8,    4,    4,
  4,    4,    4,    4,

  // 3rd loop
  4,    4,    4,    4,
  4,    4,          2,
  4,    4,    4,    4,
  3,   8, 8, 8, 8, 8,
  4,    4,    4,    4,
  4,    4,          2,
  4, 8, 8,    4,    4,
  4,    4,    4,    4,

  // 4th loop
  4,    4,    4,    4,
  4,    4,          2,
  4,    4,    4,    4,
  3,   8, 8, 8, 8, 8,
  4,    4,    4,    4,
  4,    4,          2,
  4, 8, 8,    4,    4,
  4,    4,    4,    4,
  
  0 // End
};

int jeoIndex = 0;
unsigned long jeoNoteStart = 0;
bool jeopardyDone = false;
int currentNoteDuration = 0;

// Function to start toasting process with specified duration
void startToasting(unsigned long duration) {
  // Only start if not already toasting
  if (!toasting) {
    relayState = true;
    digitalWrite(relayPin, HIGH);
    toasting = true;
    toastStartTime = millis();
    toastDuration = duration; // Use the provided duration
    Serial.print("Toasting started for ");
    Serial.print(duration / 1000);
    Serial.println(" seconds!");
    
    // Reset jeopardy tune variables
    preEndBeeped = false;
    jeoIndex = 0;
    jeoNoteStart = 0;
    jeopardyDone = false;
    currentNoteDuration = 0;
  } else {
    Serial.println("Toasting already in progress!");
  }
}

// Function to stop toasting process
void stopToasting() {
  if (toasting) {
    toasting = false;
    relayState = false;
    digitalWrite(relayPin, LOW);
    toasts++;
    Serial.println("Toasting completed!");
  }
}

void setup() {
  Wire.begin(8, 9);
  Serial.begin(115200);
  analogReadResolution(12);

  // BUZZER
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, LOW);

  // OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    while (true);
  }

  // Accelerometer - LSM6DSOX
  if (!sox.begin_I2C()) {
    Serial.println("Failed to find LSM6DSOX chip");
    while (1) delay(10);
  }

  // Initial display - Screen 1
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("SMART TART");
  display.setTextSize(1);
  display.setCursor(0, 35);
  display.print("STEPS: ");
  display.println(steps);
  display.setCursor(0, 50);
  display.print("TOASTS: ");
  display.println(toasts);
  display.display();
  delay(2000);

  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, relayState ? HIGH : LOW);

  pinMode(Pushbutton, INPUT_PULLUP);

  //WIFI
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("Connected to WiFi!");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  server.on("/set_profile", HTTP_POST, []() {
    if (server.hasArg("plain")) {
      String body = server.arg("plain");
      Serial.println("Received POST body: " + body);

      StaticJsonDocument<200> doc;
      DeserializationError error = deserializeJson(doc, body);

      if (!error) {
        int duration = doc["duration"];
        unsigned long durationMs = duration * 1000; // Convert to milliseconds
        Serial.print("Toast duration set to: ");
        Serial.print(duration);
        Serial.println(" seconds");
        
        // Call the startToasting function with the received duration
        startToasting(durationMs);
        
        server.send(200, "application/json", "{\"status\":\"toasting started\",\"duration\":" + String(duration) + "}");
      } else {
        Serial.println("Failed to parse JSON");
        server.send(400, "application/json", "{\"error\":\"Failed to parse JSON\"}");
      }
    } else {
      server.send(400, "application/json", "{\"error\":\"Missing profile\"}");
    }
  });

  server.begin();
  Serial.println("Server Started");
}

void loop() {
  // ---- TEMP SENSOR ----
  int adcValue = analogRead(sensorPin);
  voltage = adcValue * (3.3 / 4095.0);
  tempC = (voltage - 0.85) * 100 + 25;
  tempF = tempC * 9.0 / 5.0 + 32.0;

  // ---- ACCELEROMETER ----
  sensors_event_t accel, gyro, temp;
  sox.getEvent(&accel, &gyro, &temp);

  float ax = accel.acceleration.x;
  float ay = accel.acceleration.y;
  float az = accel.acceleration.z;

  // Compute magnitude and subtract gravity
  float magnitude = sqrt(ax * ax + ay * ay + az * az);
  float accel_no_gravity = abs(magnitude - 9.81); // Gravity-compensated acceleration

  // Smart step detection
  if (accel_no_gravity > 1.2 && millis() - lastStepTime > stepCooldown) {
    steps++;
    lastStepTime = millis();
    Serial.println("Step detected!");
  }

  // ---- BUTTON TOGGLE LOGIC ----
  int reading = digitalRead(Pushbutton);

  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != currentButtonState) {
      currentButtonState = reading;

      if (currentButtonState == LOW) {
        if (!toasting) {
          // Start a new toasting session with default duration
          startToasting(60000); // Default 60 seconds
        }
      }
    }
  }

  lastButtonState = reading;

  // ---- TOAST MANAGEMENT ----
  if (toasting && (millis() - toastStartTime >= toastDuration)) {
    stopToasting();
  }

  // ---- DISPLAY ----
  display.clearDisplay();

  if (toasting) {
    unsigned long timeElapsed = millis() - toastStartTime;
    unsigned long timeLeft = toastDuration / 1000 - (timeElapsed / 1000);
    
    // Beep when 1 second remains
    if (timeLeft == 1 && !preEndBeeped) {
      tone(buzzerPin, 1500, 200); // Higher pitch for ending
      preEndBeeped = true;
    }

  // Toasting display - Screen 2
    display.setTextSize(2);
    display.setCursor(0, 0);
    display.println("TOASTING..");
    display.setTextSize(1);
    display.setCursor(0, 30);
    display.print("TEMP: ");
    display.print(tempC, 1);
    display.println(" C");
    display.setCursor(0, 45);
    display.print("TIME LEFT: ");
    display.println(timeLeft);
    display.display();

    // ---- Play Jeopardy during toasting ----
    if (!jeopardyDone && jeoIndex < sizeof(jeopardyMelody) / sizeof(jeopardyMelody[0])) {
      unsigned long elapsed = millis() - toastStartTime;

      if (elapsed - jeoNoteStart >= currentNoteDuration) {
        int tempoFactor = 2; // <- slower tempo here
        currentNoteDuration = (1000 / jeopardyDurations[jeoIndex]) * tempoFactor;
        tone(buzzerPin, jeopardyMelody[jeoIndex], currentNoteDuration);
        jeoNoteStart = elapsed;
        jeoIndex++;
      }
    } else if (!jeopardyDone) {
      noTone(buzzerPin);
      jeopardyDone = true;
    }
  } else {
    display.setTextSize(2);
    display.setCursor(0, 0);
    display.println("SMART TART");
    display.setTextSize(1);
    display.setCursor(0, 35);
    display.print("STEPS: ");
    display.println(steps);
    display.setCursor(0, 50);
    display.print("TOASTS: ");
    display.println(toasts);
    display.display();
  }

  //////////WIFI///////////////////
  server.handleClient();
  if (WiFi.status() == WL_CONNECTED) {
    unsigned long currentMillis = millis();

    if (currentMillis - lastUploadTime >= uploadInterval) {
      lastUploadTime = currentMillis;

      HTTPClient http;
      http.begin(serverName);
      http.addHeader("Content-Type", "application/json");

      String jsonPayload = "{";
      jsonPayload += "\"tarts\": " + String(toasts) + ",";
      jsonPayload += "\"steps\": " + String(steps) + ",";
      jsonPayload += "\"current_temp\": " + String(tempC);
      jsonPayload += "}";    

      int httpResponseCode = http.POST(jsonPayload);

      if (httpResponseCode > 0) {
        String response = http.getString();
        Serial.println("Server response: " + response);
      } else {
        Serial.print("Error code: ");
        Serial.println(httpResponseCode);
      }

      http.end();
    }
  } else {
    Serial.println("WiFi not connected");
  }

  delay(100);
}
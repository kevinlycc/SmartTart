#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_LSM6DSOX.h>
#include "pitches.h"
#include <WiFi.h>
#include <HTTPClient.h>

const char* ssid = "Michael";
const char* password = "Ipisbest";
const char* serverName = "http://169.234.12.169:5000/test_send";

Adafruit_LSM6DSOX sox;

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Buzzer
const int buzzerPin = 10;
bool preEndBeeped = false;

// ZTP-115M
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

// Step timing
unsigned long lastStepTime = 0;
const unsigned long stepCooldown = 300; // Minimum time between steps (ms)

// Toast Countdown
bool toasting = false;
unsigned long toastStartTime = 0;
const unsigned long toastDuration = 30000; // 30 seconds

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
NOTE_A4, NOTE_F5, NOTE_C5, NOTE_A4,
  NOTE_C5, NOTE_F5, NOTE_C5,
  NOTE_C5, NOTE_F5, NOTE_C5, NOTE_F5,
  NOTE_AS5, NOTE_G5, NOTE_F5, NOTE_E5, NOTE_D5, NOTE_CS5,
  NOTE_C5, NOTE_F5, NOTE_C5, NOTE_A4,
  NOTE_C5, NOTE_F5, NOTE_C5,
  NOTE_AS5, 0, NOTE_G5, NOTE_F5,
  NOTE_E5, NOTE_D5, NOTE_CS5, NOTE_C5
};


int jeopardyDurations[] = {
  4,    4,    4,    4,
  4,    4,          2,
  4,    4,    4,    4,
  3,   8, 8, 8, 8, 8,
  4,    4,    4,    4, // the same again
  4,    4,          2,
  4, 8, 8,    4,    4,
  4,    4,    4,    4,
4,    4,    4,    4,
  4,    4,          2,
  4,    4,    4,    4,
  3,   8, 8, 8, 8, 8,
  4,    4,    4,    4, // the same again
  4,    4,          2,
  4, 8, 8,    4,    4,
  4,    4,    4,    4,
  0};

int jeoIndex = 0;
unsigned long jeoNoteStart = 0;
bool jeopardyDone = false;
int currentNoteDuration = 0;

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

  // LSM6DSOX
  if (!sox.begin_I2C()) {
    Serial.println("Failed to find LSM6DSOX chip");
    while (1) delay(10);
  }

  // Initial display
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

  // ---- SERIAL DEBUG ----
  Serial.print("Accel Mag: ");
  Serial.print(magnitude, 2);
  Serial.print(" | w/o Gravity: ");
  Serial.print(accel_no_gravity, 2);
  Serial.print(" | Temp: ");
  Serial.print(tempC, 1);
  Serial.print(" °C / ");
  Serial.print(tempF, 1);
  Serial.println(" °F");

  // ---- BUTTON TOGGLE LOGIC ----
  int reading = digitalRead(Pushbutton);

  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != currentButtonState) {
      currentButtonState = reading;

      if (currentButtonState == LOW) {
        if (!relayState && !toasting) {
          // Start a new toasting session
          relayState = true;
          digitalWrite(relayPin, HIGH);
          toasting = true;
          toastStartTime = millis();
          Serial.println("Toasting started!");
          preEndBeeped = false;
          jeoIndex = 0;
          jeoNoteStart = 0;
          jeopardyDone = false;
          currentNoteDuration = 0;          
        }
      }
    }
  }

  lastButtonState = reading;

  // ---- DISPLAY ----
  display.clearDisplay();

  if (toasting) {
    unsigned long timeLeft = 30 - ((millis() - toastStartTime) / 1000);
    
    // Beep when 1 second remains
  if (timeLeft == 1 && !preEndBeeped) {
    tone(buzzerPin, 1500, 200); // Higher pitch for ending
    preEndBeeped = true;
  }
    display.setTextSize(2);
    display.setCursor(0, 0);
    display.println("TOASTING...");
    display.setTextSize(1);
    display.setCursor(0, 30);
    display.print("TEMP: ");
    display.print(tempC, 1);
    display.println(" C");
    display.setCursor(0, 45);
    display.print("TIME LEFT: ");
    display.println(timeLeft);
    display.display();

     // ---- Play Für Elise during toasting ----
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

    if (millis() - toastStartTime >= toastDuration) {
      toasting = false;
      relayState = false;
      digitalWrite(relayPin, LOW);
      toasts++;
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

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverName);
    http.addHeader("Content-Type", "application/json");

    String jsonPayload = "{";
    jsonPayload += "\"tarts\": " + String(toasts) + ",";
    jsonPayload += "\"steps\": " + String(steps);
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
  } else {
    Serial.println("WiFi not connected");
  }

  delay(100);
}

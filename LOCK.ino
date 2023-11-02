#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <FirebaseESP8266.h>
#include <ESP8266WiFi.h>
#include "connection.h"
#include <Keypad.h>
#include <ArduinoJson.h>

FirebaseData firebaseData;

#define RELAY_PIN D0
#define BUZZER_PIN D8

bool lock;
unsigned long buzzerStartTime = 0;
const unsigned long BUZZER_DURATION = 3000;  // 3 seconds in milliseconds

const int PIN_LENGTH = 4;  // The length of the PIN
char PIN[PIN_LENGTH + 1];  // +1 for the null terminator

const int MANUAL_LENGTH = 4;  // The length of the PIN
char MANUAL[PIN_LENGTH + 1];  // +1 for the null terminator


const byte ROWS = 4;  // Four rows
const byte COLS = 4;  // Four columns
char keys[ROWS][COLS] = {
  { '1', '4', '7', '*' },
  { '2', '5', '8', '0' },
  { '3', '6', '9', '#' },
  { 'A', 'B', 'C', 'D' }
};

byte colPins[ROWS] = { D8, D1, D2, D3 };
byte rowPins[COLS] = { D4, D5, D6, D7 };

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

void setup() {
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);  // Initialize the buzzer pin
  Serial.begin(9600);




  Serial.println("Initializing...");
  connectToWiFi();
  Serial.println("Connected to WiFi");
  connectToFirebase();
  Serial.println("Connected to Firebase");

  // Set up the Firebase node listener
  Firebase.beginStream(firebaseData, "/lock/status/lock");
  Serial.println("Listening to Firebase stream");

  // Register the callback function for the node
  Firebase.setStreamCallback(firebaseData, onDataChange);
  Serial.println("Callback registered");

  Serial.println("Enter the PIN:");
}

void loop() {
  char key = keypad.getKey();

  if (strcmp(MANUAL, "AAAA") == 0) {
    // Manual mode, use the keypad for PIN entry
    Serial.println("Manual Mode");
    if (key) {
      if (strlen(PIN) < PIN_LENGTH) {
        // Append the entered digit to the PIN
        PIN[strlen(PIN)] = key;
        Serial.print(key);
      }

      if (strlen(PIN) == PIN_LENGTH) {
        // If a 4-digit PIN is entered, check it against Firebase
        checkPIN();
        // Reset PIN after checking
        memset(PIN, 0, sizeof(PIN));
      }
    }
  } else {
    // Normal mode, check for "AAAA" condition
    Serial.println("Auto Mode");
    if (key) {
      if (strlen(MANUAL) < MANUAL_LENGTH) {
        // Append the entered digit to the MANUAL
        MANUAL[strlen(MANUAL)] = key;
        Serial.print(key);
      }

      // Check for "AAAA" in Auto Mode
      if (strcmp(MANUAL, "AAAA") == 0) {
        // Switch to Manual Mode
        Serial.println("Switching to Manual Mode");
      } 
    }
  }
}

void onDataChange(StreamData data) {
  if (data.dataType() == "boolean") {
    bool value = data.boolData();

    // Check if the value has changed
    if (value != lock) {
      lock = value;                                 // Update the lock status
      digitalWrite(RELAY_PIN, value ? HIGH : LOW);  // Control the relay pin
      digitalWrite(BUZZER_PIN, HIGH);
      unsigned long buzz = millis();
      while (millis() - buzz < 1000) {
      }
      digitalWrite(BUZZER_PIN, LOW);

      Serial.println("Lock status changed to: " + String(value ? "ON" : "OFF"));
    }
  } else {
    Serial.println("Invalid data type received");
  }
}

void checkPIN() {
  // Retrieve the data from Firebase
  if (Firebase.getString(firebaseData, "/pin/")) {
    String dataValue = firebaseData.stringData();
    Serial.print("Data received: ");
    Serial.println(dataValue);

    // Calculate the capacity based on the expected size of your JSON data
    // You should adjust this value based on your actual data size.
    const size_t capacity = 500;  // Adjust as needed

    StaticJsonDocument<500> doc;
    DeserializationError error = deserializeJson(doc, dataValue);

    if (!error) {
      // Create an array to store PIN values
      int pinValues[5];
      int index = 0;

      for (JsonPair pair : doc.as<JsonObject>()) {
        int pinValue = pair.value()["pin"];
        pinValues[index] = pinValue;
        index++;
      }

      // Compare the inputted PIN with the extracted PIN values
      int inputPin = atoi(PIN);  // Convert the inputted PIN to an integer

      bool accessGranted = false;
      for (int i = 0; i < 5; i++) {
        if (inputPin == pinValues[i]) {
          accessGranted = true;
          break;  // Exit the loop if a match is found
        }
      }

      if (accessGranted) {
        Serial.println("Access granted");
        // Perform the action for granting access, e.g., unlock the door.
        digitalWrite(RELAY_PIN, LOW);  // Turn on the relay to unlock the door

        // Update the lock status to true
        Firebase.setBool(firebaseData, "/lock/status/lock", false);

        // Beep the buzzer twice for access granted
        for (int i = 0; i < 2; i++) {
          digitalWrite(BUZZER_PIN, HIGH);
          delay(500); // Beep for 0.5 seconds
          digitalWrite(BUZZER_PIN, LOW);
          delay(500); // Wait for 0.5 seconds
        }

        // Delay for 3 seconds (3000 milliseconds)
        delay(3000);

        // Restart the Arduino sketch (soft reset)
        ESP.restart();
      } else {
        Serial.println("Access denied");
        digitalWrite(RELAY_PIN, HIGH);  // Turn on the relay to lock the door
        // Update the lock status to false
        Firebase.setBool(firebaseData, "/lock/status/lock", true);

        // Beep the buzzer for 3 seconds if denied
        unsigned long startTime = millis();
        while (millis() - startTime < 2000) {
          digitalWrite(BUZZER_PIN, HIGH);
        }

        // Restart the Arduino sketch (soft reset)
        ESP.restart();
      }
    } else {
      Serial.println("Failed to parse JSON data");
    }
  }
}






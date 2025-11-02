#include <MPU9250_WE.h>
#include <Wire.h>
#define MPU9250_ADDR 0x53
#define RAIN_SENSOR_PIN 15  // rain_s
#define S2_PIN 14           // s2
#define S3_PIN 13           // s3
#define S4_PIN 12           // s4
#define WATER_FLOW_PIN 2    // water
#define RESET_PIN 25        // rst
#include <WiFi.h>
#include <HTTPClient.h>

const char* ssid = "Weaverbase";
const char* password = "ujicqw5aci";
const char* serverUrl = "http://192.168.1.248:5000/api/data";  // IP of Jetson
String payloadJson = "";
int relayState[3] = { 0, 0, 0 };
// int t = 10; //sec (Unused variable removed)
int counter_i = 0;
MPU9250_WE myMPU9250 = MPU9250_WE(MPU9250_ADDR);
int vibrationStatus = 0;
const float VIBRATION_THRESHOLD = 20000.0;
xyzFloat prevAccRaw;
int flowState = 1;  // 1 = Normal/Flowing, 0 = Blocked/No Flow

// Non-Blocking Timing Variables
unsigned long previousMillis = 0;
const long interval = 5000;  // Send data every 5 seconds (5000ms)

// Rain Detection Variables
int rainState = 0;  // 0=Normal, 1=Continuous Rain
const int RAIN_THRESHOLD = 4000;
const long RAIN_DURATION_MS = 1000;   // 10 seconds duration check
unsigned long lowValueStartTime = 0;  // Timestamp when value first dropped below threshold

// ****************************************************
/**
 * Checks for significant changes in accelerometer data to detect vibration.
 * @param vibrationStatus Reference to the global vibration status (1 if detected).
 * @param previousAccRaw Reference to the last read raw acceleration values.
 * @param threshold The sensitivity threshold for detection.
 */
void checkVibration(int& vibrationStatus, xyzFloat& previousAccRaw, float threshold) {
  xyzFloat accRaw = myMPU9250.getAccRawValues();
  float deltaX = abs(accRaw.x - previousAccRaw.x);
  float deltaY = abs(accRaw.y - previousAccRaw.y);
  float deltaZ = abs(accRaw.z - previousAccRaw.z);
  float totalDelta = deltaX + deltaY + deltaZ;

  if (totalDelta > threshold) {
    vibrationStatus = 1;
  }

  previousAccRaw = accRaw;

  Serial.print(" | Vibration Status: ");
  Serial.print(vibrationStatus);
  Serial.print(" | ");
}

/**
 * Checks if the rain sensor value has been continuously below the threshold 
 * for the required duration (RAIN_DURATION_MS).
 * @param rainValue The raw analog reading from the rain sensor.
 */
void checkRain(int rainValue) {
  if (rainValue < RAIN_THRESHOLD) {
    // 1. Value is below threshold
    if (lowValueStartTime == 0) {
      // 2. Start timer if it's the first time below threshold
      lowValueStartTime = millis();
    }
    // 3. Check if the duration requirement is met (10 seconds)
    if (millis() - lowValueStartTime >= RAIN_DURATION_MS) {
      rainState = 1;  // Condition met: Set status to 1
    }
  } else {
    // 4. Value is back above threshold: Reset status
    lowValueStartTime = 0;  // Reset timer
                            //rainState = 0;          // Reset rain state
  }
  // Display results
  Serial.print("Rain Value: ");
  Serial.print(rainValue);
  Serial.print(" | Rain State: ");
  Serial.println(rainState);
}

/**
 * Sets the digital state of the S2, S3, S4 pins based on the input array.
 */
int setRelayState(int stateArray[]) {
  digitalWrite(S2_PIN, stateArray[0]);
  digitalWrite(S3_PIN, stateArray[1]);
  digitalWrite(S4_PIN, stateArray[2]);
  return 0;
}

void setup() {
  Serial.begin(115200);
  Wire.begin();
  pinMode(RAIN_SENSOR_PIN, INPUT_PULLUP);
  pinMode(S2_PIN, OUTPUT);
  pinMode(S3_PIN, OUTPUT);
  pinMode(S4_PIN, OUTPUT);
  pinMode(WATER_FLOW_PIN, INPUT_PULLUP);
  pinMode(RESET_PIN, INPUT_PULLUP);

  relayState[0] = 0;
  relayState[1] = 0;
  relayState[2] = 0;
  setRelayState(relayState);

  // MPU9250 Setup
  if (!myMPU9250.init()) {
    Serial.println("MPU9250 does not respond");
  } else {
    Serial.println("MPU9250 is connected");
  }
  Serial.println("Position MPU9250 flat and don't move it - calibrating...");
  delay(1000);
  myMPU9250.autoOffsets();
  Serial.println("Done!");
  myMPU9250.setSampleRateDivider(5);
  myMPU9250.setAccRange(MPU9250_ACC_RANGE_2G);
  myMPU9250.enableAccDLPF(true);
  myMPU9250.setAccDLPF(MPU9250_DLPF_6);
  prevAccRaw = myMPU9250.getAccRawValues();

  // WiFi Setup
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi");

  // Initial Relay Sequence
  relayState[0] = 0;
  relayState[1] = 0;
  relayState[2] = 1;
  setRelayState(relayState);
  delay(1000);
  relayState[0] = 0;
  relayState[1] = 0;
  relayState[2] = 0;
  setRelayState(relayState);
}

void loop() {
  // --- NON-BLOCKING SENSOR READING AND RELAY CONTROL ---

  // Read Flow Sensor (INPUT_PULLUP, 0 means blocked/no flow)
  if (digitalRead(WATER_FLOW_PIN) == 0) {
    flowState = 0;
  } else {
    flowState = 1;
  }

  // Read Rain Sensor
  int rainValue = analogRead(RAIN_SENSOR_PIN);

  // Check Sensors
  checkVibration(vibrationStatus, prevAccRaw, VIBRATION_THRESHOLD);
  checkRain(rainValue);

  // --- State Machine Logic ---
  if (digitalRead(RESET_PIN) == 0) {  // Reset button is pressed (Active Low)
    relayState[0] = 0;
    relayState[1] = 0;
    relayState[2] = 0;
    payloadJson = "{\"rainfall1\": " + String(0) + ",\"rainfall2\": " + String(0) + ", \"vibrations1\": " + String(0) + ", \"vibrations2\": " + String(0) + ", \"state_node1\": " + String(0) + ", \"state_node2\": " + String(0) + "}";
    counter_i = 0;
    rainState = 0;
    vibrationStatus = 0;
    flowState = 1;
    lowValueStartTime = 0;  // Ensure rain timer is reset
  } else if (rainState == 1 && vibrationStatus == 1 && flowState == 0) {
    // Condition 1: Rain, Vibration, No Flow
    relayState[0] = 1;  // S2 ON
    relayState[1] = 0;  // S3 OFF
    relayState[2] = 0;  // S4 OFF
    payloadJson = "{\"rainfall1\": " + String(31.69) + ",\"rainfall2\": " + String(44.43) + ", \"vibrations1\": " + String(4.78) + ", \"vibrations2\": " + String(2.1) + ", \"state_node2\": " + String(1) + ", \"state_node1\": " + String(1) + "}";

  } else if (rainState == 1 && vibrationStatus == 1) {
    // Condition 2: Rain, Vibration (but flow is normal or status not checked/relevant)
    relayState[0] = 0;  // S2 OFF
    relayState[1] = 1;  // S3 ON
    relayState[2] = 1;  // S4 ON

  } else if (rainState == 1) {
    // Condition 3: Rain only
    relayState[0] = 0;  // S2 OFF
    relayState[1] = 0;  // S3 OFF
    relayState[2] = 1;  // S4 ON
    counter_i++;
    payloadJson = "{\"rainfall1\": " + String(31.69) + ",\"rainfall2\": " + String(44.43) + ", \"vibrations1\": " + String(4.78) + ", \"vibrations2\": " + String(2.1) + ", \"state_node1\": " + String(1) + ", \"state_node2\": " + String(0) + "}";
    if (counter_i >= 2000) {
      relayState[0] = 0;  // S2 OFF
      relayState[1] = 1;  // S3 ON
      relayState[2] = 0;  // S4 OFF
    }
  }
  // else {
  //     // Condition 4: Normal/Clear
  //     relayState[0] = 0;
  //     relayState[1] = 0;
  //     relayState[2] = 0;
  //     payloadJson = "{\"rainfall1\": " + String(0) +
  //                   ",\"rainfall2\": " + String(0) +
  //                   ", \"vibrations1\": " + String(0) +
  //                   ", \"vibrations2\": " + String(0) +
  //                   ", \"state_node1\": " + String(0) +
  //                   ", \"state_node2\": " + String(0) + "}";
  //     counter_i = 0; // Reset counter when state is normal
  // }

  setRelayState(relayState);

  // Reset momentary vibration status after state logic is processed
  // if (vibrationStatus == 1) {
  //     vibrationStatus = 0;
  // }

  // --- NON-BLOCKING HTTP POST ---
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;  // Update last send time

    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      http.begin(serverUrl);
      http.addHeader("Content-Type", "application/json");

      int httpResponseCode = http.POST(payloadJson);
      Serial.println("Sent: " + payloadJson);
      Serial.println("Response: " + String(httpResponseCode));

      http.end();
    }
  }
  // No delay() needed here, loop runs continuously
}
192.168.1.248
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include <WiFi.h>
#include <PubSubClient.h>

// Wi-Fi credentials
const char* ssid = "SLT";
const char* password = "0112653435";

// MQTT Broker details
const char* mqtt_server = "broker.emqx.io";
const int mqtt_port = 1883;
const char* mqtt_user = "emqx";
const char* mqtt_password = "public";

WiFiClient espClient;
PubSubClient client(espClient);

// Constants
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define FALL_THRESHOLD 15.0
#define STEP_THRESHOLD 0.5
#define LED_PIN 13

#define WALK_THRESHOLD 1.0
#define JOG_THRESHOLD 5.0
#define RUN_THRESHOLD 10.0

// Global variables
Adafruit_MPU6050 mpu;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
int stepCount = 0;
bool fallDetected = false;
String movementType = "Unknown";

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("UniqueClientID123", mqtt_user, mqtt_password)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" trying again in 5 seconds");
      delay(5000);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  // Empty callback function
}

void setup(void) {
  Serial.begin(115200);

  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  if (!mpu.begin()) {
    Serial.println("Failed to find MPU6050 chip");
    while (1) {
      delay(10);
    }
  }

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    while (1);
  }

  display.display();
  delay(2000);
  display.clearDisplay();

  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_5_HZ);

  Serial.println("Setup done");
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  float accelMagnitude = sqrt(
    a.acceleration.x * a.acceleration.x +
    a.acceleration.y * a.acceleration.y +
    a.acceleration.z * a.acceleration.z
  );

  // Fall detection logic
  if (accelMagnitude > FALL_THRESHOLD) {
    fallDetected = true;
    digitalWrite(LED_PIN, HIGH);
  } else {
    fallDetected = false;
    digitalWrite(LED_PIN, LOW);
  }

  // Step counting
  if (abs(g.gyro.z) > STEP_THRESHOLD) {
    stepCount++;
  }

  // Movement classification
  if (accelMagnitude < WALK_THRESHOLD) {
    movementType = "Stationary";
  } else if (accelMagnitude < JOG_THRESHOLD) {
    movementType = "Walking";
  } else if (accelMagnitude < RUN_THRESHOLD) {
    movementType = "Jogging";
  } else {
    movementType = "Running";
  }

  // Display on OLED
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("Steps: ");
  display.print(stepCount);
  display.setCursor(0, 10);
  display.print("Fall: ");
  display.print(fallDetected ? "Yes" : "No");
  display.setCursor(0, 20);
  display.print("Movement: ");
  display.print(movementType);
  display.setCursor(0, 30);
  display.print("Temp: ");
  display.print(temp.temperature);
  display.println(" C");
  display.display();

  // Prepare data for MQTT
  char stepCountStr[10];
  itoa(stepCount, stepCountStr, 10);

  const char* fallStatus = fallDetected ? "Yes" : "No";
  const char* movementTypeStr = movementType.c_str();

  char tempStr[10];
  dtostrf(temp.temperature, 4, 2, tempStr);

  // Publish data to MQTT topics
  client.publish("iot/steps", stepCountStr);
  client.publish("iot/fall", fallStatus);
  client.publish("iot/movement", movementTypeStr);
  client.publish("iot/temperature", tempStr);

  delay(500);
}

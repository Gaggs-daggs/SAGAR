#include <Arduino.h>
#include <Wire.h>
#include <DHT.h>
#include <Adafruit_BME280.h>

#define DHT_PIN 33
#define TURBIDITY_PIN 34
#define TRIG_PIN 25
#define ECHO_PIN 26
#define SDA_PIN 21
#define SCL_PIN 22
#define DHT_TYPE DHT22

// Set to 1 only after adding a separate resistor divider to HC-SR04 ECHO.
#define ENABLE_ULTRASONIC 0

const unsigned long SENSOR_INTERVAL_MS = 2000;
const float ADC_REFERENCE_VOLTAGE = 3.3f;

DHT dht(DHT_PIN, DHT_TYPE);
Adafruit_BME280 bme;
bool bmeAvailable = false;
unsigned long lastReading = 0;

void setupSensors() {
  dht.begin();
  Wire.begin(SDA_PIN, SCL_PIN);
  bmeAvailable = bme.begin(0x76) || bme.begin(0x77);
  pinMode(TURBIDITY_PIN, INPUT);
#if ENABLE_ULTRASONIC
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);
#endif
}

void readDHT22(float &temperature, float &humidity) {
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();
}

void readBMP280(float &pressure) {
  pressure = bmeAvailable ? bme.readPressure() / 100.0f : NAN;
}

void readTurbidity(int &raw, float &voltage) {
  raw = analogRead(TURBIDITY_PIN);
  voltage = (raw / 4095.0f) * ADC_REFERENCE_VOLTAGE;
}

float readUltrasonic() {
#if ENABLE_ULTRASONIC
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  unsigned long duration = pulseIn(ECHO_PIN, HIGH, 30000UL);
  if (duration == 0) return NAN;
  return duration * 0.0343f / 2.0f;
#else
  return NAN;
#endif
}

String jsonNumber(float value, int decimals) {
  return isnan(value) ? "null" : String(value, decimals);
}

String createJSON(float temperature, float humidity, float pressure, int turbidityADC,
                  float turbidityVoltage, float distance) {
  String json = "{";
  json += "\"temperature\":" + jsonNumber(temperature, 1);
  json += ",\"humidity\":" + jsonNumber(humidity, 1);
  json += ",\"pressure\":" + jsonNumber(pressure, 1);
  json += ",\"turbidity_adc\":" + String(turbidityADC);
  json += ",\"turbidity_voltage\":" + jsonNumber(turbidityVoltage, 3);
  json += ",\"distance\":" + jsonNumber(distance, 1);
  json += "}";
  return json;
}

void sendSensorData() {
  float temperature;
  float humidity;
  float pressure;
  int turbidityADC;
  float turbidityVoltage;
  readDHT22(temperature, humidity);
  readBMP280(pressure);
  readTurbidity(turbidityADC, turbidityVoltage);
  float distance = readUltrasonic();
  // NTU requires calibration against known standards; this is raw ADC demonstration data.
  Serial.println(createJSON(temperature, humidity, pressure, turbidityADC,
                            turbidityVoltage, distance));
}

void setup() {
  Serial.begin(115200);
  setupSensors();
}

void loop() {
  unsigned long now = millis();
  if (now - lastReading >= SENSOR_INTERVAL_MS) {
    lastReading = now;
    sendSensorData();
  }
}
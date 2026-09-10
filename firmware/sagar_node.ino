#include <Arduino.h>
#include <Wire.h>
#include <DHT.h>
#include <Adafruit_BMP280.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <TinyGPSPlus.h>

#define DHT_PIN 33
#define TURBIDITY_PIN 34
#define TRIG_PIN 25
#define ECHO_PIN 26
#define SDA_PIN 21
#define SCL_PIN 22
#define DS18B20_PIN 27
#define DHT_TYPE DHT22
#define GPS_RX_PIN 16
#define GPS_TX_PIN 17
#define MPU_ADDRESS 0x68
#define MPU_SDA_PIN 32
#define MPU_SCL_PIN 23

// HC-SR04 ECHO is connected through a separate resistor divider.
#define ENABLE_ULTRASONIC 1

const unsigned long SENSOR_INTERVAL_MS = 2000;
const float ADC_REFERENCE_VOLTAGE = 3.3f;

DHT dht(DHT_PIN, DHT_TYPE);
Adafruit_BMP280 bmp;
TwoWire mpuWire = TwoWire(1);
OneWire oneWire(DS18B20_PIN);
DallasTemperature ds18b20(&oneWire);
TinyGPSPlus gps;
HardwareSerial gpsSerial(2);
bool bmpAvailable = false;
bool mpuAvailable = false;
uint8_t mpuWhoAmI = 0;
unsigned long lastReading = 0;

int16_t readMPUWord(byte highRegister) {
  mpuWire.beginTransmission(MPU_ADDRESS);
  mpuWire.write(highRegister);
  if (mpuWire.endTransmission(false) != 0 || mpuWire.requestFrom(MPU_ADDRESS, 2) != 2) {
    return 0;
  }
  return (int16_t)((mpuWire.read() << 8) | mpuWire.read());
}

bool initializeMPU() {
  for (int attempt = 0; attempt < 3; attempt++) {
    mpuWire.beginTransmission(MPU_ADDRESS);
    mpuWire.write(0x75);
    if (mpuWire.endTransmission(false) == 0 && mpuWire.requestFrom(MPU_ADDRESS, 1) == 1) {
      mpuWhoAmI = mpuWire.read();
      if (mpuWhoAmI == 0x70 || mpuWhoAmI == 0x68) {
        mpuWire.beginTransmission(MPU_ADDRESS);
        mpuWire.write(0x6B);
        mpuWire.write(0x00);
        if (mpuWire.endTransmission() == 0) {
          mpuWire.beginTransmission(MPU_ADDRESS);
          mpuWire.write(0x1C);
          mpuWire.write(0x10);
          mpuWire.endTransmission();
          mpuWire.beginTransmission(MPU_ADDRESS);
          mpuWire.write(0x1B);
          mpuWire.write(0x08);
          mpuWire.endTransmission();
          return true;
        }
      }
    }
    delay(20);
  }
  return false;
}

void setupSensors() {
  mpuWire.begin(MPU_SDA_PIN, MPU_SCL_PIN);
  mpuWire.setClock(100000);
  delay(1000);
  mpuAvailable = initializeMPU();
  dht.begin();
  ds18b20.begin();
  gpsSerial.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  Wire.begin(SDA_PIN, SCL_PIN);
  bmpAvailable = bmp.begin(0x76) || bmp.begin(0x77);
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
  pressure = bmpAvailable ? bmp.readPressure() / 100.0f : NAN;
}

float readDS18B20() {
  ds18b20.requestTemperatures();
  float temperature = ds18b20.getTempCByIndex(0);
  return temperature == DEVICE_DISCONNECTED_C ? NAN : temperature;
}

void readMPU6050(float &accelX, float &accelY, float &accelZ,
                 float &gyroX, float &gyroY, float &gyroZ) {
  if (!mpuAvailable) {
    mpuAvailable = initializeMPU();
  }
  if (!mpuAvailable) {
    accelX = accelY = accelZ = gyroX = gyroY = gyroZ = NAN;
    return;
  }
  int16_t rawAccelX = readMPUWord(0x3B);
  int16_t rawAccelY = readMPUWord(0x3D);
  int16_t rawAccelZ = readMPUWord(0x3F);
  int16_t rawGyroX = readMPUWord(0x43);
  int16_t rawGyroY = readMPUWord(0x45);
  int16_t rawGyroZ = readMPUWord(0x47);
  accelX = rawAccelX / 4096.0f * 9.80665f;
  accelY = rawAccelY / 4096.0f * 9.80665f;
  accelZ = rawAccelZ / 4096.0f * 9.80665f;
  gyroX = rawGyroX / 65.5f * DEG_TO_RAD;
  gyroY = rawGyroY / 65.5f * DEG_TO_RAD;
  gyroZ = rawGyroZ / 65.5f * DEG_TO_RAD;
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


void updateGPS() {
  while (gpsSerial.available()) {
    gps.encode(gpsSerial.read());
  }
}

String jsonNumber(float value, int decimals) {
  return isnan(value) ? "null" : String(value, decimals);
}

String jsonUnsignedLong(unsigned long value, bool valid) {
  return valid ? String(value) : "null";
}

String createJSON(float temperature, float humidity, float pressure, int turbidityADC,
                  float turbidityVoltage, float distance, float probeTemperature,
                  float accelX, float accelY, float accelZ,
                  float gyroX, float gyroY, float gyroZ) {
  String json = "{";
  json += "\"temperature\":" + jsonNumber(temperature, 1);
  json += ",\"humidity\":" + jsonNumber(humidity, 1);
  json += ",\"pressure\":" + jsonNumber(pressure, 1);
  json += ",\"turbidity_adc\":" + String(turbidityADC);
  json += ",\"turbidity_voltage\":" + jsonNumber(turbidityVoltage, 3);
  json += ",\"distance\":" + jsonNumber(distance, 1);
  json += ",\"probe_temperature\":" + jsonNumber(probeTemperature, 1);
  json += ",\"mpu6050_accel_x\":" + jsonNumber(accelX, 3);
  json += ",\"mpu6050_accel_y\":" + jsonNumber(accelY, 3);
  json += ",\"mpu6050_accel_z\":" + jsonNumber(accelZ, 3);
  json += ",\"mpu6050_gyro_x\":" + jsonNumber(gyroX, 3);
  json += ",\"mpu6050_gyro_y\":" + jsonNumber(gyroY, 3);
  json += ",\"mpu6050_gyro_z\":" + jsonNumber(gyroZ, 3);
  json += ",\"mpu_available\":" + String(mpuAvailable ? "true" : "false");
  json += ",\"mpu_who_am_i\":" + String(mpuWhoAmI);
  json += ",\"gps_valid\":" + String(gps.location.isValid() ? "true" : "false");
  json += ",\"gps_latitude\":" + (gps.location.isValid() ? String(gps.location.lat(), 6) : "null");
  json += ",\"gps_longitude\":" + (gps.location.isValid() ? String(gps.location.lng(), 6) : "null");
  json += ",\"gps_altitude\":" + (gps.altitude.isValid() ? String(gps.altitude.meters(), 1) : "null");
  json += ",\"gps_satellites\":" + jsonUnsignedLong(gps.satellites.value(), gps.satellites.isValid());
  json += "}";
  return json;
}

void sendSensorData() {
  updateGPS();
  float temperature;
  float humidity;
  float pressure;
  int turbidityADC;
  float turbidityVoltage;
  readDHT22(temperature, humidity);
  readBMP280(pressure);
  readTurbidity(turbidityADC, turbidityVoltage);
  float distance = readUltrasonic();
  float probeTemperature = readDS18B20();
  float accelX;
  float accelY;
  float accelZ;
  float gyroX;
  float gyroY;
  float gyroZ;
  readMPU6050(accelX, accelY, accelZ, gyroX, gyroY, gyroZ);
  // NTU requires calibration against known standards; this is raw ADC demonstration data.
  Serial.println(createJSON(temperature, humidity, pressure, turbidityADC,
                            turbidityVoltage, distance, probeTemperature,
                            accelX, accelY, accelZ, gyroX, gyroY, gyroZ));
}

void setup() {
  Serial.begin(115200);
  setupSensors();
}

void loop() {
  updateGPS();
  unsigned long now = millis();
  if (now - lastReading >= SENSOR_INTERVAL_MS) {
    lastReading = now;
    sendSensorData();
  }
}
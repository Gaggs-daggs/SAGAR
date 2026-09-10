#include <Arduino.h>
#include <Wire.h>

#define MPU_SDA_PIN 32
#define MPU_SCL_PIN 23

TwoWire mpuWire = TwoWire(1);
bool mpuInitialized = false;

int16_t readWord(byte highRegister) {
  mpuWire.beginTransmission(0x68);
  mpuWire.write(highRegister);
  mpuWire.endTransmission(false);
  mpuWire.requestFrom(0x68, 2);
  return (int16_t)((mpuWire.read() << 8) | mpuWire.read());
}

void scanI2C() {
  Serial.println("Scanning MPU6050 bus: SDA=GPIO32 SCL=GPIO23");
  byte found = 0;
  for (byte address = 1; address < 127; address++) {
    mpuWire.beginTransmission(address);
    byte error = mpuWire.endTransmission();
    if (error == 0) {
      Serial.print("I2C device found at 0x");
      if (address < 16) Serial.print("0");
      Serial.println(address, HEX);
      found++;
    }
  }
  if (found == 0) Serial.println("No I2C devices found");
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  mpuWire.begin(MPU_SDA_PIN, MPU_SCL_PIN);
  scanI2C();

  mpuWire.beginTransmission(0x68);
  mpuWire.write(0x75);
  mpuWire.endTransmission(false);
  mpuWire.requestFrom(0x68, 1);
  if (mpuWire.available()) {
    Serial.print("WHO_AM_I(0x68)=0x");
    Serial.println(mpuWire.read(), HEX);
  }

  mpuWire.beginTransmission(0x68);
  mpuWire.write(0x6B);
  mpuWire.write(0x00);
  if (mpuWire.endTransmission() != 0) {
    Serial.println("MPU6500 initialization failed");
    return;
  }
  mpuWire.beginTransmission(0x68);
  mpuWire.write(0x1C);
  mpuWire.write(0x10);
  mpuWire.endTransmission();
  mpuWire.beginTransmission(0x68);
  mpuWire.write(0x1B);
  mpuWire.write(0x08);
  mpuWire.endTransmission();
  mpuInitialized = true;
  Serial.println("MPU6500-compatible sensor initialized successfully");
}

void loop() {
  if (!mpuInitialized) {
    delay(1000);
    return;
  }

  static bool initialized = false;
  if (!initialized) {
    initialized = true;
    delay(1000);
  }

  float accelX = readWord(0x3B) / 4096.0f * 9.80665f;
  float accelY = readWord(0x3D) / 4096.0f * 9.80665f;
  float accelZ = readWord(0x3F) / 4096.0f * 9.80665f;
  float gyroX = readWord(0x43) / 65.5f * DEG_TO_RAD;
  float gyroY = readWord(0x45) / 65.5f * DEG_TO_RAD;
  float gyroZ = readWord(0x47) / 65.5f * DEG_TO_RAD;
  Serial.printf("accel_mps2=%.3f,%.3f,%.3f gyro_rads=%.3f,%.3f,%.3f\n",
                accelX, accelY, accelZ, gyroX, gyroY, gyroZ);
  delay(1000);
}

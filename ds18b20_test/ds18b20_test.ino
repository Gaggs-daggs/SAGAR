#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define DS18B20_PIN 27

OneWire oneWire(DS18B20_PIN);
DallasTemperature sensors(&oneWire);

void setup() {
  Serial.begin(115200);
  delay(1000);
  sensors.begin();
  Serial.print("DS18B20 bus GPIO27, devices=");
  Serial.println(sensors.getDeviceCount());
}

void loop() {
  sensors.requestTemperatures();
  int count = sensors.getDeviceCount();
  Serial.print("devices=");
  Serial.print(count);
  if (count > 0) {
    float temperature = sensors.getTempCByIndex(0);
    Serial.print(" temperature_c=");
    Serial.println(temperature, 2);
  } else {
    Serial.println(" temperature_c=NO_DEVICE");
  }
  delay(1500);
}

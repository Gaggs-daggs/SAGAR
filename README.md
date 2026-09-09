# SAGAR-NODE

Basic wired environmental monitoring prototype: ESP32 sensors -> USB serial -> local Python Flask dashboard. This version uses no Wi-Fi, cloud, MQTT, LoRa, Bluetooth, or internet service at runtime.

## Wiring

| Device | Pin | ESP32 |
|---|---|---|
| DHT22 | VCC / DATA / GND | 3.3V / GPIO33 / GND |
| GY-BMP280 | VCC / GND / SDA / SCL | 3.3V / GND / GPIO21 / GPIO22 |
| Turbidity | VCC / OUT / GND | VIN/5V / divider junction to GPIO34 / GND |
| HC-SR04 | VCC / TRIG / ECHO / GND | VIN/5V / GPIO25 / GPIO26 through a separate divider / GND |

Connect turbidity OUT to one 4.7k resistor, the resistor's other end to GPIO34 and the top of the second 4.7k resistor, and the second resistor's other end to GND. This halves the sensor output before it reaches the ESP32 ADC. GPIO34 is input-only; never configure it as an output. Accurate NTU requires calibration against known standards, so the dashboard reports comparative raw ADC and divider voltage only.

HC-SR04 ECHO can be 5 V and must never connect directly to GPIO26. Add a separate suitable divider before enabling it. The firmware currently has `ENABLE_ULTRASONIC 0`, so the prototype safely reports `null` / `NO ECHO` without that divider. Do not reuse the turbidity resistors.

All grounds must be common. Power the ESP32 and sensors from the USB-connected board. Do not add a battery or external supply in this version. Leave BMP280 CSB and SDO unconnected.

## Arduino CLI

Install Arduino CLI from the official Arduino CLI release for your operating system, then open a new terminal. Check it:

```text
arduino-cli version
arduino-cli config init
arduino-cli core update-index
arduino-cli core install esp32:esp32
arduino-cli lib install "DHT sensor library"
arduino-cli lib install "Adafruit Unified Sensor"
arduino-cli lib install "Adafruit BME280 Library"
```

Connect the ESP32 by USB and run `arduino-cli board list`. Use the port shown there. The usual generic DevKit FQBN is `esp32:esp32:esp32`, but confirm the board package and detected board in your output rather than assuming it. Compile and upload from the project root:

```text
arduino-cli compile --fqbn esp32:esp32:esp32 firmware
arduino-cli upload -p COM3 --fqbn esp32:esp32:esp32 firmware
arduino-cli monitor -p COM3 -c baudrate=115200
```

Replace `COM3` and the FQBN with the values from `arduino-cli board list`. On Linux use `/dev/ttyUSB0` or `/dev/ttyACM0`; on macOS use `/dev/cu.usbserial-XXXX`.

## Dashboard

From `sagar-node`:

```text
python -m venv .venv
# Windows PowerShell
.venv\Scripts\Activate.ps1
# macOS/Linux: source .venv/bin/activate
python -m pip install -r requirements.txt
python dashboard/app.py
```

Open `http://127.0.0.1:5000`. Set `SERIAL_PORT` in `dashboard/app.py` to your port, or use `SAGAR_SERIAL_PORT=AUTO` for automatic detection. Python can list ports with:

```text
python -c "from serial.tools import list_ports; print([(p.device, p.description) for p in list_ports.comports()])"
```

The server reads one JSON line at a time at 115200 baud, ignores malformed lines, reconnects after disconnects, and keeps 60 readings. DHT22 and BMP280 errors appear as missing values; the serial reader stays alive. The dashboard declares the ESP32 offline after six seconds without a valid reading. A large turbidity change triggers a demonstration ALERT; it is not a scientific classification.

## Presentation procedure

1. Wire the sensors and common ground; keep HC-SR04 disabled unless its separate divider is fitted.
2. Connect the ESP32 to the laptop over USB.
3. Run `arduino-cli board list`, compile, upload, and start the monitor if you want to inspect JSONL.
4. Start `python dashboard/app.py` and open the local URL.
5. Show live temperature, humidity, pressure, raw turbidity, and voltage.
6. Move the turbidity probe between clearer and cloudier water. Move an object in front of the protected HC-SR04 if enabled; gently warm or cool the DHT22.
7. Show the rolling graphs and the turbidity ALERT state.

This is a basic wired prototype. It does not measure calibrated NTU, wind speed/direction, ocean readiness, or autonomous solar operation. For a future system, add local storage, a decision engine, LoRa or satellite communication, and a remote ground station. Future modules may include MPU6050 for motion, INA219 for power, GPS for location, RA-02 LoRa, microSD, solar, and battery storage.

## Troubleshooting

- No port: try another data-capable USB cable, install the board's USB-UART driver, and rerun `arduino-cli board list`.
- Upload fails: close Arduino CLI monitor, verify the port and FQBN, and hold BOOT while upload begins if your board needs it.
- Offline dashboard: stop any serial monitor, set the correct `SERIAL_PORT`, and ensure only one process owns the port.
- DHT22 ERROR / blank temperature: check 3.3 V, DATA on GPIO33, common ground, and sensor timing; this sketch tolerates failed reads.
- BMP280 ERROR / blank pressure: check I2C wiring and try the module's 0x76/0x77 address; both are attempted.
- Turbidity error: confirm the 1:1 divider junction is on GPIO34 and the sensor is powered from VIN/5V; never exceed 3.3 V at GPIO34.
- NO ECHO: expected while ultrasonic support is disabled. Never enable it without a separate ECHO divider.
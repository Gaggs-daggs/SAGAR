import json
import os
import threading
import time
from collections import deque

import serial
from serial.tools import list_ports
from flask import Flask, jsonify, render_template

SERIAL_PORT = os.getenv("SAGAR_SERIAL_PORT", "COM3")
BAUD_RATE = 115200
STALE_AFTER_SECONDS = 6
history = deque(maxlen=60)
latest = {"temperature": None, "humidity": None, "pressure": None,
          "turbidity_adc": None, "turbidity_voltage": None, "distance": None,
          "probe_temperature": None, "gps_valid": False,
          "mpu6050_accel_x": None, "mpu6050_accel_y": None, "mpu6050_accel_z": None,
          "mpu6050_gyro_x": None, "mpu6050_gyro_y": None, "mpu6050_gyro_z": None,
          "gps_latitude": None,
          "gps_longitude": None, "gps_altitude": None, "gps_satellites": None,
          "timestamp": None, "received_at": None}
lock = threading.Lock()

app = Flask(__name__)


def detect_port():
    ports = list(list_ports.comports())
    if SERIAL_PORT and SERIAL_PORT != "AUTO":
        return SERIAL_PORT
    for port in ports:
        description = f"{port.description or ''} {port.manufacturer or ''}".lower()
        if "esp32" in description or "cp210" in description or "ch340" in description:
            return port.device
    return ports[0].device if ports else None


def valid_reading(data):
    required = ("temperature", "humidity", "pressure", "turbidity_adc", "turbidity_voltage",
                "distance", "probe_temperature", "gps_valid", "gps_latitude",
                "gps_longitude", "gps_altitude", "gps_satellites",
                "mpu6050_accel_x", "mpu6050_accel_y", "mpu6050_accel_z",
                "mpu6050_gyro_x", "mpu6050_gyro_y", "mpu6050_gyro_z")
    return isinstance(data, dict) and all(key in data for key in required)


def serial_reader():
    while True:
        port = detect_port()
        if not port:
            time.sleep(2)
            continue
        try:
            with serial.Serial(port, BAUD_RATE, timeout=1) as connection:
                while True:
                    line = connection.readline().decode("utf-8", errors="ignore").strip()
                    if not line:
                        continue
                    try:
                        data = json.loads(line)
                    except json.JSONDecodeError:
                        continue
                    if not valid_reading(data):
                        continue
                    data.setdefault("probe_temperature", None)
                    received_at = time.time()
                    data["received_at"] = received_at
                    data["timestamp"] = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime(received_at))
                    with lock:
                        latest.update(data)
                        history.append(dict(data))
        except (serial.SerialException, OSError):
            time.sleep(2)


def snapshot():
    with lock:
        data = dict(latest)
    is_online = data["received_at"] is not None and time.time() - data["received_at"] <= STALE_AFTER_SECONDS
    data["online"] = is_online
    data["age_seconds"] = None if data["received_at"] is None else round(time.time() - data["received_at"], 1)
    return data


@app.get("/")
def index():
    return render_template("index.html")


@app.get("/api/data")
def api_data():
    return jsonify(snapshot())


@app.get("/api/history")
def api_history():
    with lock:
        values = list(history)
    return jsonify(values)


if __name__ == "__main__":
    threading.Thread(target=serial_reader, daemon=True).start()
    app.run(host="127.0.0.1", port=5000, debug=False)
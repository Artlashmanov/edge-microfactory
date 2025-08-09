# Irrigation (ESP32)
- **Pins:** Relay=GPIO26, Moisture sensor=GPIO36 (VP/A0).
- **Web:** `/pump?cmd=RUN&ms=N`, `/pump?cmd=ON|OFF`, `/status`.
- **MQTT:** pub `moisture/plant1`, sub `watering/plant1` (ON/OFF).

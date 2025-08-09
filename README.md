# Edge Microfactory (WIP)

Modular **edge** setup: ESP32/Arduino nodes + Core services + Present (dashboards).

## Today (MVP)
- Irrigation: ESP32 + relay + soil sensor, **timed pump** via web UI, MQTT telemetry.
- Omni-robot: ESP32 web controller → UNO motor board (74HC595 + PWM + servo + ultrasonic).

## Repository structure
- `edge/irrigation-esp32` – firmware & web UI (run pump for N ms, MQTT fallback).
- `edge/robot/esp32-ui` – web joystick (3×3 arrows, ⟳⟲), OTA, logs.
- `edge/robot/uno-firmware` – motor control firmware (A5 <code> 5A protocol).
- `core/` – Mosquitto + Postgres + Grafana (docker-compose).
- `present/` – dashboards (WIP).
- `docs/` – architecture and wiring.

## Quick start
- Open each PlatformIO project and fill Wi-Fi/MQTT in `src/main.cpp`.
- Flash UNO first (`edge/robot/uno-firmware`), then ESP32 UI (`edge/robot/esp32-ui`).
- For irrigation open ESP32 IP in browser → set seconds → RUN.

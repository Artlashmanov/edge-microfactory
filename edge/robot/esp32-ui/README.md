# Robot UI (ESP32 → UNO)
- **UART:** ESP32 TX2=GPIO17 → UNO RX0 (use level-shifter or resistor divider 5V→3V3), ESP32 RX2=GPIO16 ← UNO TX, common GND.
- **Protocol:** `A5 <code> 5A` (e.g. 0x5C Forward, 0x53 CW, 0xAC CCW).
- **UI:** 3×3 arrows + STOP, rotate ⟳/⟲, Servo ◄/►, distance from UNO in status.

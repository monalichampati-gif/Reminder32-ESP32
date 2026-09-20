# REMINDER 32 — Distraction-Free Study Assistant

An embedded, offline desk companion designed to help students stay focused during study sessions without smartphone distractions.

## Project Highlights
- **IDE / Environment:** Built using PlatformIO (VS Code) with C++
- **Offline Timekeeping:** DS1302 Real-Time Clock with CR2032 battery backup
- **Distraction-Free UI:** High-contrast 0.96" OLED Display (SSD1306)
- **Interactive Control:** Non-contact TTP223 Capacitive Touch Sensors
- **Persistent Data:** Reminders and configurations saved in non-volatile flash storage (NVS)

## Hardware Circuit
- ESP32 Microcontroller
- DS1302 RTC Module
- 0.96" I2C OLED Screen (128x64)
- TTP223 Touch Modules
- 5V Active Buzzer

# REMINDER 32 — Distraction-Free Study Assistant

An embedded, offline desk companion designed to help students stay focused during study sessions without smartphone distractions.

---

## Project Highlights

- **IDE / Environment:** Built using PlatformIO (VS Code) with C++
- **Offline Timekeeping:** DS1302 Real-Time Clock with CR2032 battery backup
- **Distraction-Free UI:** High-contrast 0.96" OLED Display (SSD1306)
- **Interactive Control:** Non-contact TTP223 Capacitive Touch Sensors
- **Persistent Data:** Reminders and configurations saved in non-volatile flash storage (NVS)

---

## Hardware Circuit

- ESP32 Microcontroller
- DS1302 RTC Module
- 0.96" I2C OLED Screen (128x64)
- TTP223 Touch Modules
- 5V Active Buzzer

---

## Wiring & Pinout Table

| Component | Module Pin | ESP32 Pin |
| :--- | :--- | :--- |
| **0.96" OLED Display** | VCC / GND | 3.3V / GND |
| | SDA / SCL | GPIO 21 / GPIO 22 |
| **DS1302 RTC Module** | VCC / GND | 3.3V / GND |
| | CLK / DAT / RST | GPIO 18 / GPIO 19 / GPIO 5 |
| **Buttons / Sensors** | UP Button | GPIO 19 |
| | DOWN Button | GPIO 18 |
| | SELECT Button | GPIO 4 |
| **Active Buzzer** | POS (+) / NEG (-) | GPIO 17 / GND |

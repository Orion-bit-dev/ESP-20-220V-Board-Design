# 220V Power Monitor (ESP32)

An ESP32 project that switches two 220 V lamps with push buttons and a live web page, senses an outside 220 V power source, keeps working when WiFi drops, and lets me save a new WiFi network from my phone.

Built as part of my learning in programming and electronics. It builds on two earlier projects: a basic power monitor and a power-sensing relay with a web page.

---

## ⚠️ Safety first (220 V)

This project involves mains voltage, which can seriously injure or kill.

- **Never put 220 V on the breadboard or near the ESP32 and OLED.** Use the screw terminals of the relay modules only.
- Keep the whole 220 V side in a **closed enclosure**, with a **fuse or breaker** before it.
- Have a **qualified electrician** make the 220 V connections. Switch the power off at the breaker before touching anything.
- The ESP32 side runs at only 3.3 V and 5 V. That is the part I built myself.

---

## Features

- **Two relay outputs** switch two 220 V lamps (LED 1 and LED 2).
- **Physical push buttons** toggle each relay.
- **Live web page** shows the full state and refreshes every second (`/status` endpoint returning JSON).
- **OLED display** (I2C) shows the same state, plus the IP address for 3 seconds at boot.
- **Mains sensing** of an outside 220 V source ("Power 2") using an isolated supply and an opto module.
- **WiFi that doesn't freeze the device:** a non-blocking `millis()` timer reconnects every 15 seconds, so the buttons always work, even offline.
- **WiFi setup mode:** if no WiFi is saved, or I hold the setup button for 3 seconds, the ESP32 creates its own network (`ESP32_Setup`). I join it from my phone, open `192.168.4.1`, enter the new WiFi, and it saves to flash and restarts.
- **Admin password** required for the web Toggle buttons, since they switch 220 V.

### LED logic

| LED | Turns on when |
|---|---|
| LED 1 | Relay A is on **or** Power 2 is present |
| LED 2 | Relay B is on |

---

## Hardware

- ESP32 dev board
- 2 × 5 V relay modules (active low)
- 2 × push buttons (Relay A, Relay B) + 1 × setup button
- SSD1306 OLED, 128×64, I2C
- 100 µF capacitor
- HLK-PM01 (isolated 220 V to 5 V module)
- Opto-isolator module
- Fuse, enclosure, and two 220 V lamps

### Pin map (ESP32 side)

| Part | ESP32 pin |
|---|---|
| Relay A `IN` | D18 (relay `VCC` to VIN, `GND` to GND) |
| Relay B `IN` | D19 |
| Button A | D25 and GND |
| Button B | D26 and GND |
| Setup button | D32 and GND |
| Opto module output (Power 2 sense) | D27 and GND (LOW = present) |
| OLED | SDA D21, SCL D22, VCC 3V3, GND |
| Capacitor 100 µF | across 3V3 and GND (stripe leg to GND) |

### Sensing an outside 220 V source safely

```
220 V (Power 2) -> fuse -> HLK-PM01 (isolated 5 V module) -> opto module input (IN+ / IN-)
opto module output -> ESP32 D27 and GND
```

- Opto module jumper **ON** (3–12 V input).
- The HLK output minus goes **only to the opto IN-**, never to the ESP32 GND. The opto module is the isolation bridge.
- Do **not** use a bare opto module directly on 220 V, and never use a resistor or capacitor dropper supply.
- Alternative: a relay with a 220 V coil, with its dry contact wired to D27 and GND like a switch.

---

## Software

### Requirements

- Arduino IDE with the ESP32 board package
- Libraries: `Adafruit GFX Library` and `Adafruit SSD1306`
- Built-in ESP32 libraries used: `WiFi`, `WebServer`, `Preferences`, `Wire`

### Setup

1. Open `power_monitor_220v.ino` in the Arduino IDE.
2. **Change the placeholder passwords** at the top of the sketch:
   - `apPass`: the password of the setup WiFi (8 characters or more)
   - `adminPass`: the password needed for the web Toggle buttons
3. Select your ESP32 board and upload.
4. On first boot there is no saved WiFi, so the OLED shows **WIFI SETUP MODE**. Connect your phone to `ESP32_Setup`, open `192.168.4.1`, enter your WiFi name and password, and tap **Save & Reboot**.
5. After reboot, the OLED shows the IP address. Open it in a browser to see the live page.

> Never commit real WiFi names or passwords to a public repository.

### Web endpoints

| Endpoint | Method | Purpose |
|---|---|---|
| `/` | GET | Live status page (or the WiFi form in setup mode) |
| `/status` | GET | Current state as JSON |
| `/toggleA`, `/toggleB` | POST | Toggle a relay (needs the admin password) |
| `/savewifi` | POST | Save a new WiFi (works only in setup mode) |

---

## Security fixes from my first version

- Removed the fallback WiFi name and password from the code. With nothing saved, the device goes straight to setup mode.
- `/savewifi` now works only in setup mode. Before, anyone on my WiFi could overwrite my settings.
- The web Toggle buttons need the admin password, because they switch 220 V.
- Holding the setup button no longer restarts setup mode repeatedly.
- The OLED now also updates when I toggle from the web page.

---

## Test checklist

| Check | Expected result |
|---|---|
| Nothing saved yet | OLED shows WIFI SETUP MODE |
| Boot with saved WiFi | "Connecting to WiFi", then the IP for 3 s, then the normal screen |
| Web page | Updates every second, no `?` shown |
| Buttons A and B | Relay clicks, web page and OLED follow |
| Web Toggle, wrong admin password | "Not allowed", relay does not move |
| Web Toggle, right admin password | Relay toggles, OLED follows |
| Power 2 on and off | LED 1 follows, Serial prints the change |
| Router off for 15 s, then on | Serial prints "WiFi dropped!", then it reconnects |
| Hold setup button for 3 s | OLED shows setup instructions, relays turn off |
| Save a new WiFi in setup mode | Restarts and joins it |
| `/savewifi` in normal mode | "Only in setup mode" |
| Unplug and plug back in | Remembers the saved WiFi |

---

## What I learned

- **OLED static at boot:** the WiFi power spike crashed the screen. Fixed with a 100 µF capacitor and calling `display.begin()` again after the WiFi attempt.
- **Never use `WiFi.mode(WIFI_OFF)`:** it crashed my ESP32 (`xQueueSemaphoreTake`). Use `WIFI_STA` or `WIFI_AP` instead, and let the buttons work offline.
- **No `while` loop without a `millis()` timeout:** the ESP32 freezes if the router is off.
- **Active-low relays:** "off" means letting go of the pin (`OUTPUT_OPEN_DRAIN` with HIGH). A normal `digitalWrite(HIGH)` left the relay stuck on.
- **The web page `?` bug:** switching to `XMLHttpRequest` fixed it for me. `fetch()` worked on an earlier page, so the real cause is not proven and is worth testing again.
- **Keep secrets out of code:** I once had my real WiFi password in a sketch. Secrets don't belong in code, notes, or chats.

---

## Roadmap

- [ ] Add a password to the `/savewifi` page and show the IP in the setup screen
- [ ] Draw the schematic in KiCad, with a mains block (fuse, HLK-PM01) separated from the low-voltage block (opto module, relays, ESP32)
- [ ] Add photos of the build and wiring

---

## Disclaimer

This is a learning project, provided as is. Working with mains voltage is dangerous. Always involve a qualified electrician for the 220 V side, and use this at your own risk.

# GrandpaMB — Grandpa's own hardware board

**Status:** v0.1.0 — Experimental (simulation only, Wokwi)

## What GrandpaMB is (and isn't)

GrandpaMB is the **physical body** of Grandpa: an ESP32-S3 edge node that sits on your desk,
shows what Grandpa is doing, takes button input, senses the room and switches small loads.

The **brain stays on the PC / Pironman** (Ollama, AI Engine, memory, tools). An ESP32 has
~512 KB RAM — it cannot run an LLM. That's the right split anyway: cheap, low-power board
for I/O; powerful machine for AI.

```
 [ PC: Grandpa core + Ollama ]  <── serial / (later) WiFi+MQTT ──>  [ GrandpaMB (ESP32-S3) ]
                                                                     ├─ LED ring  (state)
                                                                     ├─ OLED      (status/messages)
                                                                     ├─ WAKE / MUTE buttons
                                                                     ├─ DHT22 / PIR / LDR (room)
                                                                     └─ relay / buzzer / LED
```

## Hardware v0 (pin map — `include/config.h` and `diagram.json` must match)

| Part | Pin | Notes |
|---|---|---|
| SSD1306 OLED 128x64 | SDA 8, SCL 9 | I2C 0x3C, 3.3V |
| 12-LED NeoPixel ring | 5 | 5V |
| WAKE button | 4 | to GND, internal pull-up |
| MUTE button | 6 | to GND; **long-press = relay kill switch** |
| DHT22 | 15 | temp + humidity |
| PIR motion | 16 | presence |
| LDR module (AO) | 1 | ADC1 (ADC2 can't be used with WiFi) |
| Relay module | 17 | boots OFF, ON needs confirmation |
| Buzzer | 18 | |
| Status LED + 220Ω | 2 | |

## Setup on Windows (one time)

1. VS Code extensions: **PlatformIO IDE** and **Wokwi Simulator** (`.vscode/extensions.json` will prompt you).
2. Activate Wokwi: `F1` → **Wokwi: Request a New License** → sign in in the browser → it returns to VS Code.
   *(This is the #1 reason "Wokwi won't connect".)*
3. Open the **GrandpaMB folder itself** in VS Code (`File → Open Folder`), not a parent folder —
   Wokwi looks for `wokwi.toml` in the workspace root.

## Run it

```bat
:: Easiest: VS Code PlatformIO sidebar (alien icon) -> grandpamb -> Build
:: Or from cmd (pio is not on PATH by default on Windows):
%USERPROFILE%\.platformio\penv\Scripts\pio.exe run
```
To make plain `pio` work everywhere, add `%USERPROFILE%\.platformio\penv\Scripts` to your user PATH and open a new terminal.
Then `F1` → **Wokwi: Start Simulator**. Open `diagram.json` to see the circuit.

> "firmware not found" error = you didn't build yet. `wokwi.toml` points at `.pio/build/grandpamb/firmware.bin`.

## Run without VS Code (wokwi-cli)

wokwi-cli runs the same `wokwi.toml` + `diagram.json` from the terminal (the simulation runs on Wokwi's cloud, so you need internet).

One time (cmd):
```bat
powershell -NoProfile -ExecutionPolicy Bypass -Command "iwr https://wokwi.com/ci/install.ps1 -useb | iex"
:: create a CI token at https://wokwi.com/dashboard/ci, then:
setx WOKWI_CLI_TOKEN <your-token>
:: close ALL cmd windows (and VS Code) - setx only affects NEW terminals
```
Never commit the token or put it in any file in this repo.

Every run (new cmd window, in the GrandpaMB folder):
```bat
wokwi-cli --version
%USERPROFILE%\.platformio\penv\Scripts\pio.exe run
wokwi-cli --timeout 15000 --expect-text "\"type\":\"hello\"" .
wokwi-cli --timeout 30000 --scenario tests/smoke.test.yaml .
```
Exit code 0 = pass (`echo %ERRORLEVEL%`). The smoke test checks: hello → `ping`/pong →
`state thinking` → `relay on` asks for confirmation (relay stays OFF) → `cancel`.

## Talk to it

**A. Directly in Wokwi's serial monitor** (plain text works):
```
help
ping              -> board becomes "idle" (Grandpa connected)
state thinking    -> purple spinner
say Vanakkam!
beep 300
relay on          -> returns a token, e.g. "token":"3FA9C1"
confirm 3FA9C1    -> relay ON (within 10 s, wrong token cancels)
relay off
```

**B. From Python (this is how Grandpa core will use it)** — keep the simulator running, new terminal:
```powershell
pip install pyserial
python tools\grandpa_link.py --port rfc2217://localhost:4000
```
Click the green **WAKE** button in the simulator → the script runs a demo turn
(listening → thinking → speaking → idle). Type `relay on` → you must type `yes` on the PC.

Tests (no simulator needed): `pip install pytest` then `python -m pytest tools -q`

## Protocol (newline-delimited JSON over serial)

| Host → board | Board reply |
|---|---|
| `{"id":1,"cmd":"ping"}` | `{"type":"reply","id":1,"ok":true,"msg":"pong"}` |
| `{"id":2,"cmd":"state","value":"listening"}` | states: idle, listening, thinking, speaking, muted, error |
| `{"id":3,"cmd":"relay","value":"on"}` | `{"confirm_required":true,"token":"…","expires_ms":10000}` |
| `{"id":4,"cmd":"confirm","token":"…"}` | relay switched |

Board → host (unsolicited): `hello`, `telemetry` (every 5 s), `event` (`wake`, `wake_long`,
`muted`, `unmuted`, `motion_start`, `motion_end`, `relay_forced_off`, `confirm_expired`).

## Safety rules built in

- Relay is **OFF on every boot/reset** (set before anything else in `setup()`).
- Relay **ON = 2-step**: board token + human "yes" on PC. **OFF is always instant**.
- MUTE long-press = physical kill switch for the relay, no software can block it.
- Lines over 256 chars are dropped, not executed truncated. Beep capped at 2 s.
- While MUTED, the host cannot change state and WAKE is ignored.
- ⚠ Real mains (230V) on the relay = only with a proper enclosed relay module, fuse, and someone experienced. Practise with a 5V/12V LED strip first.

## Roadmap: simulation → real PCB

| Stage | What | Tool | Status |
|---|---|---|---|
| 0 | This simulation, protocol, Python bridge | Wokwi + PlatformIO | **Done (v0.1)** |
| 1 | Same circuit on breadboard with real ESP32-S3 DevKitC-1 (N16R8) | breadboard | Planned |
| 2 | Add voice: INMP441 I2S mic + MAX98357A I2S amp + speaker (Wokwi can't simulate I2S audio — real hardware only) | breadboard | Planned |
| 3 | WiFi + MQTT to Grandpa core / Pironman (replace USB serial) | firmware | Planned |
| 4 | Schematic + PCB layout (ESP32-S3-WROOM-1 module, USB-C, 3.3V LDO, I2S, headers) | **KiCad** (free) | Planned |
| 5 | Order PCB + assembly | JLCPCB / PCBWay | Planned |

Wokwi is a **simulator**, not a PCB tool — it proves the circuit and firmware. The actual board
file (Gerbers) comes from KiCad in Stage 4.

### Stage 1 shopping list (approx. — check current prices)
ESP32-S3 DevKitC-1 N16R8, SSD1306 0.96" I2C OLED, WS2812 12-LED ring, DHT22, HC-SR501 PIR,
LDR module, 1-ch 5V relay module (optocoupled), active/passive buzzer, 2 tactile buttons,
LEDs + 220Ω, breadboard + jumper wires. Stage 2 adds INMP441 + MAX98357A + 3W 4Ω speaker.

## Project layout
```
GrandpaMB/
├─ diagram.json          # Wokwi circuit
├─ wokwi.toml            # tells Wokwi which firmware to run + serial bridge port
├─ platformio.ini        # build config + libraries
├─ include/config.h      # ALL pins and constants
├─ src/
│  ├─ main.cpp           # wiring everything together, app state
│  ├─ protocol.*         # JSON/plain-text command handling + confirm flow
│  ├─ status_ring.*      # LED ring animations
│  ├─ display.*          # OLED dashboard
│  ├─ sensors.*          # DHT22, PIR, LDR
│  ├─ buttons.*          # debounced buttons (press / long-press)
│  ├─ actuators.*        # relay, buzzer, LED
│  └─ assistant_state.*  # shared state enum
├─ tests/
│  └─ smoke.test.yaml    # Wokwi automation scenario (wokwi-cli --scenario)
└─ tools/
   ├─ grandpa_link.py    # PC bridge (Wokwi or real COM port)
   └─ test_grandpa_link.py
```

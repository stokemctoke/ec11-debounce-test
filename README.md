# EC11 Debounce Test

A focused learning sketch for the **EC11 rotary encoder** on a **Tenstar Robot ESP32-C3 Super-Mini**. It decodes the encoder in software using a robust **polled + confirmed Buxtronix** debounce, and gives you immediate feedback through an external WS2812 LED, the onboard blue LED, and an optional SSD1306 OLED.

The goal is to get familiar with the EC11, how quadrature works, and how a software debounce turns a noisy mechanical encoder into clean, one-step-per-detent counts.

## Hardware

| Connection | Board pin |
|---|---|
| Encoder CLK (A) | `GPIO0` |
| Encoder DT (B) | `GPIO1` |
| Encoder SW (push) | `GPIO3` |
| Encoder common | `GND` |
| External WS2812 data | `GPIO10` (recommended 300–500Ω series resistor) |
| External WS2812 V+ | `5V` |
| External WS2812 GND | `GND` |
| Onboard blue LED | `GPIO8` (active LOW, used by the sketch) |
| OLED SDA (optional) | `GPIO6` |
| OLED SCL (optional) | `GPIO7` |
| OLED VCC (optional) | `3.3V` |
| OLED GND (optional) | `GND` |

The OLED is an **SSD1306 128×64** over I2C. It's auto-detected at boot (the I2C bus is scanned and the panel is driven at whichever of `0x3C`/`0x3D` responds). The sketch runs fine without one and prints the same event stream to Serial regardless.

A 20-detent EC11 is assumed. One detent = one step.

## Behaviour

- Right turn → external LED flashes **red**, onboard blinks
- Left turn → external LED flashes **blue**, onboard blinks
- Short click → external LED flashes **green**, onboard blinks
- **Hold the encoder switch for 1 second → all counters reset**, LED flashes **white**

The cycle counter wraps every 20 rotations; clicks do not advance the cycle counter but increment their own total.

If an OLED is attached, the header shows the active debounce method, with the current cycle position, last action, and totals below.

Serial monitor (115200 baud) prints one line per event:

```
--> RIGHT - STEP 01
--> RIGHT - STEP 02
<-- LEFT  - STEP 03
(*) CLICK - CLK 01
>>> counters reset <<<
```

## File layout

Everything lives in `ec11_debounce_test/`. Each file has one job:

| File | Purpose |
|---|---|
| `ec11_debounce_test.ino` | `setup()` + `loop()` — orchestration, rotation drain, and the short/long-press button handler |
| `Config.h` | All pin assignments and timing constants |
| `State.{h,cpp}` | Shared counters and `lastAction`; `recordRotation()` / `recordClick()` / `resetCounts()` |
| `LedFx.{h,cpp}` | FastLED external strip + onboard LED flash management |
| `Display.{h,cpp}` | SSD1306 detection, I2C scan, splash, redraw |
| `SerialOut.{h,cpp}` | Formatted banner and event lines |
| `Encoder.{h,cpp}` | The polled + confirmed Buxtronix decoder (`encoder_init`/`poll`/`getPos`) |

## How the debounce works

1. **Sample** CLK and DT every 200 µs (5 kHz).
2. **Confirm** — a new pin reading must hold steady across 2 consecutive samples before it's accepted. This rejects any bounce shorter than ~400 µs.
3. **Decode** — confirmed transitions drive a full-step Buxtronix state machine that only emits a step on a complete CW/CCW quadrature cycle. Bounce that doesn't complete a cycle harmlessly returns to the start state.

The result is robust against contact bounce at the cost of ~one sample of latency. Tune `SAMPLE_US` and `CONFIRM_COUNT` in `Config.h` to experiment.

The push switch is debounced separately with a 30 ms time-window filter; holding it past `LONG_PRESS_MS` (1000 ms) triggers the counter reset.

> **Note:** the ESP32-C3 has no hardware Pulse Counter (PCNT) peripheral, so the
> "let the silicon decode quadrature for free" approach isn't possible on this
> board — decoding is done entirely in software.

## Building

Open `ec11_debounce_test/ec11_debounce_test.ino` in the Arduino IDE. The other files appear as tabs.

**Tools menu settings:**

| Setting | Value |
|---|---|
| Board | `ESP32C3 Dev Module` |
| USB CDC On Boot | **Enabled** (required for the Serial monitor) |
| CPU Frequency | `160MHz (WiFi)` |
| Flash Mode | `QIO` |
| Flash Size | `4MB (32Mb)` |
| Upload Speed | `921600` |

**Libraries** (Sketch → Include Library → Manage Libraries):

- `FastLED`
- `U8g2` (only needed if you want the optional OLED display)

Requires **Arduino-ESP32 core 3.0+**.

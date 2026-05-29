# EC11 Debounce Test

Side-by-side demo of **three different EC11 rotary encoder debounce strategies** on a **Tenstar Robot ESP32-C3 Super-Mini**, with shared visual feedback via an external WS2812 LED, the onboard blue LED, and an optional SSD1306 OLED.

The default method is a polled + confirmed Buxtronix decoder — the rest are present as separate, self-contained files so you can read each approach in isolation and (once the mode-cycler lands) swap between them with a long press of the encoder.

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

The OLED is an **SSD1306 128×64** over I2C. It's auto-detected at boot — the sketch runs fine without one and prints the same event stream to Serial regardless.

A 20-detent EC11 is assumed. One detent = one step.

## Behaviour

- Right turn → external LED flashes **red**, onboard blinks
- Left turn → external LED flashes **blue**, onboard blinks
- Click → external LED flashes **green**, onboard blinks

The cycle counter wraps every 20 rotations; clicks do not advance the cycle counter but increment their own total.

If an OLED is attached, the display shows the current cycle position, last action, and totals.

Serial monitor (115200 baud) prints one line per event:

```
--> RIGHT - STEP 01
--> RIGHT - STEP 02
<-- LEFT  - STEP 03
(*) CLICK - CLK 01
```

## File layout

Everything lives in `ec11_debounce_test/`. Each file has one job:

| File | Purpose |
|---|---|
| `ec11_debounce_test.ino` | `setup()` + `loop()` — all orchestration, no decoding logic |
| `Config.h` | All pin assignments and timing constants |
| `State.{h,cpp}` | Shared counters and `lastAction`; `recordRotation()` / `recordClick()` |
| `LedFx.{h,cpp}` | FastLED external strip + onboard LED flash management |
| `Display.{h,cpp}` | SSD1306 detection, I2C scan, splash, redraw |
| `SerialOut.{h,cpp}` | Formatted banner and event lines |
| `Encoder.h` | Common interface (`init`/`deinit`/`getPos`/`poll`) + registry |
| `Encoder.cpp` | The `ENC_METHODS[]` table and `activeEncoderIdx` |
| `EncoderPolledBux.cpp` | **Default.** Polled + 2-sample confirm → Buxtronix state machine |
| `EncoderRawBux.cpp` | ISR-driven Buxtronix with no filter — included as the "bouncy fail" demo |
| `EncoderQEM.cpp` | Classic 16-entry quadrature event matrix lookup |

To try a different debounce method right now, change `activeEncoderIdx` in `Encoder.cpp` (0 = polled Buxtronix, 1 = raw ISR, 2 = QEM) and reflash. A long-press mode cycler that switches at runtime is the next planned feature.

> **Note:** the ESP32-C3 has no hardware Pulse Counter (PCNT) peripheral, so the
> common "let the silicon decode quadrature for free" approach isn't possible on
> this board — all three methods here decode in software.

## How each method works

- **PolledBux** — samples CLK/DT every 200 µs, requires the reading to be stable across 2 samples before feeding it to a Buxtronix state machine that only emits a step on a complete quadrature cycle. Robust against bounce, ~200 µs latency.
- **RawBux** — same state machine, but driven by CHANGE interrupts on both pins with no filter. Demonstrates how badly a noisy EC11 wrecks an unfiltered decoder: invalid mid-bounce transitions reset the machine and most steps are lost.
- **QEM** — pure lookup-table decoder, increments a 4×-resolution counter on each valid quadrature transition. Tiny and easy to read, but counts spurious bounces as steps.

The push switch is debounced separately with a 30 ms time-window filter.

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

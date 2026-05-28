# EC11 Debounce Test

Polled, software-debounced EC11 rotary encoder driver for the **Tenstar Robot ESP32-C3 Super-Mini**, with a mirrored external WS2812 LED for visual feedback.

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
| OLED SDA (optional) | `GPIO5` |
| OLED SCL (optional) | `GPIO6` |
| OLED VCC (optional) | `3.3V` |
| OLED GND (optional) | `GND` |

The OLED is an **SSD1306 128×64** over I2C. It's auto-detected at boot — the sketch runs fine without one and prints the same event stream to Serial regardless.

A 20-detent EC11 is assumed. One detent = one step.

## Behaviour

- Right turn → external LED flashes **red**, onboard blinks
- Left turn → external LED flashes **blue**, onboard blinks
- Click → external LED flashes **green**, onboard blinks

If an OLED is attached, the display shows the current step count, last action, and CW / CCW / click totals.

Serial monitor (115200 baud) prints one line per event:

```
--> RIGHT - STEP 01
--> RIGHT - STEP 02
<-- LEFT  - STEP 03
(*) CLICK - STEP 04
```

## How the debounce works

The encoder pins are polled every 200 µs. A new pin reading must be confirmed across two consecutive samples before it's fed to a Buxtronix full-step quadrature state machine. Bounce shorter than the sample period is rejected; only complete CW or CCW cycles emit a step.

The push switch is debounced separately with a 30 ms time-window filter.

## Building

Open `ec11_debounce_test/ec11_debounce_test.ino` in the Arduino IDE.

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

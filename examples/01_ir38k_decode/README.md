# Example 01 - Midea 38 kHz IR Control Signal Decoder (4-Wire Controllers)

Older Midea wired controllers are **4-wire**: the control board sends a **38 kHz modulated infrared control signal** to the indoor unit over the wiring, and the controller itself also receives the handheld remote. This sketch passively taps that signal and decodes the **48-bit Midea infrared frame**, so the commanded AC state (power / mode / set temperature / fan speed) can be monitored.

The same sketch also works as a plain IR remote sniffer: point any Midea AC remote at the receiver and watch the frames.

## Hardware

| Item | Notes |
| :--- | :--- |
| ESP8266 (NodeMCU) | any ESP8266 board |
| 38 kHz demodulating IR receiver (TSOP1838 / VS1838B or similar) | OUT is active low |

## Wiring

| Signal | ESP8266 pin |
| :--- | :--- |
| IR receiver OUT | D6 (GPIO12), `INPUT_PULLUP` |
| IR receiver VCC | 3.3 V |
| IR receiver GND | GND |

## Signal Timing

| Part | Timing |
| :--- | :--- |
| Header | 4.48 ms mark + 4.48 ms space |
| Bit "1" | 0.56 ms mark + 1.68 ms space |
| Bit "0" | 0.56 ms mark + 0.56 ms space |
| Repeat | every command is transmitted twice (two identical 48-bit frames) |

## Frame Layout (as captured, 6 bytes)

| Byte | Content |
| :--- | :--- |
| [0] | `0xB2` fixed header |
| [1] | `~[0]` |
| [2] | fan code << 5 \| 0x1F (`0x7B` when power off) |
| [3] | `~[2]` |
| [4] | temp code << 4 \| mode << 2 (`0xE0` when power off) |
| [5] | `~[4]` |

## Serial Output (115200 baud)

```
IR 1: B2 6B BF 40 D0 2F
IR 2: B2 6B BF 40 D0 2F
AC : [ON] [Mode:Cool] [Set:26C] [Fan:Auto]
IR 1: B2 94 7B E0 1F 5E
IR 2: B2 94 7B E0 1F 5E
AC : [OFF]
```

## Build & Upload

- **Arduino IDE**: install the ESP8266 core, open `01_ir38k_decode.ino`, select your board and upload.
- **PlatformIO**: `pio run -t upload` in this folder (see `platformio.ini`).

## 中文说明

美的早年（4 线制）线控器：控制板通过电线向内机发送 **38kHz 红外控制信号**，控制板本身也接收遥控器信号。本例程被动监听该信号，解码出 **48bit 美的红外帧**，从而获知被下达的空调状态（开关/模式/设定温度/风速）。把它对准任意美的空调遥控器，也可以当作红外 sniffing 工具使用。

- 接线：38kHz 一体化红外接收头（如 TSOP1838/VS1838B）OUT → D6(GPIO12)，上拉输入，低电平有效；VCC → 3.3V；GND → GND。
- 时序：帧头 4.48ms 低 + 4.48ms 高；数据位长空 = 1（1.68ms），短空 = 0（0.56ms）；每条命令连续发送两遍（两个相同的 48bit 帧）。
- 帧格式：`B2 | ~B2 | 风速<<5|0x1F | ~风 | 温度码<<4|模式<<2 | ~温`，关机为固定 `0x7B / 0xE0`。
- 串口 115200 输出原始帧和解码结果。

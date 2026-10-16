# Example 01 - Midea 38 kHz IR Control Signal Decoder (4-Wire Controllers)

Older Midea wired controllers are **4-wire**: the control board sends a **38 kHz modulated infrared control signal** to the indoor unit over the wiring, and the controller itself also receives the handheld remote. This sketch passively taps the **raw drive signal of that infrared link -- the signal that still carries the 38 kHz carrier** -- and decodes the **48-bit Midea infrared frame**, so the commanded AC state (power / mode / set temperature / fan speed) can be monitored.

> **The input is the carrier signal, not a demodulated one.**
> `IR_RX_PIN` expects the electrical signal that drives the IR emitter / runs down the control wire, with the **38 kHz carrier still on it**: a *mark* is a burst of 38 kHz square wave, a *space* is the idle DC level between bursts.
> Do **not** feed this pin from a demodulating IR receiver module (TSOP1838 / VS1838B or similar). Such a module already strips the carrier and outputs a clean baseband mark/space waveform, which this decoder is not written for.
> Here the carrier is removed in software instead: inside a burst the pin keeps toggling every ~26 us, so **any edge interval shorter than 100 us counts as "still inside the current mark"** (its start is latched once); a longer interval closes the mark and gives the space length. That is a software envelope detector -- which is what makes a plain GPIO pin enough to read the signal.

## Hardware

| Item | Notes |
| :--- | :--- |
| ESP8266 (NodeMCU) | any ESP8266 board |
| IR signal line tap | the carrier-carrying drive signal, active low (burst = low, idle = high) |

## Wiring

| Signal | ESP8266 pin |
| :--- | :--- |
| IR signal line (carrier present) | D6 (GPIO12), `INPUT_PULLUP` |
| GND | shared with the tapped circuit |

## Signal Timing

Measured on the envelope of the drive signal (burst length = mark, gap = space):

| Part | Timing |
| :--- | :--- |
| Carrier inside every mark | 38 kHz |
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

美的早年（4 线制）线控器：控制板通过电线向内机发送 **38kHz 红外控制信号**，控制板本身也接收遥控器信号。本例程被动监听这条红外链路的**原始驱动信号——仍携带着 38kHz 载波的那个信号**，解码出 **48bit 美的红外帧**，从而获知被下达的空调状态（开关/模式/设定温度/风速）。

> **输入的是带载波的原始信号，不是解调后的波形。**
> `IR_RX_PIN` 接的是驱动红外发射管、或沿控制线传输的那路电信号，**38kHz 载波还在上面**：*mark* 是一段 38kHz 方波脉冲串，*space* 是脉冲串之间的空闲直流电平。
> **不要**把一体化红外接收头（TSOP1838/VS1838B 等）的 OUT 接到这个引脚——那类模块已经把载波剥掉了，输出的是干净的基带 mark/space 波形，本解码器不是为它写的。
> 这里的载波是**在软件里剥掉的**：脉冲串内部，引脚每 ~26µs 就翻转一次，因此**任何短于 100µs 的边沿间隔都视为“还在同一个 mark 内”**（只在开始处锁存一次起始时刻）；一旦出现更长的间隔，就意味着这个 mark 结束、同时给出 space 长度。这就是软件包络检波——正因如此，一个普通 GPIO 就足以读取该信号。

- 接线：带 38kHz 载波的红外信号线 → D6(GPIO12)，上拉输入，低电平有效（有脉冲串时为低，空闲为高）；GND 与被测电路共地。
- 时序（取驱动信号的包络，脉冲串长度 = mark，间隔 = space）：每个 mark 内部的载波 38kHz；帧头 4.48ms mark + 4.48ms space；数据位长空 = 1（1.68ms），短空 = 0（0.56ms）；每条命令连续发送两遍（两个相同的 48bit 帧）。
- 帧格式：`B2 | ~B2 | 风速<<5|0x1F | ~风 | 温度码<<4|模式<<2 | ~温`，关机为固定 `0x7B / 0xE0`。
- 串口 115200 输出原始帧和解码结果。

# Example 02 - Midea IR Control Sender (48-bit Midea AC Frame @ 38 kHz)

Sends the classic Midea air conditioner infrared command - the same frame the wired controller / handheld remote puts on the 4-wire link - so the AC can be controlled without touching the panel. The frame format matches the one decoded in [Example 01](../01_ir38k_decode/).

The demo loop cycles through a few example commands every 10 seconds: ON + Cool 26C (high fan) → ON + Heat 24C (auto fan) → ON + Fan → OFF.

## Hardware

| Item | Notes |
| :--- | :--- |
| ESP8266 (NodeMCU) | any ESP8266 board |
| IR LED (940 nm) | on D2 (GPIO4) by default, adjust `IR_LED_PIN` to match your board |

## Wiring

Direct drive works at close range: `GPIO → 100..150 Ω → IR LED → GND`. Use a driver transistor (NPN / MOSFET) for longer range.

## Frame Layout (48 bits, 6 bytes)

| Byte | Content |
| :--- | :--- |
| [0] | `0xB2` fixed header |
| [1] | `~[0]` |
| [2] | fan code << 5 \| 0x1F (`0x7B` when power off) |
| [3] | `~[2]` |
| [4] | temp code << 4 \| mode << 2 (`0xE0` when power off) |
| [5] | `~[4]` |

Waveform (38 kHz carrier, ~50% duty): header 4.48 ms mark + 4.48 ms space; bit "1" 0.56 + 1.68 ms; bit "0" 0.56 + 0.56 ms; trailer 0.56 ms mark + 5.6 ms gap. The whole frame is transmitted twice per command, like the remote does.

## API

```cpp
// power: on/off, mode: AC_MODE_COOL/DRY/AUTO/HEAT/FAN
// fan:   AC_FAN_HIGH/MED/LOW/AUTO, tempC: 17..30 (ignored in FAN mode)
MideaEncode(true, AC_MODE_COOL, AC_FAN_HIGH, 26);
MideaIrSendFrame();   // sends the frame twice
```

## Build & Upload

- **Arduino IDE**: install the ESP8266 core, open `02_ir_send.ino`, select your board and upload.
- **PlatformIO**: `pio run -t upload` in this folder (see `platformio.ini`).

## 中文说明

用红外发射管向空调发送美的经典 48bit 红外命令（与线控器/遥控器同格式，即例程 01 解码的格式），不碰面板即可控制空调。例程默认每 10 秒轮发一条演示命令：开机+制冷 26℃（高风）→ 制热 24℃（自动风）→ 送风 → 关机。

- 接线：红外发射管接 D2(GPIO4)（用 `IR_LED_PIN` 修改引脚），近距离直驱即可（GPIO → 100~150Ω → 发射管 → GND），远距离建议加三极管驱动。
- API：`MideaEncode(开机, 模式, 风速, 温度)` + `MideaIrSendFrame()`（自动连发两帧）。
- 时序：帧头 4.48ms 低 + 4.48ms 高；位 1 = 0.56+1.68ms；位 0 = 0.56+0.56ms；帧尾 0.56ms 低 + 5.6ms 间隔。
- 串口 115200 打印每次发送的命令和原始帧。

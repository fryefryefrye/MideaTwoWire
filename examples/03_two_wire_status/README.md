# Example 03 - Two-Wire Bus Status Parser (X1/X2, This Project)

Parses the **polarity-free two-wire bus** between the wired controller and the indoor unit of newer Midea central air conditioners: power is drawn from the bus (bridge rectifier + isolating inductor + DC-DC, see the main [README](../../README.md)), and the demodulated 4800 bps UART signal is fed into this sketch, which decodes the XYE protocol frames and prints the AC working status.

This is the firmware that runs on the adapter board hidden inside the controller (see main README, section "Field Deployment").

## Hardware

| Item | Notes |
| :--- | :--- |
| ESP8266 (NodeMCU) | any ESP8266 board |
| Analog front end | the demodulation circuit from the main README (coupling capacitor → bias → envelope detector → LM393 comparator) |

## Wiring

| Signal | ESP8266 pin |
| :--- | :--- |
| Comparator output (demodulated bus UART) | D6 (GPIO12), 4800 baud software serial |

## Decoding

- Full frame validation: header `0xAA`, tail `0x55 0xFE`, length and CRC16 (CRC-16/MODBUS)
- Room temperature from the `AA 20 F8 ...` frame (0.1 °C resolution)
- AC status from the `AA 23 F8 ... 65 ...` frame: power, mode, set temperature, fan speed

Details and field tables: see the main [README](../../README.md), sections 4.1–4.5.

## Serial Output (115200 baud)

```
TW UART [16B]: AA 23 F8 01 80 65 0A 42 80 54 00 00 00 00 00 XX XX
TW AC: [ON] [Cool] [Set:27C] [AutoFan]
TW Room: 23.5C (raw=235, 0.1C)
```

## Build & Upload

- **Arduino IDE**: install the ESP8266 core, open `03_two_wire_status.ino`, select your board and upload.
- **PlatformIO**: `pio run -t upload` in this folder (see `platformio.ini`).

## 中文说明

解析新一代美的中央空调线控器与内机之间的**无极性两线总线**：供电从总线取（桥堆整流 + 电感隔离 + DC-DC，见主 README 第 3 节），解调后的 4800bps 串口信号接入本例程，解析 XYE 协议帧并打印空调工作状态。这也是塞进线控器缝隙的适配板（主 README「实机部署」一节）上运行的固件。

- 接线：比较器输出（解调后的总线串口）→ D6(GPIO12)，软串口 4800 波特。
- 解码内容：帧头 `0xAA` / 帧尾 `0x55 0xFE` / 长度 / CRC16 全帧校验；`AA 20 F8 ...` 帧给室温（0.1℃ 分辨率）；`AA 23 F8 ... 65 ...` 帧给开关/模式/设定温度/风速。
- 字段表见主 README 4.1–4.5 节，串口 115200 输出。

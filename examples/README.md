# Examples

Minimal, self-contained example sketches (ESP8266 / Arduino) for the Midea wired controller buses. Each folder is a plain Arduino sketch (`.ino` in the folder root) that can be opened directly in the Arduino IDE, and also builds with PlatformIO (`pio run -t upload` in the folder).

| Example | Bus | What it does |
| :--- | :--- | :--- |
| [01_ir38k_decode](01_ir38k_decode/) | 4-wire (38 kHz IR signal) | Decodes the 48-bit Midea IR control frame sent by the wired controller / remote: power, mode, set temperature, fan speed |
| [02_ir_send](02_ir_send/) | IR output (both generations) | Sends AC control commands with an IR LED (the same 48-bit frame format) |
| [03_two_wire_status](03_two_wire_status/) | 2-wire polarity-free X1/X2 bus (this project) | Decodes the XYE protocol frames on the two-wire bus: status, set temperature, fan speed and room temperature |

All three use the same input/output pin convention (`D6` = signal input, `D2` = IR LED output), so they can be combined on one board — which is exactly what the deployed adapter boards do: status monitoring from the wiring plus IR control output, hidden inside the controller enclosure.

Hardware background, circuit design and protocol details: see the main [README](../README.md).

## 中文说明

面向美的线控器总线的最简示例代码（ESP8266 / Arduino）。每个目录都是普通 Arduino 工程（`.ino` 在目录根下），可直接用 Arduino IDE 打开，也可用 PlatformIO 在目录内 `pio run -t upload` 编译上传。

| 例程 | 总线 | 功能 |
| :--- | :--- | :--- |
| [01_ir38k_decode](01_ir38k_decode/) | 4 线制（38kHz 红外信号） | 解码线控器/遥控器发出的 48bit 美的红外控制帧：开关、模式、设定温度、风速 |
| [02_ir_send](02_ir_send/) | 红外输出（两代机型通用） | 用红外发射管发送空调控制命令（同一 48bit 帧格式） |
| [03_two_wire_status](03_two_wire_status/) | 2 线制无极性 X1/X2 总线（本项目） | 解码两线总线上的 XYE 协议帧：状态、设定温度、风速、室温 |

三个例程统一引脚约定（`D6` = 信号输入，`D2` = 红外发射管），可以合并到同一块板上——实际部署的适配板正是这样做的：总线状态监听 + 红外控制输出，整体塞进线控器缝隙。硬件电路与协议细节见主 [README](../README.zh-CN.md)。

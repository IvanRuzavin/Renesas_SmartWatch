# R7FA4M2 Multi-Click Smartwatch Example

This project was done using [NECTOStudio IDE](https://www.mikroe.com/necto)

This bare-metal example turns an [Clicker 4 for R7FA4M2A](https://www.mikroe.com/blog/clicker-4-for-r7fa4m2ad3cfp) board into a small smartwatch-style dashboard. It combines a real-time clock [RTC 14 Click Board](https://www.mikroe.com/rtc-14-click), environmental sensing [Temp&Hum 27 CLick Board](https://www.mikroe.com/temphum-27-click), a graphical [IPS 2 Display Click Board](https://www.mikroe.com/ips-display-2-click), a stopwatch, physical interrupt controls, and [BLE Tiny Click Board](https://www.mikroe.com/ble-tiny-click) to control the board through [Renesas CodeLess/SmartConsole](https://lpccs-docs.renesas.com/UM-140-DA145x-CodeLess/smartconsole.html).

## Features

- Clock dashboard with `HH:MM AM/PM`, weekday and date.
- Temperature and humidity values with compact graphical icons.
- BLE connected/disconnected indicator on both screens.
- Millisecond stopwatch with start, pause, continue, reset and background operation.
- Physical controls through P304/IRQ9 and P111/IRQ4.
- Equivalent remote controls through BLE TINY and SmartConsole.
- RTC date/time synchronization through a pipe-separated CodeLess memory command.
- Cooperative foreground loop; ISRs only acknowledge hardware and update small state flags/counters.

## Hardware and peripheral assignment

| mikroBUS socket | Click board | MCU peripheral | Main pins | Purpose |
|---|---|---|---|---|
| 1 | BLE TINY Click | SCI3 UART, 57,600 baud | P408 RX, P409 TX, P211 RST | CodeLess connection status, RTC synchronization and remote stopwatch controls |
| 2 | IPS Display 2 Click | SCI0 simple SPI, 5 MHz | P410 MISO, P411 MOSI, P412 SCK, P708 CS, P415 DC, P210 RST, P001 backlight | 240 × 240 smartwatch UI |
| 3 | RTC 14 Click | IIC0, 100 kHz | P400 SCL, P401 SDA, P610 EIN | Date and time source |
| 4 | Temp&Hum 27 Click | IIC0, 100 kHz | P400 SCL, P401 SDA, P105 enable, P101 alert | Temperature and relative-humidity measurements |

RTC 14 and Temp&Hum 27 intentionally share IIC0. Their 7-bit addresses are different (`0x6F` and `0x54`), so both devices can use P400/P401 simultaneously.

## Interrupt allocation

| Source | Event | ICU/NVIC slot | Priority | Role |
|---|---:|---:|---:|---|
| AGT0 underflow | `0x040` | 15 | 12 | 1 ms system tick and stopwatch timebase |
| P304 / IRQ9 | `0x00A` | 16 | 6 | B3: stopwatch screen, reset and back |
| P111 / IRQ4 | `0x005` | 17 | 6 | B4: start, pause and continue |
| SCI3 RXI | `0x192` | 18 | 5 | Store BLE UART bytes in the receive ring buffer |
| SCI3 ERI | `0x195` | 19 | 5 | Clear SCI errors and preserve a pending byte |

The active vector table is copied to aligned RAM and only the required entries are replaced. This preserves the UART vectors already supplied by the surrounding NECTO project.

## Screen and stopwatch behavior

### B3 — P304/IRQ9 or `STOPWATCH|B3`

- From the watch screen: open the stopwatch at its current value and state.
- From a nonzero stopwatch value: reset to `00:00:00:000` and stop.
- From a zero stopwatch value: return to the watch screen.

### B4 — P111/IRQ4 or `STOPWATCH|B4`

- Ready at zero: start.
- Running: pause.
- Paused: continue from the preserved value.
- On the watch screen: no stopwatch state change.

### BLE B2 — `STOPWATCH|B2`

B2 switches to the watch dashboard without pausing or resetting the stopwatch. When B3 is triggered later, the stopwatch reopens with the value accumulated in the background.

## SmartConsole commands

SmartConsole stores commands in CodeLess memory slot 0. The firmware reads the slot every 200 ms while connected and replaces a successfully processed command with `EMPTY`.

### Set RTC date and time

```text
AT+MEM=0,RTC|YYYY|MM|DD|HH|MM|SS|WEEKDAY
```

Example:

```text
AT+MEM=0,RTC|2026|07|13|15|57|00|1
```

`HH` is a 24-hour value. Weekday numbering is `0 = Sunday` through `6 = Saturday`.

### Show the watch without stopping the stopwatch

```text
AT+MEM=0,STOPWATCH|B2
```

### Trigger B3

```text
AT+MEM=0,STOPWATCH|B3
```

### Trigger B4

```text
AT+MEM=0,STOPWATCH|B4
```

When SmartConsole's **AT+ prefix** option is enabled, enter only the part beginning with `MEM=...`. SmartConsole automatically converts local commands to the remote `ATr...` form.

## Demo: control through pin interrupts

The first demo shows the physical-control path. P304 generates IRQ9 for B3 actions, while P111 generates IRQ4 for B4 actions. Both inputs use internal pull-ups, falling-edge detection and 200 ms software debounce. AGT0 continues to maintain the stopwatch independently of display rendering.

![Controlling the example through pin interrupts](./pin_interrupt_control.gif)

## Demo: control through Bluetooth

The second demo shows the same state machine driven remotely through BLE TINY. SmartConsole writes B2, B3, B4 or RTC payloads to CodeLess memory slot 0. SCI3 interrupts collect the UART response, and the foreground parser applies the command without performing display or I2C work inside an ISR.

![Controlling the example through Bluetooth](./bluetooth_control.gif)

## Project structure

```text
.
├── README.md
├── Doxyfile
├── include
│   ├── app_config.h
│   ├── app_controller.h
│   ├── app_interrupts.h
│   ├── app_types.h
│   ├── bletiny.h
│   ├── iic0_bus.h
│   ├── ips2_display.h
│   ├── project_platform.h
│   ├── ra4m2_baremetal.h
│   ├── rtc14.h
│   └── temphum27.h
├── src
│   ├── app_controller.c
│   ├── app_interrupts.c
│   ├── bletiny.c
│   ├── iic0_bus.c
│   ├── ips2_display.c
│   ├── main.c
│   ├── rtc14.c
│   └── temphum27.c
```

## Module responsibilities

- `app_controller`: owns visible screen, stopwatch state, pending actions and foreground scheduling.
- `app_interrupts`: installs RAM vectors and configures AGT0, IRQ9 and IRQ4.
- `iic0_bus`: shared polling IIC0 master implementation.
- `rtc14`: RTC validation, BCD conversion, read/write and logging.
- `temphum27`: sensor reset, measurement, CRC and fixed-point conversion.
- `ips2_display`: SCI0 SPI transport, display initialization, drawing primitives and UI caching.
- `bletiny`: SCI3 UART, ring buffer, CodeLess parsing, BLE state and SmartConsole commands.

## Integration notes

1. Add every file under `src/` to the NECTO project and add `include/` to the compiler include path.
2. Keep the project's existing `mcu.h`, `delays.h`, startup code and linker script.
3. The project must provide `SYSTEM_GetClocksFrequency()` and `printf_me()`.
4. Do not add another owner for SCI0, SCI3, IIC0 or AGT0.
5. The example writes the configured default RTC date/time at every reset. Remove the `rtc14_set_default_datetime()` call in `app_controller_init()` when RTC battery-backed time must survive MCU resets.
6. Long I2C and display operations run only in the foreground. The interrupt handlers remain short.

## Documentation

All public declarations have Doxygen-style comments.

Generated HTML documentation is written to `docs/html/`.

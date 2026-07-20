# R7FA4M2 Multi-Click Smartwatch Example

This project was done using [NECTOStudio IDE](https://www.mikroe.com/necto).

This bare-metal example turns a [Clicker 4 for R7FA4M2A](https://www.mikroe.com/blog/clicker-4-for-r7fa4m2ad3cfp) board into a small smartwatch-style demo. It combines a real-time clock using [RTC 14 Click](https://www.mikroe.com/rtc-14-click), environmental sensing through [Temp&Hum 27 Click](https://www.mikroe.com/temphum-27-click), graphics on [IPS Display 2 Click](https://www.mikroe.com/ips-display-2-click), stopwatch control through push buttons, and remote BLE control using [BLE Tiny Click](https://www.mikroe.com/ble-tiny-click) with [Renesas CodeLess / SmartConsole](https://lpccs-docs.renesas.com/UM-140-DA145x-CodeLess/smartconsole.html).

## Visualization

- The **smartwatch dashboard** on the first screen shows:
  - `HH:MM` time with AM/PM or 24-hour mode,
  - temperature and humidity summary cards,
  - day of the week,
  - full date.
- The dedicated **Environment screen** keeps the animated thermometer and filling humidity droplet.
- The dedicated **Time of Day screen** now uses the title **"TIME OF DAY"**.
- The sun and moon follow a **lower trajectory** that remains inside the visible sky scene instead of approaching the header.
- The sun/moon position is recalculated from RTC time and changes once per minute.
- Physical B3 now cycles through four screens, while BLE still uses separate trigger numbers and reserves B3 for stopwatch stop/reset.
- An optional compile-time demonstration mode can generate the same B3 action automatically every 15 seconds.
- Missing source-level Doxygen comments have been added, and a ready-to-use `Doxyfile` is included.

## Features

- Four-screen smartwatch UI:
  1. **Smartwatch Dashboard** — time, weekday, date, temperature and humidity summary.
  2. **Environment** — animated temperature and humidity widgets.
  3. **Time of Day** — RTC time with lower sun/moon sky trajectory.
  4. **Stopwatch** — start, pause, continue, stop and reset.
- Animated thermometer level and temperature-dependent color.
- Animated humidity droplet with water level based on relative humidity.
- RTC-based day/night visualization with transparent sun/moon rendering.
- Sun/moon position updated once per RTC minute.
- BLE connected/disconnected status indicator on every screen.
- Millisecond stopwatch with background counting.
- Physical interrupt controls through P304/IRQ9 and P111/IRQ4.
- Separate BLE/SmartConsole commands for screen selection and stopwatch control.
- RTC date/time synchronization through a pipe-separated CodeLess memory command.
- Cooperative foreground loop; ISRs only acknowledge hardware and update small state flags/counters.
- Optional automatic screen demonstration using the existing physical B3 state-machine path.
- Doxygen comments for public APIs, internal helpers, interrupt handlers and configuration groups.

## Hardware and peripheral assignment

| mikroBUS socket | Click board | MCU peripheral | Main pins | Purpose |
|---|---|---|---|---|
| 1 | BLE TINY Click | SCI3 UART, 57,600 baud | P408 RX, P409 TX, P211 RST | CodeLess connection status, RTC synchronization, remote screen selection and remote stopwatch control |
| 2 | IPS Display 2 Click | SCI0 simple SPI, 5 MHz | P410 MISO, P411 MOSI, P412 SCK, P708 CS, P415 DC, P210 RST, P001 backlight | 240 × 240 smartwatch UI |
| 3 | RTC 14 Click | IIC0, 100 kHz | P400 SCL, P401 SDA, P610 EIN | Date and time source |
| 4 | Temp&Hum 27 Click | IIC0, 100 kHz | P400 SCL, P401 SDA, P105 enable, P101 alert | Temperature and relative-humidity measurements |

RTC 14 and Temp&Hum 27 intentionally share IIC0. Their 7-bit addresses are different (`0x6F` and `0x54`), so both devices can use P400/P401 simultaneously.

## Interrupt allocation

| Source | Event | ICU/NVIC slot | Priority | Role |
|---|---:|---:|---:|---|
| AGT0 underflow | `0x040` | 15 | 12 | 1 ms system tick and stopwatch timebase |
| P304 / IRQ9 | `0x00A` | 16 | 6 | B3: physical screen cycle or stopwatch reset |
| P111 / IRQ4 | `0x005` | 17 | 6 | B4: stopwatch start, pause and continue |
| SCI3 RXI | `0x192` | 18 | 5 | Store BLE UART bytes in the receive ring buffer |
| SCI3 ERI | `0x195` | 19 | 5 | Clear SCI errors and preserve a pending byte |

The active vector table is copied to aligned RAM and only the required entries are replaced. This preserves the UART vectors already supplied by the surrounding NECTO project.

## Screen behavior

### Screen 1 — Smartwatch Dashboard

The restored first screen contains:

- `HH:MM` time,
- AM/PM or 24-hour indicator,
- compact temperature and humidity values,
- day of the week,
- date,
- BLE status indicator.

### Screen 2 — Environment

This screen contains only environmental information:

- temperature value,
- animated thermometer,
- temperature color transition from blue to red,
- humidity value,
- animated water droplet,
- BLE status indicator.

### Screen 3 — Time of Day

This screen contains only time-of-day information:

- header text **TIME OF DAY**,
- digital RTC time,
- daytime sky with the sun or nighttime sky with the moon,
- a lowered trajectory that keeps the celestial body inside the sky area,
- sun/moon position updated once every minute,
- BLE status indicator.

### Screen 4 — Stopwatch

This screen contains only stopwatch information:

- elapsed time in hours, minutes, seconds and milliseconds,
- status (`READY`, `RUNNING`, `PAUSED`),
- B4 action hints,
- B3 behavior hint,
- BLE status indicator.

## Physical button behavior

### B3 — P304 / IRQ9

Physical B3 is now dedicated to **screen cycling** with one exception.

- From **Smartwatch Dashboard**: go to **Environment**.
- From **Environment**: go to **Time of Day**.
- From **Time of Day**: go to **Stopwatch**.
- From **Stopwatch** at `00:00:00.000`: go to **Smartwatch Dashboard**.
- From **Stopwatch** while running or while the elapsed value is nonzero: **stop and reset** the stopwatch to zero, and stay on the stopwatch screen.

This means physical B3 does **not** jump back from the stopwatch to the main screen until the stopwatch is already reset to zero.

### B4 — P111 / IRQ4

Physical B4 controls only the stopwatch while the stopwatch screen is active:

- Ready at zero: **start**.
- Running: **pause**.
- Paused: **continue**.
- On other screens: no stopwatch state change.

## Automatic 15-second screen demonstration

Automatic screen cycling is disabled by default. To enable it, edit `include/app_config.h` and change one macro:

```c
#define APP_AUTO_B3_DEMO_ENABLED            (true)
```

When enabled, the application queues the same B3 action used by the physical P304/IRQ9 button every 15 seconds. The sequence is therefore:

```text
Smartwatch Dashboard -> Environment -> Time of Day -> Stopwatch -> Smartwatch Dashboard
```

The implementation does not maintain a second navigation state machine. Automatic mode sets the normal `app_physical_b3_pending` flag, so button and automatic behavior remain identical. Every processed manual or automatic B3 action restarts the 15-second interval.

If the stopwatch is running or contains a nonzero value when an automatic B3 action occurs, the normal B3 rule still applies: the stopwatch is stopped and reset first, and the screen remains on the stopwatch until the following B3 action.

To return to manual-only operation, set the macro back to `false`.

## BLE / SmartConsole behavior

BLE navigation is intentionally separated from the physical-button behavior.

- `STOPWATCH|B1` → show **Smartwatch Dashboard**.
- `STOPWATCH|B2` → show **Environment**.
- `STOPWATCH|B3` → **stop and reset** the stopwatch and show the stopwatch screen.
- `STOPWATCH|B4` → show the **Stopwatch** screen and toggle start/pause/continue.
- `STOPWATCH|B5` → show **Time of Day**.

This keeps **BLE B3** dedicated to stopping/resetting the stopwatch, while physical B3 remains the screen-cycle button.

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

### Show the Smartwatch Dashboard

```text
AT+MEM=0,STOPWATCH|B1
```

### Show the Environment screen

```text
AT+MEM=0,STOPWATCH|B2
```

### Show the Time of Day screen

```text
AT+MEM=0,STOPWATCH|B5
```

### Stop and reset the stopwatch

```text
AT+MEM=0,STOPWATCH|B3
```

### Show or control the stopwatch

```text
AT+MEM=0,STOPWATCH|B4
```

When SmartConsole's **AT+ prefix** option is enabled, enter only the part beginning with `MEM=...`. SmartConsole automatically converts local commands to the remote `ATr...` form.

## Demo: physical screen cycling

The first GIF shows the local B3/B4 control path and the four-screen navigation. P304 generates IRQ9 for B3 actions, while P111 generates IRQ4 for B4 actions. When `APP_AUTO_B3_DEMO_ENABLED` is `true`, the same sequence can be demonstrated without pressing B3 because the firmware queues B3 every 15 seconds.

![Controlling and cycling the smartwatch screens](./pin_interrupt_control.gif)

## Demo: control through Bluetooth

The second GIF shows remote control through BLE Tiny Click and SmartConsole. BLE uses dedicated screen commands, while `STOPWATCH|B3` remains reserved for stopping and resetting the stopwatch.

![Controlling the example through Bluetooth](./bluetooth_control.gif)

## Project structure

```text
.
├── README.md
├── CMakeLists.txt
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
└── src
    ├── app_controller.c
    ├── app_interrupts.c
    ├── bletiny.c
    ├── iic0_bus.c
    ├── ips2_display.c
    ├── main.c
    ├── rtc14.c
    └── temphum27.c
```

## Module responsibilities

- `app_controller`: owns visible screen, stopwatch state, pending actions and foreground scheduling.
- `app_interrupts`: installs RAM vectors and configures AGT0, IRQ9 and IRQ4.
- `iic0_bus`: shared polling IIC0 master implementation.
- `rtc14`: RTC validation, BCD conversion, read/write and logging.
- `temphum27`: sensor reset, measurement, CRC and fixed-point conversion.
- `ips2_display`: SCI0 SPI transport, display initialization, drawing primitives, caching and all four smartwatch UI screens.
- `bletiny`: SCI3 UART, ring buffer, CodeLess parsing, BLE state and SmartConsole commands.

## Integration notes

1. Add every file under `src/` to the NECTO project and add `include/` to the compiler include path.
2. Keep the project's existing `mcu.h`, `delays.h`, startup code and linker script.
3. The project must provide `SYSTEM_GetClocksFrequency()` and `printf_me()`.
4. Do not add another owner for SCI0, SCI3, IIC0 or AGT0.
5. The example writes the configured default RTC date/time at every reset. Remove the `rtc14_set_default_datetime()` call in `app_controller_init()` when RTC battery-backed time must survive MCU resets.
6. Long I2C and display operations run only in the foreground. The interrupt handlers remain short.

## Documentation

The project includes Doxygen-style comments for public declarations, internal helper functions, interrupt handlers, shared data types and configuration groups. Static functions are included in the generated output.

The generated documentation is written to:

```text
docs/html/index.html
```

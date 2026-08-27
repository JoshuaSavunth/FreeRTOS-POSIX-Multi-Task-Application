# FreeRTOS POSIX Multi-Task Application

This project builds on the FreeRTOS POSIX demo and turns it into a small simulated embedded system running on Linux. It uses multiple FreeRTOS tasks that communicate through queues, structured messages, synchronization primitives, and software timers.

## Features

* **Sensor task** — generates simulated temperature readings.
* **Processing task** — classifies measurements.
* **Worker task** — performs calculations.
* **Logger task** — handles mutex-protected output.
* **Monitor task** — monitors system status.
* **Software timer** — generates periodic heartbeat events.
* **Host CLI** — sends commands and receives telemetry through POSIX FIFOs.

## Architecture

```text
                       Linux Host
                    ┌──────────────┐
                    │   Host CLI   │
                    └──────┬───────┘
                           │
                      POSIX FIFOs
                           │
                           ▼
              ┌────────────────────────┐
              │    FreeRTOS POSIX App  │
              │                        │
              │  Sensor → Queue        │
              │             ↓          │
              │         Processing     │
              │          ↙     ↘       │
              │      Logger   Monitor  │
              │                        │
              │     Software Timer     │
              └────────────────────────┘
```

## Communication

Sensor data is passed through a FreeRTOS queue using a structured message containing:

* `message_type`
* `sensor_id`
* `timestamp`
* `measurement`
* `status`

```text
Sensor → Queue → Processing → Logger / Monitor
```

The application uses FreeRTOS tasks, queues, mutexes, and software timers for task scheduling, communication, synchronization, and periodic events.

## Host Interface

The Linux host communicates with the RTOS application through two POSIX FIFOs:

```text
Host → /tmp/rtos_cmd
Host ← /tmp/rtos_telemetry
```

Supported commands:

```text
STATUS
START
STOP
RESET
```

Example telemetry:

```text
[TELEM] HEARTBEAT 3001
[TELEM] TEMP 27
[TELEM] WORKER 49
[TELEM] TIMER EVENT
```

The FIFO interface also demonstrates blocking and non-blocking behavior, `ENXIO` handling, and the difference between Linux IPC and FreeRTOS inter-task communication.

## Building and Running

### FreeRTOS Application

```bash
make clean
make
./build/posix_demo
```

### Host CLI

```bash
gcc host.c -o host
./host
```

## Project Structure

```text
FreeRTOS/
└── Demo/
    └── Posix_GCC/
        ├── main_blinky.c
        ├── main.c
        ├── console.c
        └── Makefile

host/
└── host.c
```

## What This Project Covers

* FreeRTOS tasks and scheduling
* Inter-task communication and structured messages
* Mutexes and shared resources
* Software timers
* Actor-style task design
* POSIX FIFO IPC
* Linux system programming
* C and Makefile-based development

## Goal

The goal is to take a basic FreeRTOS POSIX demo and build it into a more realistic embedded-style system, demonstrating how tasks, messaging, synchronization, timers, and Linux IPC work together.

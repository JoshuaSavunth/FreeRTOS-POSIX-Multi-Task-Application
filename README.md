# FreeRTOS POSIX Multi-Task Application

This project builds on the FreeRTOS POSIX demo and turns it into a small simulated embedded system running on Linux. It uses multiple FreeRTOS tasks that act as independent actors and communicate through queues and structured messages.

## Features

The system includes:

* Sensor task - generates simulated temperature readings.
* Processing task - classifies measurements.
* Worker task - performs calculations.
* Logger task - performs mutex-protected output.
* Monitor task - monitors overall system status.
* Software timer - generates periodic heartbeat events.
* Host-side C program - sends commands and receives telemetry through POSIX FIFOs.

## System Overview
The application runs several FreeRTOS tasks that pass data between them using queues. A sensor task generates simulated temperature readings and sends them to a processing task, which classifies each reading and passes the results to logging and monitoring tasks. A mutex prevents the logging tasks from writing over each other. A software timer sends a periodic heartbeat to confirm the system is still running. On the Linux side, a small CLI program sends commands like STATUS and STOP to the FreeRTOS app through a FIFO, and reads telemetry back through another FIFO.

## Communication

Sensor data is passed through a FreeRTOS queue using a structured message containing:

* `message_type`
* `sensor_id`
* `timestamp`
* `measurement`
* `status`

The main data flow is:

```text
Sensor → Queue → Processing → Logger / Monitor
```

The system uses standard FreeRTOS APIs such as:

* xTaskCreate()
* xQueueCreate()
* xQueueSend()
* xQueueReceive()
* Mutex APIs
* Software timer APIs

## Host Interface

The Linux host communicates with the RTOS application using two POSIX FIFOs:

```text
Host → /tmp/rtos_cmd
Host ← /tmp/rtos_telemetry
```

### Supported Commands

The host CLI supports the following commands:

```text
STATUS
START
STOP
RESET
```

### Example Telemetry

```text
[TELEM] HEARTBEAT 3001
[TELEM] TEMP 27
[TELEM] WORKER 49
[TELEM] TIMER EVENT
```

## FIFO Communication

This project also demonstrates several aspects of Linux FIFO-based IPC, including:

* Blocking FIFO behavior
* Non-blocking FIFO opens
* Handling ENXIO
* Host-side IPC
* The difference between POSIX IPC and FreeRTOS queues

The host communicates with the FreeRTOS application through Linux FIFOs, while the FreeRTOS tasks communicate internally using queues and synchronization primitives.

## Building and Running

### FreeRTOS Application

Build the FreeRTOS POSIX application with:

```bash
make
```

Then run it:

```bash
./build/posix_demo
```

### Host CLI

Build the host-side program:

```bash
gcc host.c -o host
```

Then run it:

```bash
./host
```

## Project Structure

```text
FreeRTOS/
└── Demo/
    └── Posix_GCC/
        ├── main_blinky.c
        ├── console.c
        └── Makefile

host/
└── host.c
```

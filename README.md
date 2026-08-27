# FreeRTOS-POSIX-Multi-Task-Application

This project builds on the FreeRTOS POSIX demo and turns it into a small simulated embedded system running on Linux. It uses multiple FreeRTOS tasks that act as independent actors and communicate through queues and structured messages.

The system includes:

Sensor task that generates simulated temperature readings
Processing task that classifies measurements
Worker task that performs calculations
Logger task with mutex-protected output
Monitor task for system status
Software timer for periodic heartbeat events
Host-side C program for sending commands and receiving telemetry
Architecture
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
              │  Sensor                │
              │     │                  │
              │   Queue                │
              │     ▼                  │
              │  Processing            │
              │    ┌─┴──────┐          │
              │    ▼        ▼          │
              │ Logger    Monitor      │
              │    │        │          │
              │    └── Mutex ┘          │
              │                        │
              │    Software Timer       │
              └────────────────────────┘

Communication

Sensor data is passed through a FreeRTOS queue using a structured message containing:

message_type
sensor_id
timestamp
measurement
status


The main data flow is:

Sensor → Queue → Processing → Logger / Monitor


The system uses standard FreeRTOS APIs such as xTaskCreate(), xQueueCreate(), xQueueSend(), xQueueReceive(), mutex functions, and software timer functions.

Host Interface

The host communicates with the RTOS application using two POSIX FIFOs:

Host → /tmp/rtos_cmd
Host ← /tmp/rtos_telemetry


Supported commands:

STATUS
START
STOP
RESET


Example telemetry:

[TELEM] HEARTBEAT 3001
[TELEM] TEMP 27
[TELEM] WORKER 49
[TELEM] TIMER EVENT


This part of the project also covers FIFO blocking behavior, non-blocking opens, ENXIO handling, and the difference between host-side IPC and FreeRTOS queues.

Building and Running
FreeRTOS Application
make clean
make
./build/posix_demo

Host CLI
gcc host.c -o host
./host

Project Structure
FreeRTOS/
└── Demo/
    └── Posix_GCC/
        ├── main_blinky.c
        ├── main.c
        ├── console.c
        └── Makefile

host/
└── host.c

What This Project Covers
FreeRTOS tasks and scheduling
Inter-task communication with queues
Structured message passing
Mutexes and shared resources
Software timers
Actor-style task design
POSIX FIFO IPC
Linux system programming
C and Makefile-based development
Host-to-RTOS communication

The main goal was to take a basic FreeRTOS POSIX demo and build it into a more realistic embedded-style system while learning how tasks, synchronization, messaging, and Linux IPC work together.

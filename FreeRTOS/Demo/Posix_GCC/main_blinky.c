/*
 * FreeRTOS V202212.00
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * https://www.FreeRTOS.org
 * https://github.com/FreeRTOS
 *
 */

#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>   /* rand */

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "semphr.h"

#include <unistd.h>     /* read, write */
#include <fcntl.h>      /* open, O_* flags */
#include <sys/stat.h>   /* mkfifo */
#include <string.h>     /* strlen, strcmp, strcspn, snprintf */

/* Local includes. */
#include "console.h"


/*
 * The tasks as described in the comments at the top of this file.
 */
/*static void prvQueueReceiveTask( void * pvParameters );*/
static void prvQueueSendTask( void * pvParameters );
static void prvProcessingTask( void * pvParameters );

static void prvCommandTask( void * pvParameters );

static void prvTelemetryTask( void * pvParameters );
/*
 * The callback function executed when the software timer expires.
 */
static void prvQueueSendTimerCallback( TimerHandle_t xTimerHandle );
static void prvHeartbeatTimerCallback( TimerHandle_t xTimerHandle );

static void prvSensorTask( void * pvParameters );
static void prvLoggerTask( void * pvParameters );
static void prvMonitorTask( void * pvParameters );

/* Priorities at which the tasks are created. */
#define mainQUEUE_RECEIVE_TASK_PRIORITY    ( tskIDLE_PRIORITY + 2 )
#define mainQUEUE_SEND_TASK_PRIORITY       ( tskIDLE_PRIORITY + 1 )

/* The rate at which data is sent to the queue.  The times are converted from
 * milliseconds to ticks using the pdMS_TO_TICKS() macro. */
#define mainTASK_SEND_FREQUENCY_MS         pdMS_TO_TICKS( 200UL )
#define mainTIMER_SEND_FREQUENCY_MS        pdMS_TO_TICKS( 2000UL )

/* The number of items the queue can hold at once. */
#define mainQUEUE_LENGTH                   ( 2 )

/* The values sent to the queue receive task from the queue send task and the
 * queue send software timer respectively. */
#define mainVALUE_SENT_FROM_TASK           ( 100UL )
#define mainVALUE_SENT_FROM_TIMER          ( 200UL )

#define FIFO_CMD_PATH        "/tmp/rtos_cmd"
#define FIFO_TELEM_PATH      "/tmp/rtos_telemetry"

typedef struct
{
    uint8_t message_type;   /* worker, sensor, timer, etc. */
    uint8_t sensor_id;      /* which sensor produced it */
    uint32_t timestamp;     /* tick count when generated */
    uint32_t measurement;   /* raw value (temp, computed result, etc.) */
    uint8_t status;         /* OK / WARNING / CRITICAL / ERROR */
} SensorMessage;

typedef struct
{
    char text[64];
    uint32_t value;
    uint8_t has_value;
} LogMessage;

static QueueHandle_t xLogQueue = NULL;

/* Message types */
#define MSG_TYPE_WORKER_RESULT   0
#define MSG_TYPE_TEMPERATURE     1
#define MSG_TYPE_TIMER_EVENT     2
#define MSG_TYPE_HEARTBEAT      3

/* Sensor IDs */
#define SENSOR_ID_TEMPERATURE    1

/* Status codes */
#define STATUS_OK                0
#define STATUS_WARNING           1
#define STATUS_CRITICAL          2
#define STATUS_ERROR             3

/*-----------------------------------------------------------*/

static int fdCmd = -1;
static int fdTelem = -1;

/*-----------------------------------------------------------*/

/* The queue used by both tasks. */
static QueueHandle_t xQueue = NULL;

/* A software timer that is started from the tick hook. */
static TimerHandle_t xTimer = NULL;

static SemaphoreHandle_t xConsoleMutex = NULL;

static TimerHandle_t xHeartbeatTimer = NULL;

/*-----------------------------------------------------------*/
static void prvIPCInit( void )
{
    /* Create FIFOs if they don't exist */
    mkfifo( FIFO_CMD_PATH, 0666 );
    mkfifo( FIFO_TELEM_PATH, 0666 );

    /* Open command FIFO for non-blocking read */
    fdCmd = open( FIFO_CMD_PATH, O_RDONLY | O_NONBLOCK );
    if( fdCmd < 0 )
    {
        perror("open cmd fifo");
    }

    /* Open telemetry FIFO for non-blocking write */
    fdTelem = open( FIFO_TELEM_PATH, O_WRONLY | O_NONBLOCK );
    if( fdTelem < 0 )
    {
        perror("open telemetry fifo");
    }
}

/*** SEE THE COMMENTS AT THE TOP OF THIS FILE ***/
void main_blinky( void )
{
    const TickType_t xTimerPeriod = mainTIMER_SEND_FREQUENCY_MS;

    /* Create the queue. */
    xQueue = xQueueCreate( mainQUEUE_LENGTH, sizeof( SensorMessage ) );

    if( xQueue != NULL )
    {
        /* Create the console mutex. */
        xConsoleMutex = xSemaphoreCreateMutex();

        if( xConsoleMutex != NULL )
        {
            xLogQueue = xQueueCreate( 4, sizeof( LogMessage ) );

            if( xLogQueue != NULL )
            {
                prvIPCInit();

                xTaskCreate( prvLoggerTask,
                             "Logger",
                             configMINIMAL_STACK_SIZE,
                             NULL,
                             tskIDLE_PRIORITY + 1,
                             NULL );

                xTaskCreate( prvMonitorTask,
                             "Monitor",
                             configMINIMAL_STACK_SIZE,
                             NULL,
                             tskIDLE_PRIORITY + 1,
                             NULL );

                xTaskCreate( prvProcessingTask,
                             "Proc",
                             configMINIMAL_STACK_SIZE,
                             NULL,
                             mainQUEUE_RECEIVE_TASK_PRIORITY,
                             NULL );

                xTaskCreate( prvCommandTask,
                             "Cmd",
                             configMINIMAL_STACK_SIZE,
                             NULL,
                             tskIDLE_PRIORITY + 1,
                             NULL );

                xTaskCreate( prvTelemetryTask,
                             "Telem",
                             configMINIMAL_STACK_SIZE,
                             NULL,
                             tskIDLE_PRIORITY + 1,
                             NULL );

                xTaskCreate( prvQueueSendTask,
                             "TX",
                             configMINIMAL_STACK_SIZE,
                             NULL,
                             mainQUEUE_SEND_TASK_PRIORITY,
                             NULL );

                xTaskCreate( prvSensorTask,
                             "Sensor",
                             configMINIMAL_STACK_SIZE,
                             NULL,
                             mainQUEUE_SEND_TASK_PRIORITY,
                             NULL );
            }

            /* Create heartbeat timer (auto-reload every 1000ms). */
            xHeartbeatTimer = xTimerCreate(
                "Heartbeat",
                pdMS_TO_TICKS( 1000 ), /* 1-second heartbeat */
                pdTRUE,                /* auto-reload */
                NULL,
                prvHeartbeatTimerCallback );

            if( xHeartbeatTimer != NULL )
            {
                xTimerStart( xHeartbeatTimer, 0 );
            }

            /* Create the software timer, but don't start it yet. */
            xTimer = xTimerCreate(
                "Timer",
                xTimerPeriod,
                pdTRUE,
                NULL,
                prvQueueSendTimerCallback );

            if( xTimer != NULL )
            {
                xTimerStart( xTimer, 0 );
            }

            /* Start the tasks and timer running. */
            vTaskStartScheduler();
        }
    }

    /* If all is well, the scheduler will now be running, and the following
     * line will never be reached. If the following line does execute, then
     * there was insufficient FreeRTOS heap memory available for the idle and/or
     * timer tasks to be created. See the memory management section on the
     * FreeRTOS web site for more details. */
    for( ; ; )
    {
    }
}

/*-----------------------------------------------------------*/

static void prvQueueSendTask( void * pvParameters )
{
    TickType_t xNextWakeTime;
    const TickType_t xBlockTime = mainTASK_SEND_FREQUENCY_MS;

    /* Phase 3: introduce a real computation */
    uint32_t input = 1;   /* worker input value */

    ( void ) pvParameters;

    xNextWakeTime = xTaskGetTickCount();

    for( ;; )
    {
        /* periodic wake-up */
        vTaskDelayUntil( &xNextWakeTime, xBlockTime );

        uint32_t result = input * input;   /* square the number */

        SensorMessage msg;
        msg.message_type = MSG_TYPE_WORKER_RESULT;
        msg.sensor_id    = 0; /* not a physical sensor */
        msg.timestamp    = xTaskGetTickCount();
        msg.measurement  = result;
        msg.status       = STATUS_OK;

        xQueueSend( xQueue, &msg, 0U );

        /* next input */
        input++;
    }
}

/*-----------------------------------------------------------*/

static void prvQueueSendTimerCallback( TimerHandle_t xTimerHandle )
{
    const uint32_t ulValueToSend = mainVALUE_SENT_FROM_TIMER;

    /* This is the software timer callback function.  The software timer has a
     * period of two seconds and is reset each time a key is pressed.  This
     * callback function will execute if the timer expires, which will only happen
     * if a key is not pressed for two seconds. */

    /* Avoid compiler warnings resulting from the unused parameter. */
    ( void ) xTimerHandle;

    /* Send to the queue - causing the queue receive task to unblock and
     * write out a message.  This function is called from the timer/daemon task, so
     * must not block.  Hence the block time is set to 0. */
    SensorMessage msg;
    msg.message_type = MSG_TYPE_TIMER_EVENT;
    msg.sensor_id    = 0;
    msg.timestamp    = xTaskGetTickCount();
    msg.measurement  = mainVALUE_SENT_FROM_TIMER;
    msg.status       = STATUS_OK;

    xQueueSend( xQueue, &msg, 0U );

}
/*-----------------------------------------------------------*/
static void prvHeartbeatTimerCallback( TimerHandle_t xTimerHandle )
{
    ( void ) xTimerHandle;

    SensorMessage msg;
    msg.message_type = MSG_TYPE_HEARTBEAT;
    msg.sensor_id    = 0;
    msg.timestamp    = xTaskGetTickCount();
    msg.measurement  = 0;        /* no measurement */
    msg.status       = STATUS_OK;

    /* Timer callbacks must be lightweight: only send a message. */
    xQueueSend( xQueue, &msg, 0U );
}

/*-----------------------------------------------------------*/
static void prvSensorTask( void *pvParameters )
{
    TickType_t xNextWakeTime;
    const TickType_t xSensorPeriod = pdMS_TO_TICKS( 500 ); /* 500ms sensor rate */

    ( void ) pvParameters;

    xNextWakeTime = xTaskGetTickCount();

    for( ;; )
    {
        vTaskDelayUntil( &xNextWakeTime, xSensorPeriod );

        /* Simulated temperature sensor */
        uint32_t temperature = 20 + ( rand() % 10 ); /* 20°C to 29°C */

        SensorMessage msg;
        msg.message_type = MSG_TYPE_TEMPERATURE;
        msg.sensor_id    = SENSOR_ID_TEMPERATURE;
        msg.timestamp    = xTaskGetTickCount();
        msg.measurement  = temperature;
        msg.status       = STATUS_OK;

        xQueueSend( xQueue, &msg, 0U );

    }
}

/*-----------------------------------------------------------*/
static void prvLoggerTask( void * pvParameters )
{
    LogMessage log;
    ( void ) pvParameters;

    for( ;; )
    {
        xQueueReceive( xLogQueue, &log, portMAX_DELAY );

        xSemaphoreTake( xConsoleMutex, portMAX_DELAY );
        console_print( log.text );
        if( log.has_value )
        {
            char numbuf[16];
            snprintf(numbuf, sizeof(numbuf), "%u", log.value);
            console_print(numbuf);
        }

        console_print( "\n" );
        xSemaphoreGive( xConsoleMutex );
    }
}
/*-----------------------------------------------------------*/
static void prvMonitorTask( void * pvParameters )
{
    ( void ) pvParameters;

    for( ;; )
    {
        vTaskDelay( pdMS_TO_TICKS( 1000 ) );

        UBaseType_t depth = uxQueueMessagesWaiting( xQueue );

        LogMessage log;
        snprintf( log.text, sizeof( log.text ), "[MONITOR] Main queue depth: " );
        log.value    = depth;
        log.has_value = 1;

        xQueueSend( xLogQueue, &log, 0 );
    }
}
/*-----------------------------------------------------------*/
static void prvCommandTask( void * pvParameters )
{
    (void) pvParameters;

    char buffer[64];

    for( ;; )
    {
        int n = read( fdCmd, buffer, sizeof(buffer)-1 );

        if( n > 0 )
        {
            buffer[n] = '\0';

            LogMessage log;

            if( strcmp(buffer, "STATUS") == 0 )
            {
                snprintf(log.text, sizeof(log.text), "[CMD] STATUS received\n");
                log.has_value = 0;
                xQueueSend( xLogQueue, &log, 0 );
            }
            else if( strcmp(buffer, "RESET") == 0 )
            {
                snprintf(log.text, sizeof(log.text), "[CMD] RESET received\n");
                log.has_value = 0;
                xQueueSend( xLogQueue, &log, 0 );
            }
            else
            {
                snprintf(log.text, sizeof(log.text), "[CMD] Unknown command: %s", buffer);
                log.has_value = 0;
                xQueueSend( xLogQueue, &log, 0 );
            }
        }

        vTaskDelay( pdMS_TO_TICKS( 100 ) );
    }
}
/*-----------------------------------------------------------*/
static void prvTelemetryTask( void * pvParameters )
{
    (void) pvParameters;

    for( ;; )
    {
        char msg[64];
        snprintf(msg, sizeof(msg), "HEARTBEAT %lu\n", (unsigned long)xTaskGetTickCount());

        write(fdTelem, msg, strlen(msg));

        vTaskDelay( pdMS_TO_TICKS( 1000 ) );
    }
}
/*-----------------------------------------------------------*/

static void prvProcessingTask( void * pvParameters )
{
    SensorMessage msg;
    LogMessage log;

    ( void ) pvParameters;

    for( ;; )
    {
        /* Block until a message arrives */
        xQueueReceive( xQueue, &msg, portMAX_DELAY );

        /* Processing Actor: classify and forward log messages */

        if( msg.message_type == MSG_TYPE_TEMPERATURE )
        {
            uint32_t temp = msg.measurement;

            if( temp < 25 )
            {
                snprintf( log.text, sizeof(log.text), "[TEMP] Normal: " );
            }
            else if( temp < 28 )
            {
                snprintf( log.text, sizeof(log.text), "[TEMP] Warning: " );
            }
            else
            {
                snprintf( log.text, sizeof(log.text), "[TEMP] Critical: " );
            }

            log.value     = temp;
            log.has_value = 1;
            xQueueSend( xLogQueue, &log, 0 );
        }
        else if( msg.message_type == MSG_TYPE_WORKER_RESULT )
        {
            snprintf( log.text, sizeof(log.text), "[WORKER] Computed result: " );
            log.value     = msg.measurement;
            log.has_value = 1;
            xQueueSend( xLogQueue, &log, 0 );
        }
        else if( msg.message_type == MSG_TYPE_TIMER_EVENT )
        {
            snprintf( log.text, sizeof(log.text), "[TIMER] Timer event" );
            log.has_value = 0;
            xQueueSend( xLogQueue, &log, 0 );
        }
        else if( msg.message_type == MSG_TYPE_HEARTBEAT )
        {
            snprintf( log.text, sizeof(log.text), "[HEARTBEAT] System alive at tick " );
            log.value     = msg.timestamp;
            log.has_value = 1;
            xQueueSend( xLogQueue, &log, 0 );
        }
        else
        {
            snprintf( log.text, sizeof(log.text), "[PROC] Unknown message type" );
            log.has_value = 0;
            xQueueSend( xLogQueue, &log, 0 );
        }
    }
}

/*-----------------------------------------------------------*/



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#define FIFO_CMD_PATH        "/tmp/rtos_cmd"
#define FIFO_TELEM_PATH      "/tmp/rtos_telemetry"

int main(void)
{
    /* Ensure FIFOs exist */
    mkfifo(FIFO_CMD_PATH, 0666);
    mkfifo(FIFO_TELEM_PATH, 0666);

    printf("Opening FIFOs...\n");

    int fdCmd = -1;
    int fdTelem = -1;

    while (fdCmd < 0 || fdTelem < 0)
    {
        if (fdCmd < 0)
        {
            fdCmd = open(FIFO_CMD_PATH, O_WRONLY | O_NONBLOCK);
        if (fdCmd < 0)
            perror("open cmd fifo");
        }

        if (fdTelem < 0)
        {
            fdTelem = open(FIFO_TELEM_PATH, O_RDONLY | O_NONBLOCK);
        if (fdTelem < 0)
            perror("open telemetry fifo");
        }

        usleep(100000); /* 100ms */
    }

    printf("Connected to RTOS application.\n");
    printf("Type commands: STATUS, RESET, START, STOP\n");


    char input[64];
    char telem[128];

    while (1)
    {
        /* Non-blocking telemetry read */
        int n = read(fdTelem, telem, sizeof(telem) - 1);
        if (n > 0)
        {
            telem[n] = '\0';
            printf("[RTOS] %s", telem);
        }

        /* Check for user input */
        printf("\n> ");
        fflush(stdout);

        if (fgets(input, sizeof(input), stdin) != NULL)
        {
            /* Strip newline */
            input[strcspn(input, "\n")] = '\0';

            /* Send command to RTOS */
            write(fdCmd, input, strlen(input));
            write(fdCmd, "\n", 1);
        }

        usleep(100000); /* 100ms */
    }

    close(fdCmd);
    close(fdTelem);

    return 0;
}

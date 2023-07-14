
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>

#include "uart.h"

#define MAIN_PREFIX     "main : "
#define WTASK_PREFIX    "write_task: "
#define RTASK_PREFIX    "read_task: "

#define BAUD_RATE   1500000

#define WRITE_FILE  "/dev/ttyS2"
#define READ_FILE   "/dev/ttyS4"

int read_fd  = -1;
int write_fd = -1;

#define STATE_FILE_OPENED  1
#define STATE_TASK_CREATED 2

unsigned int read_state  = 0;
unsigned int write_state = 0;

/*                                --s-ms-us */
useconds_t write_task_period_us =    500000llu;
pthread_t write_task;
pthread_t read_task;

long get_system_time_microsecond()
{
    struct timeval tv = {};
    if (0 == gettimeofday(&tv, NULL))
        return tv.tv_sec * 1e6 + tv.tv_usec;
    else
        return 0;
}

static int close_file(int fd, char *name)
{
    int err, i = 0;

    do
    {
        i++;
        err = close(fd);
        switch (err)
        {
        case -EAGAIN:
            printf(MAIN_PREFIX "%s -> EAGAIN (%d times)\n",
                   name, i);
            usleep(50); /* wait 50us */
            break;
        case 0:
            printf(MAIN_PREFIX "%s -> closed\n", name);
            break;
        default:
            printf(MAIN_PREFIX "%s -> %s\n", name,
                   strerror(errno));
            break;
        }
    } while (err == -EAGAIN && i < 10);

    return err;
}

static void cleanup_all(void)
{
    if (read_state & STATE_FILE_OPENED)
    {
        close_file(read_fd, READ_FILE " (read)");
        read_state &= ~STATE_FILE_OPENED;
    }

    if (write_state & STATE_FILE_OPENED)
    {
        close_file(write_fd, WRITE_FILE " (write)");
        write_state &= ~STATE_FILE_OPENED;
    }

    if (write_state & STATE_TASK_CREATED)
    {
        printf(MAIN_PREFIX "delete write_task\n");
        pthread_cancel(write_task);
        write_state &= ~STATE_TASK_CREATED;
    }

    if (read_state & STATE_TASK_CREATED)
    {
        printf(MAIN_PREFIX "delete read_task\n");
        pthread_cancel(read_task);
        read_state &= ~STATE_TASK_CREATED;
    }
}

static void catch_signal(int sig)
{
    cleanup_all();
    printf(MAIN_PREFIX "exit\n");
    exit(0);
}

static void* write_task_proc(void *arg)
{
    long write_time;
    ssize_t sz = sizeof(long);
    int written = 0;

    while (1)
    {
        write_time = get_system_time_microsecond();

        written = write(write_fd, &write_time, sz);
        if (written < 0)
        {
            printf(WTASK_PREFIX "error on write, %s\n",
                   strerror(errno));
            break;
        }
        else if (written != sz)
        {
            printf(WTASK_PREFIX "only %d / %zd byte transmitted\n",
                   written, sz);
            break;
        }
        else
        {
            printf(WTASK_PREFIX "write %d byte %ld\n", written, write_time);
        }

        usleep(write_task_period_us);
    }

    write_state &= ~STATE_TASK_CREATED;
    printf(WTASK_PREFIX "exit\n");

    return NULL;
}

static void* read_task_proc(void *arg)
{
    int nr = 0;
    long read_time = 0;
    long write_time = 0;
    long mini = ~0;
    long max = 0;
    long escap = 0;
    ssize_t sz = sizeof(long);
    int rd = 0;

    printf("   Nr  | write->read(us) |\n");
    printf("--------------------------\n");

    while (1)
    {
        rd = read(read_fd, &write_time, sz);
        if (rd == sz)
        {
            read_time = get_system_time_microsecond();
            printf(RTASK_PREFIX "read %d byte %ld, read_time %ld\n",
                rd, write_time, read_time);
            if (nr == 0)
            {
                nr++;
                continue;
            }
            escap = read_time - write_time;
            if (escap < mini) mini = escap;
            if (escap > max)  max = escap;
            if (nr % 100 == 0)
                printf("%6d |%16ld\n", nr, escap);
            if (nr % 10000 == 0)
            {
                printf("%6s |%16ld\n", "mini", mini);
                printf("%6s |%16ld\n", "max", max);
                printf("%6s |%16ld\n", "jitter", max - mini);
                mini = ~0;
                max = 0;
            }
            nr++;
        }
        else if (rd < 0)
        {
            printf(RTASK_PREFIX "error on read, code %s\n",
                   strerror(errno));
            break;
        }
        else
        {
            printf(RTASK_PREFIX "only %d / %zd byte received \n",
                   rd, sz);
            break;
        }
    }

    read_state &= ~STATE_TASK_CREATED;
    printf(RTASK_PREFIX "exit\n");

    return NULL;
}

int main(int argc, char *argv[])
{
    int err = 0;

    signal(SIGTERM, catch_signal);
    signal(SIGINT, catch_signal);

    /* open ttyS2 */
    write_fd = open_dev(WRITE_FILE);
    if (write_fd < 0)
        goto error;

    write_state |= STATE_FILE_OPENED;
    printf(MAIN_PREFIX "write-file opened\n");

    /* writing write-config */
    if (set_speed(write_fd, BAUD_RATE))
        goto error;
    if (set_parity(write_fd, 8, 1, 'N'))
        goto error;
    printf(MAIN_PREFIX "write-config written\n");

    /* open ttyS4 */
    read_fd = open_dev(READ_FILE);
    if (read_fd < 0)
        goto error;
    read_state |= STATE_FILE_OPENED;
    printf(MAIN_PREFIX "read-file opened\n");

    /* writing read-config */
    if (set_speed(read_fd, BAUD_RATE))
        goto error;
    if (set_parity(read_fd, 8, 1, 'N'))
        goto error;
    printf(MAIN_PREFIX "read-config written\n");

#if 1
    /* start read_task */
    printf(MAIN_PREFIX "starting read-task\n");
    err = pthread_create(&read_task, NULL, &read_task_proc, NULL);
    if (err)
    {
        printf(MAIN_PREFIX "failed to start read_task, %s\n",
               strerror(-err));
        goto error;
    }
    read_state |= STATE_TASK_CREATED;
#endif

    /* start write_task */
    printf(MAIN_PREFIX "starting write-task\n");
    err = pthread_create(&write_task, NULL, &write_task_proc, NULL);
    if (err)
    {
        printf(MAIN_PREFIX "failed to start write_task, %s\n",
               strerror(-err));
        goto error;
    }
    write_state |= STATE_TASK_CREATED;

    for (;;)
        pause();

    return 0;

error:
    cleanup_all();
    return err;
}

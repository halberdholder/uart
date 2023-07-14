
#include <stdio.h>     /*标准输入输出*/
#include <string.h>    /*标准字符串处理*/
#include <unistd.h>    /*Unix标准函数*/

#include "uart.h"

#define DEV "/dev/ttyS2" /*USB转串口0*/

int main(int argc, char const *argv[])
{
    int fd = 0;
    int nwrite = 0;
    char buff[512] = {0};
    char *dev = DEV;

    fd = open_dev(dev);
    if (fd <= 0)
        return -1;
    if (set_speed(fd, 1500000))
        return -1;
    if (set_parity(fd, 8, 1, 'N'))
        return -1;

    snprintf(buff, sizeof(buff), "helloworld");

    do
    {
        nwrite = write(fd, buff, strlen(buff));
        printf("fd:%d, nwrite:%d, buff:%s\n", fd, nwrite, buff);
    } while (1);

    close(fd);
    return 0;
}


#include <stdio.h>     /*标准输入输出*/
#include <string.h>    /*标准字符串处理*/
#include <stdlib.h>    /*标准函数库*/
#include <unistd.h>    /*Unix标准函数*/
#include <sys/types.h> /*数据类型定义，比如一些XXX_t的类型*/
#include <sys/stat.h>  /*返回值结构*/
#include <fcntl.h>     /*文件控制*/
#include <termios.h>   /*PPSIX终端控制*/
#include <errno.h>     /*错误号*/

#define DEV "/dev/ttyUSB0" /*USB转串口0*/

/*串口打开*/
int open_dev(char *Dev)
{
    int fd = 0;
    fd = open(Dev, O_RDWR | O_NOCTTY | O_NDELAY);
    if (-1 == fd)
    {
        perror("open COM error");
        return -1;
    }
    return fd;
}

/*设置串口通信速率*/
int set_speed(int fd, int speed)
{
    int index = 0;
    int status = 0;
    int speed_arr[] = {
        B1500000,
        B115200,
        B38400,
        B19200,
        B9600,
        B4800,
        B2400,
        B1200,
        B300,
    };
    int name_arr[] = {
        1500000,
        115200,
        38400,
        19200,
        9600,
        4800,
        2400,
        1200,
        300,
    };

    struct termios Opt = {0};

    tcgetattr(fd, &Opt);
    for (index = 0; index < sizeof(speed_arr) / sizeof(int); index++)
    {
        if (speed == name_arr[index])
        {
            /*
             * tcflush函数刷清(抛弃)输入缓存(终端驱动程序已接收到，但用户程序尚未读)或输出缓
             * 存(用户程序已经写，但尚未发送)。queue参数应是下列三个常数之一：TCIFLUSH刷清输入
             * 队列，TCOFLUSH刷清输出队列，TCIOFLUSH刷清输入输出队列。
             */

            /*设置前flush*/
            tcflush(fd, TCIOFLUSH);
            cfsetispeed(&Opt, speed_arr[index]);
            cfsetospeed(&Opt, speed_arr[index]);

            /*tcsetattr(串口描述符, 立即使用或者其他标示, 指向termios的指针)，通过
            tcsetattr函数把新的属性设置到串口上。*/
            status = tcsetattr(fd, TCSANOW, &Opt);
            if (0 != status)
            {
                perror("tcsetattr COM error");
                return -1;
            }

            /*设置后flush*/
            tcflush(fd, TCIOFLUSH);
            return 0;
        }
    }

    return 0;
}

/*设置数据位、停止位和校验位*/
int set_parity(int fd, int databits, int stopbits, int parity)
{
    struct termios Opt = {0};

    if (0 != tcgetattr(fd, &Opt))
    {
        perror("tcgetattr COM error");
        return -1;
    }

    /*设置数据位，取值为7或8*/
    Opt.c_cflag &= ~CSIZE;
    switch (databits)
    {
    case 7:
        Opt.c_cflag |= CS7;
        break;
    case 8:
        Opt.c_cflag |= CS8;
        break;
    default:
        fprintf(stderr, "Unsupported data size\n");
        return -1;
        break;
    }

    /*设置停止位，取值为1或2*/
    switch (stopbits)
    {
    case 1:
        Opt.c_cflag &= ~CSTOPB;
        break;
    case 2:
        Opt.c_cflag |= CSTOPB;
        break;
    default:
        fprintf(stderr, "Unsupported stop bits\n");
        return -1;
        break;
    }

    /*设置校验位，取值为E,N,O,S*/
    switch (parity)
    {
    case 'e':
    case 'E':
    {
        Opt.c_cflag |= PARENB;  /*Enable parity*/
        Opt.c_cflag &= ~PARODD; /*转换为偶效验*/
        Opt.c_iflag |= INPCK;   /*Disnable parity checking*/
    }
    break;
    case 'n':
    case 'N':
    {
        Opt.c_cflag &= ~PARENB; /*Clear parity enable*/
        Opt.c_iflag &= ~INPCK;  /*Enable parity checking*/
    }
    break;
    case 'o':
    case 'O':
    {
        Opt.c_cflag |= (PARODD | PARENB); /*设置为奇效验*/
        Opt.c_iflag |= INPCK;             /*Disnable parity checking*/
    }
    break;
    case 's': /*as no parity*/
    case 'S': /*as no parity*/
    {
        Opt.c_cflag &= ~PARENB;
        Opt.c_cflag &= ~CSTOPB;
    }
    break;
    default:
        fprintf(stderr, "Unsupported parity\n");
        return -1;
        break;
    }

    /*设置结构体输入校验位*/
    if ('n' != parity)
    {
        Opt.c_iflag |= INPCK;
    }

    /*
     * 原始模式设置
     * 如果不是开发终端之类的，串口仅用于传输数据，不需要处理数据，可将串口设置为原始模式(Raw Mode)
     * 进行通讯。
     */
    // Opt.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); /*Input*/
    // Opt.c_oflag &= ~OPOST;                          /*Output*/

    tcflush(fd, TCIFLUSH);
    Opt.c_cc[VTIME] = 150; /*设置超时15秒*/
    Opt.c_cc[VMIN] = 0;    /*更新结构体并立即执行*/
    if (0 != tcsetattr(fd, TCSANOW, &Opt))
    {
        perror("tcsetattr COM error");
        return -1;
    }

    return 0;
}

int main(int argc, char const *argv[])
{
    int fd = 0;
    int nwrite = 0;
    char buff[512] = {0};
    char *dev = DEV;

    fd = open_dev(dev);
    set_speed(fd, 19200);
    set_parity(fd, 8, 1, 'N');

    snprintf(buff, 512, "helloworld");

    do
    {
        nwrite = write(fd, buff, strlen(buff));
        printf("fd:%d, nwrite:%d, buff:%s\n", fd, nwrite, buff);
    } while (0);

    close(fd);
    return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>

#include "../src/tcs_34725.h"

int main() {
    rgb_data_t data;
    int fd = open("/dev/tcs34725", O_RDWR);

    if (fd < 0) {
        perror("Failed to open the device");
        return errno;
    }

    if (ioctl(fd, TCS_GET_COLORS, &data) == -1) {
        perror("TCS_GET_COLORS failed");
        close(fd);
        return errno;
    }

    printf("Colors read from kernel: \n");
    printf("Clear: %d\n", data.clear_data);
    printf("Read: %d\n", data.red_data);
    printf("Green: %d\n", data.green_data);
    printf("Blue: %d\n", data.blue_data);

    close(fd);
    return 0;
}
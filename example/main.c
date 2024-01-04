#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>

#include "../src/tcs_34725.h"

int main() {
    rgb_raw_data_t raw_data;
    rgb_norm_data_t rgb_norm_data;
    int fd = open("/dev/tcs34725", O_RDWR);

    // Open the char device file
    if (fd < 0) {
        perror("Failed to open the device");
        return errno;
    }

    // Execute the IO operation defined in the driver header
    if (ioctl(fd, TCS_GET_RAW_COLORS, &raw_data) == -1) {
        perror("TCS_GET_RAW_COLORS failed");
        close(fd);
        return errno;
    }

    if (ioctl(fd, TCS_GET_RGB_COLORS, &rgb_norm_data) == -1) {
        perror("TCS_GET_RGB_COLORS failed");
        close(fd);
        return errno;
    }

    printf("Raw colors read from sensor: \n");
    printf("Clear: %d\n", raw_data.clear_data);
    printf("Red: %d\n", raw_data.red_data);
    printf("Green: %d\n", raw_data.green_data);
    printf("Blue: %d\n", raw_data.blue_data);

    printf("RGB colors read from sensor: \n");
    printf("Red: %d\n", rgb_norm_data.red_data);
    printf("Green: %d\n", rgb_norm_data.green_data);
    printf("Blue: %d\n", rgb_norm_data.blue_data);

    close(fd);
    return 0;
}
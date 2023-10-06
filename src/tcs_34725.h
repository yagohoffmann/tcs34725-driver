#ifndef TCS_34725_H
#define TCS_34725_H

#include <linux/ioctl.h>

typedef struct {
    int clear_data;
    int red_data;
    int green_data;
    int blue_data;
} rgb_data_t;

#define TCS_GET_COLORS _IOR('t', 1, rgb_data_t *)

#endif // TCS_34725_H
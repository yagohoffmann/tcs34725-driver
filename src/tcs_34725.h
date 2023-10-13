#ifndef TCS_34725_H
#define TCS_34725_H

#include <linux/ioctl.h>

typedef struct {
    int clear_data;
    int red_data;
    int green_data;
    int blue_data;
} rgb_data_t;

typedef struct {
    unsigned char red_data;
    unsigned char green_data;
    unsigned char blue_data;
} rgb_norm_data_t;

#define TCS_GET_COLORS _IOR('t', 1, rgb_data_t *)

#endif // TCS_34725_H
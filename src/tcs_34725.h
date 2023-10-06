#ifndef TCS_34725_H
#define TCS_34725_H

#include <linux/ioctl.h>
#include <stdint.h>

typedef struct {
    uint16_t clear_data;
    uint16_t red_data;
    uint16_t green_data;
    uint16_t blue_data;
} rgb_data_t;

#define TCS_GET_COLORS _IOR('t', 1, rgb_data_t *)

#endif // TCS_34725_H
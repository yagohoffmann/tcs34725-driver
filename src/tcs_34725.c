/***************************************************************************//**
*  \file       main.c
*
*  \details    Simple I2C driver for TCS
*
*  \author     Yago Hoffmann
*
*
* *******************************************************************************/
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <asm/uaccess.h>

#include "tcs_34725.h"

#define FIRST_MINOR 0
#define MINOR_CNT 1

#define I2C_BUS_AVAILABLE   (          1 )          // I2C Bus available in our Raspberry Pi
#define SLAVE_DEVICE_NAME   ( "HFN_TCS34725" )      // Device and Driver Name
#define TCS_SLAVE_ADDR      (       0x29 )          // Senor Address
#define MAXIMUM_WRITE_SIZE 36

#define COMMAND_MASK  0x80
#define REGISTER_ADDRESS_ENABLE 0x00
#define REGISTER_ADDRESS_ID 0x12
#define REGISTER_ADDRESS_STATUS 0x13
#define REGISTER_ADDRESS_CDATA_LOW 0x14
#define TCS34725_ID 0x44

// I2C Adapter Structure
static struct i2c_adapter *hfn_i2c_adapter     = NULL;
// I2C Cient Structure
static struct i2c_client  *hfn_i2c_client_tcs = NULL;
static dev_t dev;
// Char device structure
static struct cdev c_dev;
static struct class *cl;

// Look-up table used to apply a gamma of 2.5 to the read data ( y = x ^ 1/2.5 )
int gamma_lut[256] = { 0, 27, 36, 43, 48, 52, 56, 60, 63, 66, 69, 72, 75, 77, 79, 82, 84, 86, 88, 90, 92, 93, 95,
                       97, 99, 100, 102, 103, 105, 106, 108, 109, 111, 112, 113, 115, 116, 117, 119, 120, 121, 122,
                       123, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 136, 137, 138, 139, 140, 141, 141,
                       142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 152, 153, 154, 155, 156, 157, 157,
                       158, 159, 160, 161, 161, 162, 163, 164, 165, 165, 166, 167, 168, 168, 169, 170, 171, 171,
                       172, 173, 173, 174, 175, 176, 176, 177, 178, 178, 179, 180, 180, 181, 182, 182, 183, 184,
                       184, 185, 186, 186, 187, 187, 188, 189, 189, 190, 191, 191, 192, 192, 193, 194, 194, 195,
                       195, 196, 197, 197, 198, 198, 199, 200, 200, 201, 201, 202, 202, 203, 204, 204, 205, 205,
                       206, 206, 207, 207, 208, 208, 209, 210, 210, 211, 211, 212, 212, 213, 213, 214, 214, 215,
                       215, 216, 216, 217, 217, 218, 218, 219, 219, 220, 220, 221, 221, 222, 222, 223, 223, 224,
                       224, 225, 225, 226, 226, 227, 227, 228, 228, 229, 229, 229, 230, 230, 231, 231, 232, 232,
                       233, 233, 234, 234, 235, 235, 235, 236, 236, 237, 237, 238, 238, 239, 239, 239, 240, 240,
                       241, 241, 242, 242, 242, 243, 243, 244, 244, 245, 245, 245, 246, 246, 247, 247, 248, 248,
                       248, 249, 249, 250, 250, 250, 251, 251, 252, 252, 252, 253, 253, 254, 254 };

static int tcs_read_registers (struct i2c_client *client, uint8_t reg_addr, uint8_t *data, uint8_t length) {
    uint8_t masked_address = reg_addr | COMMAND_MASK;
    int rv = i2c_master_send(client, &masked_address, 1);
    if (rv < 0) {
        return rv;
    }
    rv = i2c_master_recv(client, data, length);
    return rv;
}

static int tcs_write_registers (struct i2c_client *client, uint8_t reg_addr, uint8_t *data, uint8_t length) {
    uint8_t write_buffer[MAXIMUM_WRITE_SIZE];
    int i, rv;
    for (i = 0; i < length; i++) {
        write_buffer[i+1] = data[i];
    }
    write_buffer[0] = reg_addr | COMMAND_MASK;
    rv = i2c_master_send(client, write_buffer, length + 1);
    return rv;
}

static int tcs_init (struct i2c_client *client) {
    // Set PON bit
    uint8_t data = 0x01;
    int rv = tcs_write_registers(client, REGISTER_ADDRESS_ENABLE, &data, 1);
    if (rv < 0) {
        pr_err("Sensor power on failed!\n");
        return rv;
    }
    // The sensor needs a time delay between power on (PON bit) and RGBC enable (AEN bit)
    usleep_range(2400,5000);

    // Set AEN bit
    data |= 0x02;
    rv = tcs_write_registers(client, REGISTER_ADDRESS_ENABLE, &data, 1);
    if (rv < 0) {
        pr_err("RGBC enable failed!\n");
        return rv;
    }
    usleep_range(2400,5000);
    return 0;
}

static int tcs_id_read(struct i2c_client *client) {
    uint8_t read_id;
    int rv = tcs_read_registers(client, REGISTER_ADDRESS_ID, &read_id, 1);
    if (rv < 0) {
        pr_err("Error reading sensor ID!\n");
        return rv;
    }
    pr_info("ID read: 0x%x \n", read_id);
    return 0;
}

static int tcs_rgb_data_read (struct i2c_client *client, rgb_raw_data_t *data) {
    uint8_t raw_data[8];
    int rv;
    rv = tcs_read_registers(client, REGISTER_ADDRESS_CDATA_LOW, raw_data, 8);
    if (rv < 0) {
        pr_err("Error reading RGB data from sensor: %d \n", rv);
        return rv;
    }
    data->clear_data = (raw_data[1] << 8) | raw_data[0];
    data->red_data = (raw_data[3] << 8) | raw_data[2];
    data->green_data = (raw_data[5] << 8) | raw_data[4];
    data->blue_data = (raw_data[7] << 8) | raw_data[6];
    return 0;
}

static int tcs_get_normalized_data (rgb_raw_data_t *in_data, rgb_norm_data_t *out_data) {
    int tmp_red;
    int tmp_green;
    int tmp_blue;

    if (in_data == NULL || out_data == NULL) {
        return -1;
    }

    if (in_data->clear_data < 1) {
        out_data->red_data = 0;
        out_data->green_data = 0;
        out_data->blue_data = 0;
        return 0;
    }

    // Normalize color data to a value between 0 and 255 proportional to clear data 
    tmp_red = (in_data->red_data * 255) / in_data->clear_data;
    tmp_green = (in_data->green_data * 255) / in_data->clear_data;
    tmp_blue = (in_data->blue_data * 255) / in_data->clear_data;

    if (tmp_red > 255) tmp_red = 255;
    if (tmp_green > 255) tmp_green = 255;
    if (tmp_blue > 255) tmp_blue = 255;

    out_data->red_data = gamma_lut[tmp_red];
    out_data->green_data = gamma_lut[tmp_green];
    out_data->blue_data = gamma_lut[tmp_blue];

    return 0;
}

static int hfn_tcs_open(struct inode *i, struct file *f)
{
    return 0;
}
static int hfn_tcs_close(struct inode *i, struct file *f)
{
    return 0;
}

static long hfn_tcs_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
    rgb_raw_data_t rgb_raw_data;
    rgb_norm_data_t rgb_norm_data;

    switch (cmd)
    {
        case TCS_GET_RAW_COLORS:
            if (!tcs_rgb_data_read(hfn_i2c_client_tcs, &rgb_raw_data)) {
                pr_err("Error reading raw data \n");
                return -EFAULT;
            }
            if (copy_to_user((rgb_raw_data_t *)arg, &rgb_raw_data, sizeof(rgb_raw_data_t)))
            {
                pr_err("Error copying data to user space \n");
                return -EACCES;
            }
            break;
        case TCS_GET_RGB_COLORS:
            if (!tcs_rgb_data_read(hfn_i2c_client_tcs, &rgb_raw_data)) {
                pr_err("Error reading raw data \n");
                return -EFAULT;
            }
            if (!tcs_get_normalized_data(&rgb_raw_data, &rgb_norm_data)) {
                pr_err("Error converting raw data to RGB \n");
                return -EFAULT;
            }            
            if (copy_to_user((rgb_norm_data_t *)arg, &rgb_norm_data, sizeof(rgb_norm_data_t)))
            {
                pr_err("Error copying data to user space \n");
                return -EACCES;
            }
            break;
        default:
            return -EINVAL;
    }

    return 0;
}

static struct file_operations tcs_fops =
{
    .owner = THIS_MODULE,
    .open = hfn_tcs_open,
    .release = hfn_tcs_close,
    .unlocked_ioctl = hfn_tcs_ioctl
};

/*
** This function getting called when the slave has been found
** Note : This will be called only once when we load the driver.
*/
static int hfn_tcs_probe(struct i2c_client *client, const struct i2c_device_id *id) {
    int rv = tcs_id_read(client);
    if (rv < 0) return rv;
    rv = tcs_init(client);
    if (rv < 0) return rv;

    pr_info("TCS Sensor Probed!!!\n");
    return 0;
}

/*
** This function getting called when the slave has been removed
** Note : This will be called only once when we unload the driver.
*/
static void hfn_tcs_remove(struct i2c_client *client) {
    pr_info("TCS Sensor Removed!!!\n");
    return;
}

/*
** Structure that has slave device id
*/
static const struct i2c_device_id hfn_tcs_id[] = {
        { SLAVE_DEVICE_NAME, 0 },
        { }
};
MODULE_DEVICE_TABLE(i2c, hfn_tcs_id);

/*
** I2C driver Structure that has to be added to linux
*/
static struct i2c_driver hfn_tcs_driver = {
        .driver = {
            .name   = SLAVE_DEVICE_NAME,
            .owner  = THIS_MODULE,
        },
        .probe          = hfn_tcs_probe,
        .remove         = hfn_tcs_remove,
        .id_table       = hfn_tcs_id,
};

/*
** I2C Board Info strucutre
*/
static struct i2c_board_info tcs_i2c_board_info = {
        I2C_BOARD_INFO(SLAVE_DEVICE_NAME, TCS_SLAVE_ADDR)
    };

/*
** Module Init function
*/
static int __init hfn_driver_init(void) {
    int ret = -1;
    struct device *dev_ret;

    if ((ret = alloc_chrdev_region(&dev, FIRST_MINOR, MINOR_CNT, "hfn_tcs_ioctl")) < 0)
    {
        return ret;
    }

    cdev_init(&c_dev, &tcs_fops);

    if ((ret = cdev_add(&c_dev, dev, MINOR_CNT)) < 0)
    {
        return ret;
    }

    if (IS_ERR(cl = class_create(THIS_MODULE, "char")))
    {
        cdev_del(&c_dev);
        unregister_chrdev_region(dev, MINOR_CNT);
        return PTR_ERR(cl);
    }
    if (IS_ERR(dev_ret = device_create(cl, NULL, dev, NULL, "tcs34725")))
    {
        class_destroy(cl);
        cdev_del(&c_dev);
        unregister_chrdev_region(dev, MINOR_CNT);
        return PTR_ERR(dev_ret);
    }

    hfn_i2c_adapter     = i2c_get_adapter(I2C_BUS_AVAILABLE);

    if( hfn_i2c_adapter != NULL )
    {
        hfn_i2c_client_tcs = i2c_new_client_device(hfn_i2c_adapter, &tcs_i2c_board_info);

        if( hfn_i2c_client_tcs != NULL )
        {
            i2c_add_driver(&hfn_tcs_driver);
            ret = 0;
        }

        i2c_put_adapter(hfn_i2c_adapter);
    }

    pr_info("TCS Driver Added!!!\n");
    return ret;
}

/*
** Module Exit function
*/
static void __exit hfn_driver_exit(void) {
    i2c_unregister_device(hfn_i2c_client_tcs);
    i2c_del_driver(&hfn_tcs_driver);
    device_destroy(cl, dev);
    class_destroy(cl);
    cdev_del(&c_dev);
    unregister_chrdev_region(dev, MINOR_CNT);
    pr_info("TCS Driver Removed!!!\n");
}

module_init(hfn_driver_init);
module_exit(hfn_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yago Hoffmann");
MODULE_DESCRIPTION("Simple I2C driver for TCS34725 sensor");
MODULE_VERSION("1.00");

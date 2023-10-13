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

static struct i2c_adapter *hfn_i2c_adapter     = NULL;  // I2C Adapter Structure
static struct i2c_client  *hfn_i2c_client_tcs = NULL;  // I2C Cient Structure (In our case it is OLED)
static dev_t dev;
static struct cdev c_dev;
static struct class *cl;

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
    data = 0x03;
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

static int tcs_rgb_data_read (struct i2c_client *client, rgb_data_t *data) {
    uint8_t raw_data[8];
    uint8_t status;
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

static int tcs_get_normalized_data (rgb_data_t *in_data, rgb_norm_data_t *out_data) {
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
        return;
    }

    // Normalize color data
    tmp_red = (in_data->red_data * 256) / in_data->clear_data;
    tmp_green = (in_data->green_data * 256) / in_data->clear_data;
    tmp_blue = (in_data->blue_data * 256) / in_data->clear_data;



    out_data->red_data = 
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
    rgb_data_t rgb_data;

    switch (cmd)
    {
        case TCS_GET_COLORS:
            if (!tcs_rgb_data_read(hfn_i2c_client_tcs, &rgb_data)) {
                return -EFAULT;
            }
            if (copy_to_user((rgb_data_t *)arg, &rgb_data, sizeof(rgb_data_t)))
            {
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

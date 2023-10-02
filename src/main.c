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

#define I2C_BUS_AVAILABLE   (          1 )          // I2C Bus available in our Raspberry Pi
#define SLAVE_DEVICE_NAME   ( "HFN_TCS34725" )      // Device and Driver Name
#define TCS_SLAVE_ADDR      (       0x29 )          // Senor Address
#define COMMAND_MASK  0x80
#define REGISTER_ADDRESS_ID 0x12
#define TCS34725_ID 0x44

static struct i2c_adapter *hfn_i2c_adapter     = NULL;  // I2C Adapter Structure
static struct i2c_client  *hfn_i2c_client_tcs = NULL;  // I2C Cient Structure (In our case it is OLED)

static int tcs_read_registers (struct i2c_client *client, uint8_t reg_addr, uint8_t *data, uint8_t length)
{
    uint8_t masked_address = reg_addr | COMMAND_MASK;
    int rv = i2c_master_send(client, &masked_address, 1);
    if (rv < 0) return rv;
    rv = i2c_master_recv(client, data, length);
    return rv;
}

static int tcs_id_read(struct i2c_client *client)
{
    uint8_t read_id;
    int rv = tcs_read_registers(client, REGISTER_ADDRESS_ID, &read_id, 1);
    if (rv < 0) {
        pr_info("Error reading sensor ID! \n");
        return rv;
    }

    pr_info("ID read: 0x%x \n", read_id);

    return 0;
}

/*
** This function getting called when the slave has been found
** Note : This will be called only once when we load the driver.
*/
static int hfn_tcs_probe(struct i2c_client *client,
                         const struct i2c_device_id *id)
{
    int rv = tcs_id_read(client);
    pr_info("TCS Sensor Probed!!!\n");

    return rv;
}

/*
** This function getting called when the slave has been removed
** Note : This will be called only once when we unload the driver.
*/
static void hfn_tcs_remove(struct i2c_client *client)
{
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
static int __init hfn_driver_init(void)
{
    int ret = -1;
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
static void __exit hfn_driver_exit(void)
{
    i2c_unregister_device(hfn_i2c_client_tcs);
    i2c_del_driver(&hfn_tcs_driver);
    pr_info("TCS Driver Removed!!!\n");
}

module_init(hfn_driver_init);
module_exit(hfn_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yago Hoffmann");
MODULE_DESCRIPTION("Simple I2C driver for TCS34725 sensor");
MODULE_VERSION("1.00");
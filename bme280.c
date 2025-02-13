#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/version.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/device.h>

MODULE_AUTHOR("Johannes 4Linux");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("A driver for reading out a BME280 temperature sensor");

static struct i2c_client *bme280_i2c_client;
static struct kobject *bme280_kobj;
static bool is_exported = false;
static int mode = 2;
static int osrs_h_val = 16;
static int osrs_t_val = 16;
static int osrs_p_val = 16;
static int t_sb_val = 5;
static int filter_coeff = 0;

static int create_sysfs_files(void);
static void remove_sysfs_files(void);
static int bme280_probe(struct i2c_client *client);
static void bme280_remove(struct i2c_client *client);

#define SLAVE_DEVICE_NAME	"bme280"

static struct of_device_id bme280_ids[] = {
	{
		.compatible = "bosch,bme280",
	}, { /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, bme280_ids);

static struct i2c_device_id bme280_id[] = {
    { SLAVE_DEVICE_NAME, 0 }, 
    { },
};
MODULE_DEVICE_TABLE(i2c, bme280_id);

static struct i2c_driver bme280_driver = {
	.probe = bme280_probe,
	.remove = bme280_remove,
	.id_table = bme280_id,
	.driver = {
		.name = SLAVE_DEVICE_NAME,
		.of_match_table = bme280_ids,
	},
};

struct mode_map {
    u8 reg_value;
    const char *description;
};

struct bme280_osrs {
    u8 reg_addr;
    u8 mask;
    int *osrs_val;
    const char *name;
};

struct t_sb_map {
    u8 value;
    const char *description;
};

struct filter_map {
    int coeff;
    u8 reg_value;
};

uint16_t dig_T1, dig_P1;
int16_t dig_T2, dig_T3, dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9, dig_H2, dig_H4, dig_H5;
uint8_t dig_H1, dig_H3;
int8_t dig_H6;

int32_t t_fine;
s32 read_temperature(void) {
    int64_t var1, var2;
    int32_t raw_temp;
    uint8_t d1, d2, d3;

    d1 = i2c_smbus_read_byte_data(bme280_i2c_client, 0xFA);
    d2 = i2c_smbus_read_byte_data(bme280_i2c_client, 0xFB);
    d3 = i2c_smbus_read_byte_data(bme280_i2c_client, 0xFC);

    raw_temp = ((int32_t)d1 << 12) | ((int32_t)d2 << 4) | ((int32_t)d3 >> 4);

    var1 = (((raw_temp >> 3) - ((int32_t)dig_T1 << 1)) * (int32_t)dig_T2) >> 11;
    var2 = (((((raw_temp >> 4) - (int32_t)dig_T1) * ((raw_temp >> 4) - (int32_t)dig_T1)) >> 12) * (int32_t)dig_T3) >> 14;
     
    t_fine = (int32_t)(var1 + var2);
 
    return (t_fine * 5 + 128) >> 8;
}

s32 read_pressure(void) {
    int64_t var1, var2, p;
    uint8_t d1, d2, d3;
    uint32_t raw_pressure;

    d1 = i2c_smbus_read_byte_data(bme280_i2c_client, 0xF7);
    d2 = i2c_smbus_read_byte_data(bme280_i2c_client, 0xF8);
    d3 = i2c_smbus_read_byte_data(bme280_i2c_client, 0xF9);

    raw_pressure = ((uint32_t)d1 << 12) | ((uint32_t)d2 << 4) | ((uint32_t)d3 >> 4);

    var1 = ((int64_t)t_fine) - 128000;
    var2 = var1 * var1 * (int64_t)dig_P6;
    var2 = var2 + ((var1 * (int64_t)dig_P5) << 17);
    var2 = var2 + ((int64_t)dig_P4 << 35);
    var1 = ((var1 * var1 * (int64_t)dig_P3) >> 8) + ((var1 * (int64_t)dig_P2) << 12);
    var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)dig_P1) >> 33;

    if (var1 == 0) {
        return 0;
    }

    p = 1048576 - raw_pressure;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = ((int64_t)dig_P9 * (p >> 13) * (p >> 13)) >> 25;
    var2 = ((int64_t)dig_P8 * p) >> 19;
    p = ((p + var1 + var2) >> 8) + ((int64_t)dig_P7 << 4);

    return (s32)(p / 256);
}

s32 read_humidity(void) {
    int64_t var_H;
    uint8_t d1, d2;
    uint32_t raw_humidity;

    d1 = i2c_smbus_read_byte_data(bme280_i2c_client, 0xfd);
    d2 = i2c_smbus_read_byte_data(bme280_i2c_client, 0xfe);
    raw_humidity = ((uint32_t)d1 << 8) | (uint32_t)d2;

    var_H = ((int64_t)t_fine) - 76800;
    var_H = ((((((int64_t)raw_humidity << 14) - (((int64_t)dig_H4) << 20) - (((int64_t)dig_H5) * var_H)) + ((int64_t)16384)) >> 15) * (((((((var_H * ((int64_t)dig_H6)) >> 10) * (((var_H * ((int64_t)dig_H3)) >> 11) + ((int64_t)32768))) >> 10) + ((int64_t)2097152)) * ((int64_t)dig_H2) + 8192) >> 14));
    var_H = (var_H - (((((var_H >> 15) * (var_H >> 15)) >>7) * ((int64_t)dig_H1)) >> 4));
    var_H = (var_H < 0 ? 0 : var_H);
    var_H = (var_H > 419430400 ? 419430400 : var_H);

    return (s32)(var_H >> 12);
}

static ssize_t export_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
    return sprintf(buf, "%d\n", is_exported);
}

static ssize_t export_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count) {
    int val;
    if (kstrtoint(buf, 10, &val))
        return -EINVAL;

    if (val == 1 && !is_exported) {
        if (create_sysfs_files() == 0) {
            is_exported = true;
        }
        pr_info("BME280: config/ and values/ directories created in /sys/bus/i2c/devices/1-0076/\n");
    } else if (val == 0 && is_exported) {
        remove_sysfs_files();
        is_exported = false;
        pr_info("BME280: config/ and values/ directories removed from /sys/bus/i2c/devices/1-0076/\n");
    }

    return count;
}

static ssize_t reset_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
    int ret;
    u8 reset_value = 0xb6;

    if (buf[0] != '1')
        return -EINVAL;

    if (!bme280_i2c_client)
        return -ENODEV;

    ret = i2c_smbus_write_byte_data(bme280_i2c_client, 0xe0, reset_value);
    if (ret < 0) {
        pr_err("BME280: Failed to write reset command\n");
        return ret;
    }
    msleep(2);
    ret = i2c_smbus_write_byte_data(bme280_i2c_client, 0xe0, 0x00);

    i2c_smbus_write_byte_data(bme280_i2c_client, 0xF2, 5);
    i2c_smbus_write_byte_data(bme280_i2c_client, 0xF5, 5 << 5);
    i2c_smbus_write_byte_data(bme280_i2c_client, 0xF4, (5 << 5) | (5 << 2) | (3));
    
    pr_info("BME280: Device Reset Complete\n");
    return count;
}

static struct mode_map bme280_modes[] = {
    {0x00, "Sleep Mode"},
    {0x01, "Forced Mode"},
    {0x03, "Normal Mode"},
};

static ssize_t mode_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
    if (mode >= 0 && mode < ARRAY_SIZE(bme280_modes))
        return sprintf(buf, "%d: %s\n", mode, bme280_modes[mode].description);

    return sprintf(buf, "%d: Unknown Mode\n", mode);
}    


static ssize_t mode_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count) {
    int val;
    if (kstrtoint(buf, 10, &val) || val < 0 || val >= ARRAY_SIZE(bme280_modes))
        return -EINVAL;

    u8 reg_val = i2c_smbus_read_byte_data(bme280_i2c_client, 0xF4) & ~0x03;
    i2c_smbus_write_byte_data(bme280_i2c_client, 0xF4, reg_val | bme280_modes[val].reg_value);
    mode = val;
    pr_info("BME280: Device set to %s\n", bme280_modes[val].description);

    return count;
}

static struct bme280_osrs osrs_settings[] = {
    {0xF2, 0x07, &osrs_h_val, "Humidity"},
    {0xF4, 0xE0, &osrs_t_val, "Temperature"},
    {0xF4, 0x1C, &osrs_p_val, "Pressure"}
};

static ssize_t osrs_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf, int index) {
    int val = *(osrs_settings[index].osrs_val);
    return sprintf(buf, "Oversampling * %d\n", val);
}

static ssize_t osrs_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count, int index) {
    int val;
    u8 reg_val;

    if (kstrtoint(buf, 10, &val))
        return -EINVAL;

    reg_val = i2c_smbus_read_byte_data(bme280_i2c_client, osrs_settings[index].reg_addr) & ~osrs_settings[index].mask;

    u8 shift = __builtin_ctz(osrs_settings[index].mask);
    u8 new_val;
    switch (val) {
        case 1: new_val = (0x01 << shift); break;
        case 2: new_val = (0x02 << shift); break;
        case 4: new_val = (0x03 << shift); break;
        case 8: new_val = (0x04 << shift); break;
        case 16: new_val = (0x05 << shift); break;
        default: return -EINVAL;
    }

    *(osrs_settings[index].osrs_val) = val;
    i2c_smbus_write_byte_data(bme280_i2c_client, osrs_settings[index].reg_addr, reg_val | new_val);
    pr_info("BME280: %s Oversampling Factor = %d\n", osrs_settings[index].name, val);

    return count;
}

static ssize_t osrs_h_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
    return osrs_show(kobj, attr, buf, 0);
}

static ssize_t osrs_t_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
    return osrs_show(kobj, attr, buf, 1);
}

static ssize_t osrs_p_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
    return osrs_show(kobj, attr, buf, 2);
}

static ssize_t osrs_h_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count) {
    return osrs_store(kobj, attr, buf, count, 0);
}

static ssize_t osrs_t_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count) {
    return osrs_store(kobj, attr, buf, count, 1);
}

static ssize_t osrs_p_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count) {
    return osrs_store(kobj, attr, buf, count, 2);
}

static struct t_sb_map t_sb_modes[] = {
    {0, "Standby Time = 0.5ms"},
    {1, "Standby Time = 62.5ms"},
    {2, "Standby Time = 125ms"},
    {3, "Standby Time = 250ms"},
    {4, "Standby Time = 500ms"},
    {5, "Standby Time = 1000ms"},
    {6, "Standby Time = 10ms"},
    {7, "Standby Time = 20ms"},
};

static ssize_t t_sb_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
    if (t_sb_val >= 0 && t_sb_val < ARRAY_SIZE(t_sb_modes)) {
        return sprintf(buf, "%d: %s\n", t_sb_val, t_sb_modes[t_sb_val].description);
    }
    return sprintf(buf, "%d: Invalid value\n", t_sb_val);
}

static ssize_t t_sb_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count) {
    int val;
    u8 reg_val;

    if (kstrtoint(buf, 10, &val) || val < 0 || val >= ARRAY_SIZE(t_sb_modes))
        return -EINVAL;
    
    reg_val = i2c_smbus_read_byte_data(bme280_i2c_client, 0xF5) & ~0xE0;
    i2c_smbus_write_byte_data(bme280_i2c_client, 0xF5, reg_val | (val << 5));
    
    t_sb_val = val;

    pr_info("BME280: %s\n", t_sb_modes[val].description);

    return count;
}

static struct filter_map filter_modes[] = {
    {0, 0x00},
    {2, 0x01},
    {4, 0x02},
    {8, 0x03},
    {16, 0x04},
};

static ssize_t filter_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
    return sprintf(buf, "Filter Coefficient = %d\n", filter_coeff);
}

static u8 get_filter_reg_value(int coeff) {
    for (int i = 0; i < ARRAY_SIZE(filter_modes); i++) {
        if (filter_modes[i].coeff == coeff)
            return filter_modes[i].reg_value;
    }
    return 0xFF;
}

static ssize_t filter_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count) {
    int val;
    u8 reg_val, reg_setting;

    if (kstrtoint(buf, 10, &val))
        return -EINVAL;

    reg_setting = get_filter_reg_value(val);

    if (reg_setting == 0xFF)
        return -EINVAL;

    reg_val = i2c_smbus_read_byte_data(bme280_i2c_client, 0xF5) & ~0x1C;
    i2c_smbus_write_byte_data(bme280_i2c_client, 0xF5, reg_val | (reg_setting << 2));

    pr_info("BME280: Filter Coefficient: %d\n", val);
    filter_coeff = val;

    return count;
}

static ssize_t temperature_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
    int temperature = read_temperature();
    return sprintf(buf, "%d.%02d\n", temperature / 100, temperature % 100);
}

static ssize_t pressure_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
    int pressure = read_pressure();
    return sprintf(buf, "%d\n", pressure);
}

static ssize_t humidity_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
    int humidity = read_humidity();
    return sprintf(buf, "%d.%02d\n", humidity / 1024, (humidity * 100 / 1024) % 100);
}

static struct kobj_attribute export_attr = __ATTR(export, 0660, export_show, export_store);
static struct kobj_attribute mode_attr = __ATTR(mode, 0660, mode_show, mode_store);
static struct kobj_attribute osrs_h_attr = __ATTR(osrs_h, 0660, osrs_h_show, osrs_h_store);
static struct kobj_attribute osrs_t_attr = __ATTR(osrs_t, 0660, osrs_t_show, osrs_t_store);
static struct kobj_attribute osrs_p_attr = __ATTR(osrs_p, 0660, osrs_p_show, osrs_p_store);
static struct kobj_attribute t_sb_attr = __ATTR(t_sb, 0660, t_sb_show, t_sb_store);
static struct kobj_attribute filter_attr = __ATTR(filter, 0660, filter_show, filter_store);
static struct kobj_attribute reset_attribute = __ATTR(reset, 0220, NULL, reset_store);
static struct kobj_attribute temperature_attr = __ATTR(temperature, 0444, temperature_show, NULL);
static struct kobj_attribute pressure_attr = __ATTR(pressure, 0444, pressure_show, NULL);
static struct kobj_attribute humidity_attr = __ATTR(humidity, 0444, humidity_show, NULL);

static struct attribute *bme280_values_attrs[] = {
    &temperature_attr.attr,
    &pressure_attr.attr,
    &humidity_attr.attr,
    NULL,
};

static struct attribute_group bme280_values_group = {
    .name = "values",
    .attrs = bme280_values_attrs,
};

static struct attribute *bme280_config_attrs[] = {
    &mode_attr.attr,
    &reset_attribute.attr,
    &osrs_t_attr.attr,
    &osrs_p_attr.attr,
    &osrs_h_attr.attr,
    &t_sb_attr.attr,
    &filter_attr.attr,
    NULL,
};

static struct attribute_group bme280_config_group = {
    .name = "config",
    .attrs = bme280_config_attrs,
};

static int create_sysfs_files(void) {
    int ret;

    ret = sysfs_create_group(&bme280_i2c_client->dev.kobj, &bme280_values_group);
    if (ret) {
        dev_err(&bme280_i2c_client->dev, "Failed to create values directory\n");
        return ret;
    }

    ret = sysfs_create_group(&bme280_i2c_client->dev.kobj, &bme280_config_group);
    if (ret) {
        dev_err(&bme280_i2c_client->dev, "Failed to create config directory\n");
        sysfs_remove_group(&bme280_i2c_client->dev.kobj, &bme280_values_group);
        return ret;
    }
    return ret;
}

static void remove_sysfs_files(void) {
    sysfs_remove_group(&bme280_i2c_client->dev.kobj, &bme280_values_group);
    sysfs_remove_group(&bme280_i2c_client->dev.kobj, &bme280_config_group);
}

static int bme280_probe(struct i2c_client *client){
	int ret = -1;
    u8 id;
	pr_info("BME280: Entered Probe Function\n");

    bme280_i2c_client = client;
    bme280_kobj = &bme280_i2c_client->dev.kobj;

    ret = sysfs_create_file(bme280_kobj, &export_attr.attr);
    if(ret) {
        pr_err("BME280: Error creating /sys/bus/i2c/drivers/bme280/export file\n");
		kobject_put(bme280_kobj);
		return -ENOMEM;
    }

	id = i2c_smbus_read_byte_data(bme280_i2c_client, 0xD0);
    if (id != 0x60) {
        pr_err("BME280: Invalid device ID (0x%x). Expected 0x60\n", id);
        return -ENODEV;
    }
	pr_info("BME280: Device ID verified (0x%x)\n", id);

	dig_T1 = i2c_smbus_read_word_data(bme280_i2c_client, 0x88);
	dig_T2 = i2c_smbus_read_word_data(bme280_i2c_client, 0x8a);
	dig_T3 = i2c_smbus_read_word_data(bme280_i2c_client, 0x8c);

    dig_P1 = i2c_smbus_read_word_data(bme280_i2c_client, 0x8e);
	dig_P2 = i2c_smbus_read_word_data(bme280_i2c_client, 0x90);
	dig_P3 = i2c_smbus_read_word_data(bme280_i2c_client, 0x92);
    dig_P4 = i2c_smbus_read_word_data(bme280_i2c_client, 0x94);
	dig_P5 = i2c_smbus_read_word_data(bme280_i2c_client, 0x96);
	dig_P6 = i2c_smbus_read_word_data(bme280_i2c_client, 0x98);
    dig_P7 = i2c_smbus_read_word_data(bme280_i2c_client, 0x9a);
	dig_P8 = i2c_smbus_read_word_data(bme280_i2c_client, 0x9c);
	dig_P9 = i2c_smbus_read_word_data(bme280_i2c_client, 0x9e);

	dig_H1 = i2c_smbus_read_byte_data(bme280_i2c_client, 0xa1);
	dig_H2 = i2c_smbus_read_word_data(bme280_i2c_client, 0xe1);
	dig_H3 = i2c_smbus_read_byte_data(bme280_i2c_client, 0xe3);
	dig_H4 = (i2c_smbus_read_byte_data(bme280_i2c_client, 0xe4) << 4) | (i2c_smbus_read_byte_data(bme280_i2c_client, 0xe5) & 0x0f);
	dig_H5 = (i2c_smbus_read_byte_data(bme280_i2c_client, 0xe6) << 4) | ((i2c_smbus_read_byte_data(bme280_i2c_client, 0xe5) >> 4) & 0x0f);
	dig_H6 = i2c_smbus_read_byte_data(bme280_i2c_client, 0xe7);

    i2c_smbus_write_byte_data(bme280_i2c_client, 0xF2, 5);
    i2c_smbus_write_byte_data(bme280_i2c_client, 0xF5, 5 << 5);
    i2c_smbus_write_byte_data(bme280_i2c_client, 0xF4, (5 << 5) | (5 << 2) | (3));
	
    return ret;
}

static void bme280_remove(struct i2c_client *client) {
	pr_info("BME280: Entered Remove Function\n");
    if (is_exported){
        remove_sysfs_files();
    }
    sysfs_remove_file(bme280_kobj, &export_attr.attr);
    kobject_put(bme280_kobj);
}

module_i2c_driver(bme280_driver);

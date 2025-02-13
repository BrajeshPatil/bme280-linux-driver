# BME280 Linux Kernel Driver
This repository contains a Linux device driver for the BME280 environmental sensor, supporting temperature, pressure, and humidity measurements. The driver has been tested on a Raspberry Pi 5 running Linux kernel 6.6.74.

## Features
✅ **Device Tree Support** – The sensor is registered using a **Device Tree Overlay(DTO)**.

✅ **Data Retrieval via Sysfs** – The following sensor readings can be accessed through **sysfs** interface:
- Temperature (in °C)
- Pressure (in Pa)
- Humidity (in %)

✅ **Configurable Parameters via Sysfs** – The sensor settings can be adjusted, including:
- Oversampling settings
- Standby time
- Filter coefficient
- Sensor mode
- Reset functionality

## Working
The following section provides a detailed explanation of how the code functions:
1) The ``bme280_overlay.dts`` is a Device Tree Overlay that enables the **BME280 sensor** on the **I²C1** bus at address **0x76**. 
2) The ``bme280.c`` is the C code for BME280 Driver. The following steps are followed in the code:
    - The compatible string in the Device Tree Overlay (DTO) is matched against the driver, and upon a successful match, the driver is bound to the device created by the DTO. This leads to the creation of device ``1-0076`` in ``/sys/bus/i2c/devices/``
    - A kernel object ``bme280`` is created in ``/sys/bus/i2c/drivers/``, followed by the creation of an ``export`` file in ``/sys/bus/i2c/devices/1-0076/``. The directories for configuration and values are also generated within this directory, and their visibility can be controlled by modifying the export file.
    - The sensor ID is read from the register and logged in the kernel.
    - Functions for reading temperature, pressure, and humidity are implemented and mapped to their respective sysfs files. Additionally, show and store functions are provided for all configuration and data files.
3) The ``Makefile`` is used to compile the ``bme280.c`` and ``bme280_overlay.dts`` and create the Device Tree Blob Overlay(.dtbo) and Kernel Object(.ko) files.

## Getting Started
### Hardware Layout
![Design](https://github.com/user-attachments/assets/0148f9f7-29e1-4953-b711-1e7e343d1660)

Connections
| Sr. No | BME280 Pin | Raspberry Pi Pin        |
|:--------:|:-----------:|:-------------------------:|
| 1      | VIN       | 3V3 Power (Pin 1)       |
| 2      | GND       | Ground (Pin 6)          |
| 3      | SCL       | GPIO 3 (SCL) (Pin 5)    |
| 4      | SDA       | GPIO 2 (SDA) (Pin 3)    |

### Prerequisites
Install kernel headers and Required Packages
```
$ sudo apt update && sudo apt upgrade -y

$ sudo apt install -y raspberrypi-kernel-headers build-essential bc dkms
```
The Raspberry Pi's built-in BMP280 driver uses address 0x76, the same as BME280. To use a custom driver, unbind the device from BMP280 first.
### Installation
Clone the project
```
$ git clone https://github.com/BrajeshPatil/bme280-linux-driver.git

$ cd bme280-linux-driver
```

### Usage
Configure I2C on Raspberry Pi
```
$ sudo raspi-config
```
This will open an interactive shell. Select ``Interface Options --> I2C``. Then press Enter to enable I2C communication.

After hardware connections are done. Check whether the device is detected.
```
$ i2cdetect -y 1

     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f
00:                         -- -- -- -- -- -- -- -- 
10: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
20: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
30: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
40: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
50: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
60: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
70: -- -- -- -- -- -- 76 --
```
If the device is detected 0x76 or 0x77 should be available. If it is not available then there would be a problem in the Sensor or the Wiring.


Build the project
```
$ make 
```
A successfull output would be somthing like this
```
make -C /lib/modules/6.6.74+rpt-rpi-2712/build M=/home/brajesh/Programming/Sensor_Drivers/07_BME280/BME280_sysfs_probe modules
make[1]: Entering directory '/usr/src/linux-headers-6.6.74+rpt-rpi-2712'
make[1]: Leaving directory '/usr/src/linux-headers-6.6.74+rpt-rpi-2712'
dtc -@ -I dts -O dtb -o bme280_overlay.dtbo bme280_overlay.dts
echo Builded Device Tree Overlay and kernel module
Builded Device Tree Overlay and kernel module
```
Multiple files would be created in the directory. The one's which are important to us are ``bme280.ko`` and ``bme280_overlay.dtbo``


Dynamically load the DTO for the BME280
```
$ sudo dtoverlay bme280_overlay.dtbo
```
A device ``1-0076`` will be loaded in ``/sys/bus/i2c/devices/``. Use the following command to check successful creation of device. 
```
$ ls /sys/bus/i2c/devices/
```


Load the Kernel Module
```
$ sudo insmod bme280.ko
```
Use lsmod command to check correct insertion of module.
```
$ lsmod | grep bme280
```
If module insertion fails use dmesg command for debugging.
```
$ dmesg | tail -n 20
[ 1196.895040] bme280: loading out-of-tree module taints kernel.
[ 1196.895337] BME280: Now I am in the Probe Function
[ 1196.895772] ID: 0x60
```

A driver ``bme280`` would be created in ``/sys/bus/i2c/drivers/``. Also a file ``export`` will be created in the directory ``/sys/bus/i2c/devices/1-0076/``
```
$ ls /sys/bus/i2c/drivers
```


Echo 1 to the file ``/sys/bus/i2c/devices/1-0076/export`` for enabling the data and configuration files.
```
$ echo 1 | sudo tee /sys/bus/i2c/devices/1-0076/export
```

This will enable the data and configuration files. Two new directories ``config/`` and ``values/`` will now be shown in ``/sys/bus/i2c/devices/1-0076/``. The current values of temperature, pressure and humidity can be read using the files in ``values/`` directory. The files in ``config/`` can be used to change the configuration of the sensor.

Example: Reading Values
```
$ cat /sys/bus/i2c/devices/1-0076/values/temperature 
31.85
```
Similarly pressure and humidity values can be read from respective files.

## Configuration
Follow the general command to configure the sensor using the ``/sys/bus/i2c/devices/1-0076/config/`` files
```
$ echo <config_value> | sudo tee /sys/bus/i2c/devices/1-0076/config/<file_name>
```
<config_value> = The value of the corresponding setting which is to be set
<file_name> = The name of the configuration file which has to be changed

Given below are the values and corresponsing configurations for all the config files:

### 1. export
Present in ``/sys/bus/i2c/devices/1-0076/``. Used to hide/show ``config/`` and ``values/`` directories.

| Value | Description                          |
|:------------:|:--------------------------------------:|
| 0 (Default)| values/ and config/ directories hidden   |
| 1     | values/ and config/ directories present  |

### 2. filter
Present in ``/sys/bus/i2c/devices/1-0076/config/``. Used to configure filter coefficient of the sensor.

| Value        | Filter Coefficient |
|:-------------:|:--------------------:|
| 0 (Default) | 0                  |
| 2           | 2                  |
| 4           | 4                  |
| 8           | 8                  |
| 16          | 16                 |

### 3. mode
Present in ``/sys/bus/i2c/devices/1-0076/config/``. Used to select the mode in which sensor operates.

| Value | Mode          |
|:-------------:|:-------------:|
| 0           | Sleep Mode   |
| 1           | Forced Mode  |
| 2 (Default) | Normal Mode  |

### 4. osrs_h
Present in ``/sys/bus/i2c/devices/1-0076/config/``. Used to select the Oversampling Factor for Humidity Measurement.

| Value | Humidity Oversampling |
|:-------:|:-----------------------:|
| 1     | ×1                    |
| 2     | ×2                    |
| 4     | ×4                    |
| 8     | ×8                    |
| 16 (Default)   | ×16                   |

### 5. osrs_p
Present in ``/sys/bus/i2c/devices/1-0076/config/``. Used to select the Oversampling Factor for Pressure Measurement.

| Value | Humidity Oversampling |
|:-------:|:-----------------------:|
| 1     | ×1                    |
| 2     | ×2                    |
| 4     | ×4                    |
| 8     | ×8                    |
| 16 (Default)   | ×16                   |

### 7. osrs_t
Present in ``/sys/bus/i2c/devices/1-0076/config/``. Used to select the Oversampling Factor for Temperature Measurement.

| Value | Humidity Oversampling |
|:-------:|:-----------------------:|
| 1     | ×1                    |
| 2     | ×2                    |
| 4     | ×4                    |
| 8     | ×8                    |
| 16 (Default)   | ×16                   |

### 8. t_sb
Present in ``/sys/bus/i2c/devices/1-0076/config/``. Used to control standby time (inactive duration) in the Normal Mode.

| Value        | Standby Time |
|:------------:|:-----------:|
|   0          |   0.5ms    |
|   1          |  62.5ms    |
|   2          |   125ms    |
|   3          |   250ms    |
|   4          |   500ms    |
|   5 (Default)|  1000ms    |
|   6          |    10ms    |
|   7          |    20ms    |
   
### 9. reset
Writing '1' to the file resets the sensor to default settings

## License
The [License](https://github.com/BrajeshPatil/bme280-linux-driver/blob/main/LICENSE) Used for this Project.

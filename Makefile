obj-m += bme280.o

all: module dt
  echo Builded Device Tree Overlay and kernel module

module:
  make -C /lib/modules/$(shell uname -r)/build M=$(CURDIR) modules
dt: bme280_overlay.dts
  dtc -@ -I dts -O dtb -o bme280_overlay.dtbo bme280_overlay.dts
clean:
  make -C /lib/modules/$(shell uname -r)/build M=$(CURDIR) clean
  rm -rf bme280_overlay.dtbo

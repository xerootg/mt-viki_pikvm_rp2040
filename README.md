# MT-VIKI 4-port KVM control

parts to this:
 - rp2040 program
 - pikvm script

## how the MCU side is built
I used a waveshare rp2040-one dev board and hooked up a micro usb cable ground to gnd, green to 15, and white to 14. Build the firmware by:
```bash
mkdir build
cd build
cmake ..
make
```
then copy `build/mt-viki_pikvm_rp2040.uf2` onto the rp2040 in bootloader mode.

#### note: you need to set the path to your picosdk in CMakeLists.txt
```cmake
include(pico_sdk_init.cmake) # your responsible for setting PICO_SDK_PATH
```

## the script for pikvm
the mcu sends json across it's connection. because the tty might unstable for whatever reason, use `/dev/serial/by-id/usb-Raspberry_Pi_Pico_*whatever_your_id_is*-if00` as your connection in the setup.

### copy things into place
Copy `mtviki.py` into `/usr/lib/python3.12/site-packages/kvmd/plugins/ugpio/`

* *note:* I need to PR this "driver" and then this hack goes away

#### /etc/kvmd/override.yaml
```yaml
kvmd:
    gpio:
        drivers:
            mt:
                type: mtviki
                device: /dev/serial/by-id/usb-Raspberry_Pi_Pico_{YOUR_DEVICE_ID}-if00
        scheme:
            ch0_led:
                driver: mt
                pin: 0
                mode: input
            ch1_led:
                driver: mt
                pin: 1
                mode: input
            ch2_led:
                driver: mt
                pin: 2
                mode: input
            ch3_led:
                driver: mt
                pin: 3
                mode: input
            ch0_button:
                driver: mt
                pin: 0
                mode: output
                switch: false
            ch1_button:
                driver: mt
                pin: 1
                mode: output
                switch: false
            ch2_button:
                driver: mt
                pin: 2
                mode: output
                switch: false
            ch3_button:
                driver: mt
                pin: 3
                mode: output
                switch: false
        view:
            table:
                - ["#Input 1", ch0_led, ch0_button]
                - ["#Input 2", ch1_led, ch1_button]
                - ["#Input 3", ch2_led, ch2_button]
                - ["#Input 4", ch3_led, ch3_button]
```

# IF YOU HAVE AN 8 PORT
the biggest difference seems like it's the timings. I might refactor a change in for that to support more than one timing, but probably not unless someone asks since I cannot test.
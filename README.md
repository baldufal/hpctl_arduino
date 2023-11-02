# HPCTL Arduino
Arduino software for Lilygo T-Display-S component of HPCTL heating and ventilation controller.

We use this mc to control another mc (AVR) on a custon PCB via I2C.
The software of the other controller together with the I2C interface description and the PCB can be found in its repository [HPCTL AVR](https://github.com/baldufal/hpctl_avr).

# Task
This controller provides two interfaces for indirectly controlling the other controller:
- A simple HTTP REST API over Wifi
- A GUI on the attached display, which can be controlled by 4 buttons

# Setup
The setup is described on the LilyGo [product page](https://github.com/Xinyuan-LilyGO/T-Display-S3/tree/main).

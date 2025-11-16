(c) 2025 M10tech

# Bus Display BitDriver

Use i2s to directly drive the TLC5921 chips found in the Ameli buspanel,  
without using the 80C32 based controller.

Use the LCM4ESP32 concept to run this  
Details on https://github.com/HomeACcessoryKid/LCM4ESP32  
and instructions how to deploy here  
https://github.com/HomeACcessoryKid/LCM4ESP32/blob/main/deploy.md  

Also read the WIKI to get background info on the hardware needed and the priciples involved.

## Version History

### 0.0.2 proof of no concept
- the actual bits are sent out fine, and the clock is OK, but...
- the start of SIN is not together with start of SCLK
- the timer to shutdown the SCLK is unreliable
- the timer to shutdown the SCLK has an offset of ~900 microseconds
- maybe using the interupt on send complete can provide the xlat??

### 0.0.1 bootstrapping LCM4ESP32
- no useful functionality yet
- UDPlogger and wifi working

### Initial commit

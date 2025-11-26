(c) 2025 M10tech

# Bus Display BitDriver

Use bitbanging to directly drive the TLC5921 chips found in the Ameli buspanel,  
without using the 80C32 based controller.

Use the LCM4ESP32 concept to run this  
Details on https://github.com/HomeACcessoryKid/LCM4ESP32  
and instructions how to deploy here  
https://github.com/HomeACcessoryKid/LCM4ESP32/blob/main/deploy.md  

Also read the WIKI to get background info on the hardware needed and the priciples involved.

## Version History

### 0.1.4 inline assembly delay function is slighty faster
- does not use a function but is inline
- use start%= to allow use in macro
- reduces refreshrate by 15% 170 micros/column

### 0.1.3 removed heavy glitches and better contrast
- the infinite while-loop did not allow the taskWatchDog to be fed, causing a TWD-report on CPU-1
- disabled the TWD with CONFIG_ESP_TASK_WDT_EN=n in sdkconfig.defaults
- protected the XLAT pin with taskENTER_CRITICAL (might not be needed...)
- better contrast at the cost of 200 micros/column refreshrate
- additional test-patterns

### 0.1.2 using an assembly delay
- reduces time per column to 46 microseconds
- with the four level brightness, faster seems difficult
- the receiving hardware seems to hold up with the speed

### 0.1.1 improved brightness levels
- much clearer level differences
- experiments with alternative delay methods
- for now, 1 microsecond with esp_timer_get_time() loop works OK
- forgot to put IDF version to 0.1.1

### 0.1.0 four level pwm per LED bitbanging
- with artificial delay of 10 microseconds works well
- maybe levelshifter is bandwidth limited since no-delay does not work well
- differences between led levels not easy to appreciate
- no blanking used so far, which might (or not) interfere

### abandoned i2s and went to bitbanging
- note that in 0.0.3 I forgot to call init_xlat() so maybe there was still some use, but who cares?

### 0.0.3 really doesn't work
- using on_sent isr to drive xlat has still issues since clock continues driving
- during the xlat pulse, the bits are shifted on part of the panel
- sometimes the xlat arrives late or ...
- will revert to bit-banging from here
- i2s is good for timing critical applications, which this is not
- this application is critical in sequencing of lines, without timing

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

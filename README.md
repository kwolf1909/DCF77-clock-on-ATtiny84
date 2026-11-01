This application receives the european DCF77 time signal and syncs it with the external RTC-module. It is displayed on an 8-digit 14- or 7-segment display with display-controller HT16K33. The MCU used is an ATtiny84. The application has been ported from initial repository (using ATtiny1614 or ATmega4809).<br>
The circuit can be powered by a Li-Ion cell. If the voltage drops below 3.0 V, the voltage is displayed as a low voltage indicator.<br>
The temperature can be read by internal sensor of RTC or external OneWire-based sensor.<br>
A sync to DCF-time is performed in the background every 30 minutes, and immediately after power-up.<br>
The software uses a layer-based approach. Low-level: DCF-object with interrupt handler, mid-level: receiving data state machine, upper level: display handling and user interface.<br>
No blocking code, using several finite state machines (FSM).<br>
For I2C a bit-banging library is used, because the hardware-pin for I2C-CLK is shared with Timer1 clock-input (T1).<br>
The DCF-module is powered by an output pin, so it's only powered up during a receive cycle.<br>

Push-button based interface<br>
Select display modes: time with date | time with seconds | time with temperature | time with battery voltage.<br>

MCU-clock: 8 MHz<br>
Timers used: Timer1 (DCF pulse width measurement and 2 Hz generation for clock ticks)

Debugging:<br>
A serial debug output (9600 baud) can be enabled by uncommenting SERIALDEBUG. Due to limited flash space, this requires some other functions to be disabled temporarily, e.g. OneWire.<br>

Pins used (ATtiny84):<br>
PA0 - Input, DCF-signal, active-low signal, external pull-up 10 kilohm required, external RC low-pass filter recommended.<br>
PA1 - Output, TX for serial debugging.<br>
PA2 - Input, RX (not used).<br>
PA3 - Output, LED, mirrors the DCF-input signal in sync-state, active-high.<br>
PA4 - Input, external 32K clock-signal from DS3231, external pull-up 10 kilohm required.<br>
PA5 - Output, DCF-power, connect to VCC-pin of receive-module.<br>
PA6 - Input/Output (OneWire data), temperature sensor with external pullup resistor 10 kilohm.<br>
PA7 - Input, mode select button, active low with input-pullup enabled.<br>
PB0 - I2C DATA for display & RTC.<br>
PB1 - I2C CLK for display & RTC.<br>

Compiles with TinyCore (MCUdude) on Arduino IDE https://github.com/MCUdude/TinyCore.<br>
External libraries required: SoftI2CMaster https://github.com/felias-fogg/SoftI2CMaster.<br>

Hardware:<br>
DS3231 RTC Precision Breakout: https://www.adafruit.com/product/3013, or equivalent.<br>
DCF77-receiver: ELV DCF-2 (MAS6180-based AM-receiver), any other receiver possible with digital signal output.<br>
DFRobot 7-segment 8-digit 7-segment display: https://www.dfrobot.com/product-1978.html<br>
Or: Custom built alphanumeric display: http://www.technoblogy.com/show?2ULE<br>
Temperature-sensor: DS18B20<br>
Resistors: 1x 1 kilohm (LED), 3x 10 kilohm (pullups)<br>

Alphanumeric displays used on board: https://www.adafruit.com/product/2154<br>

<img width="640" height="214" alt="alpha" src="https://github.com/user-attachments/assets/6b047f4b-43a1-4883-a1a5-ab0c732ea372" /><br>

This is a debug-output for a complete receiving-cycle as an example.
```
DCF: Minute marker detected
DCF: Startbit detected, receiving now...
0000000000000000100100000000100010000010000100001011001000
DCF: Receive complete
Receive: Receive complete
Receive: Decode successful
Receive: RTC synced to DCF-time 17:51:00
```



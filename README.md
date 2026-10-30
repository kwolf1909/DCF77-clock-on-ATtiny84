This program receives the european DCF77 time signal and syncs it with the external RTC-module DS3231. It is displayed on a 8-digit 14- or 7-segment display with display-controller HT16K33. The MCU used is an ATtiny84. The application has been ported from initial repository using ATtiny1614 or ATmega4809 finally.<br>
The circuit can be powered by a Li-Ion cell. If the voltage drops below 3.0 V, the voltage is displayed as a low voltage indicator.
The temperatur can be read by internal DS3231 sensor or external OneWire-based DS18B20 sensor.<br>
A sync to DCF-time is performed in the background every 30 minutes, and immediately after power-up.<br>
The software uses a layer-based approach. Low-level: DCF-object with interrupt handler,
mid-level: receiving data state machine, upper level: display handling and user interface.<br>
No blocking code, using several finite state machines (FSM).<br>
For I2C a bit-banging library is used, because the hardware-pin for I2C-CLK is shared with Timer1 clock-input (T1).

Push-button based interface<br>
Select display modes: time with date | time with seconds | time with temperature | time with battery voltage.<br>

MCU-clock: 8 MHz<br>
Timers used: Timer1 (DCF pulse width measurement and 2 Hz generation for clock ticks)
             
External RTC: DS3231 with battery backup, supplies 32K clock for Timer1.<br>
8-digit display (I2C): DFRobot 7-segment, or custom built 14-segment.<br>
External DCF77-receiver: ELV DCF-2 (MAS6180 AM-receiver).<br>
Temperature-sensor: DS18B20 with external pullup resistor 4,7k ohms.<br>

Pins used (ATtiny84):<br>
PA0 - Input, DCF-signal, active-low signal, external pull-up 10 kohms required, external RC low-pass filter recommended<br>
PA1 - Output, TX for serial debugging<br>
PA3 - Output, LED which mirrors the DCF-input signal in sync-state<br>
PA4 - Input, external 32K clock-signal from DS3231, external pull-up 10 kohms required<br>
PA5 - Output, powering DCF receive-module<br>
PA6 - Input/Output, temp sensor DS18B20<br>
PA7 - Input, mode select button, active low with input-pullup enabled<br>
PB0 - I2C DATA for display & RTC<br>
PB1 - I2C CLK for display & RTC<br>

Compiles with TinyCore on Arduino IDE.<br>
External libraries required: SoftI2Master<br>

DFRobot 7-segment 8-digit display: https://www.dfrobot.com/product-1978.html<br>
Custom built alphanumeric display: http://www.technoblogy.com/show?2ULE<br>
Alphanumeric displays used on board: https://www.adafruit.com/product/2154

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



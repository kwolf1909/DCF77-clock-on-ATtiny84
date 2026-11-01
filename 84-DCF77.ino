//------------------------------------------------------------------------------------------------
//  84-DCF77.ino
//
//  This program receives the european DCF77 time signal and syncs it with the external RTC-clock.
//  It is displayed on an 8-digit alphanumeric- or 7-segment display with display-controller HT16K33.
//  The MCU used is an ATtiny84.
//  The circuit can be powered by a Li-Ion cell. If the voltage drops below 3.0 V,
//  the voltage is displayed as a low voltage indicator.
//  The temperatur can be read by internal sensor of RTC or external OneWire-based sensor.
//  A push button switches between different display modes:
//  Time with date, time with seconds, time with temperature, time with battery voltage
//
//  MCU-clock: 8 MHz (recommended)
//  Timers used: Timer1 (DCF pulse width measurement and 2 Hz generation for clock ticks)
//  External RTC: DS3231 with battery backup, supplies 32K clock for Timer1
//
//  Author: Klaus Wolf
//  Date: Nov 01 2026
//------------------------------------------------------------------------------------------------

#if defined(__AVR_ATtiny84__)
#define SDA_PORT                PORTB
#define SDA_PIN                 0
#define SCL_PORT                PORTB
#define SCL_PIN                 1
#define I2C_NOINTERRUPT         1
#include <SoftWire.h>
#define RTCEXT
#define RTCEXT32K
//#define RTCTEMP
#define LED
#define BUTTON
#define ONEWIRE
#define VOLTAGE
#define SEG14
#define DCF_PIN                 0  // PA0
#define TX_PIN                  1  // PA1
#define LED_PIN                 3  // PA3
#define RTC32K_PIN              4  // PA4
#define DCFPWR_PIN              5  // PA5
#define ONEWIRE_PIN             6  // PA6
#define BUTTON_PIN              7  // PA7
#define SDA_PIN                 10 // PB0
#define SCL_PIN                 9  // PB1
#else
#error "Wrong board selected!"
#endif

//#define SERIALDEBUG
#define DEBUG_PRINT(x)          Serial.print(F(x))
#define DEBUG_PRINTLN(x)        Serial.println(F(x))

#include "DS3231.h"
#include "AlphaDisplay.h"
#include "OneWire.h"
#include "DCF77.h"

#define DISPLAY_ADDRESS         0x70
#define DISPLAY_DIGITS          8
#define DISPLAY_BRIGHTNESS      4
#define ONEWIRE_RESOLUTION      11

const bool      syncAfterStart = true;
const uint8_t   readDelay = 10;
const uint16_t  voltageLimit = 3000;
const uint16_t  newSyncSeconds = 1800;

bool      syncReq, syncComplete, sens, voltage, buttonPressed, lowBatt, rtcOk;
uint8_t   timeState, showState, receiveState, syncState, readState, pinBitmaskButton;
int16_t   temp;
uint16_t  vcc;
volatile bool tickTock;

AlphaDisplay alpha;
dcf77 dcf;
timeStampDCF77 dcfTime;
DateTime dt;

#ifdef RTCEXT
RTC_DS3231 rtc;
#endif

#ifdef ONEWIRE
OneWire ow;
#endif

enum { TIME_NOTIME = 1, TIME_RTC, TIME_SYNC, TIME_SYNCED };
enum { RECEIVE_INIT = 1, RECEIVE_IDLE, RECEIVE_RECEIVING, RECEIVE_COMPLETE };
enum { SHOW_TIMEDATE = 1, SHOW_TIMEFULL, SHOW_TIMETEMP, SHOW_LOWBATT };
enum { SHOWSYNC_IDLE = 1, SHOWSYNC_MINUTEMARKER, SHOWSYNC_RECEIVING, SHOWSYNC_RECEIVECOMPL, SHOWSYNC_NOSIGNAL, SHOWSYNC_WAITSIGNAL };
enum { READ_IDLE = 1, READ_STARTCONV, READ_TEMP, READ_RTCTEMP, READ_VCC};

//----------------------------------------------------------------------------------

void setup() {
#ifdef SERIALDEBUG
  Serial.begin(9600);
  DEBUG_PRINTLN("\r\nInit...");
#endif

  Wire.begin();

  alpha.init(DISPLAY_ADDRESS, DISPLAY_DIGITS, DISPLAY_BRIGHTNESS);
  alpha.print("INIT");
  delay(1000);

#ifdef BUTTON
  // enable button input pin with pullup (active low)
  pinBitmaskButton = digitalPinToBitMask(BUTTON_PIN);
  DDRA &= ~pinBitmaskButton;
  PORTA |= pinBitmaskButton;
#endif

#ifdef ONEWIRE
  ow.setup(ONEWIRE_PIN);
  if (!ow.reset()) {
    ow.setResolution(ONEWIRE_RESOLUTION);
    alpha.print("TEMP OK ");
    delay(1000);
  }
#endif

  timeState = TIME_NOTIME;
#ifdef RTCEXT
  // initialize external RTC
  if (rtc.begin()) {
    if (rtc.lostPower()) {
      rtc.adjust(DateTime(2026, 1, 1, 12, 0, 0));
      alpha.print("RTC PWR ");
    }
    else {
      timeState = TIME_RTC;
      alpha.print("RTC OK  ");
    }
#ifdef RTCEXT32K
    rtc.enable32K();
#endif
    rtcOk = true;
  }
  else {
    alpha.print("RTC FAIL");
    rtcOk = false;
  }
  delay(1000);
#endif

#ifdef VOLTAGE
  ADCinit();
#endif
  // start DCF signal processing
  dcf.begin(DCF_PIN, DCFPWR_PIN, LED_PIN, RTC32K_PIN);

  receiveState = RECEIVE_INIT;
  syncState = SHOWSYNC_IDLE;
  showState = SHOW_TIMEDATE;
  readState = READ_IDLE;
  tickTock = false;
  buttonPressed = false;
  sens = false;
  voltage = false;
  lowBatt = false;
  vcc = voltageLimit;

#ifdef SERIALDEBUG
  DEBUG_PRINTLN("Init finished.");
#endif
}

//----------------------------------------------------------------------------------

void loop() {

  // handle DCF signal processing
  dcf.handle();

  // show sync animation and countdown
  syncState = handleAnimation(syncState);

  // state machine for receive data handling
  receiveState = handleReceive(receiveState);

  // state machine for time and display handling
  timeState = handleTime(timeState);

#ifdef BUTTON
  // switching display modes
  showState = handleButton(showState);
#endif

#if defined (ONEWIRE) || defined (VOLTAGE)
  // handle temp and voltage reading
  readState = handleReadTempVcc(readState);
#endif
  delay(2);
}

//----------------------------------------------------------------------------------

uint8_t handleAnimation(uint8_t state) {

  if (timeState != TIME_SYNC || dcf.getRequestState() == false) return SHOWSYNC_IDLE;

  switch (state) {
    case SHOWSYNC_IDLE:
      // wait for first signal
      showSync(SHOWSYNC_WAITSIGNAL);
      return SHOWSYNC_WAITSIGNAL;

    case SHOWSYNC_WAITSIGNAL:
      if (dcf.getSignalStatus() == dcf77::dcfSignal::GOOD) {
        showSync(SHOWSYNC_MINUTEMARKER);
        return SHOWSYNC_MINUTEMARKER;
      }
      if (dcf.getSignalStatus() == dcf77::dcfSignal::NOSIGNAL) {
        showSync(SHOWSYNC_NOSIGNAL);
        return SHOWSYNC_NOSIGNAL;
      }
      break;

    case SHOWSYNC_MINUTEMARKER:
      if (dcf.checkMinuteMarker()) {
        showSync(SHOWSYNC_RECEIVING);
        return SHOWSYNC_RECEIVING;
      }
      if (dcf.checkPulseReceived()) showSync(state);
      break;

    case SHOWSYNC_RECEIVING:
      if (dcf.checkComplete()) return SHOWSYNC_IDLE;
      if (dcf.checkRestart()) return SHOWSYNC_IDLE;
      if (dcf.checkReceiveBit()) showSync(state);
      break;

    case SHOWSYNC_NOSIGNAL:
      if (dcf.getSignalStatus() == dcf77::dcfSignal::WAIT) {
        showSync(SHOWSYNC_WAITSIGNAL);
        return SHOWSYNC_WAITSIGNAL;
      }
      break;
  }
  return state;
}

//----------------------------------------------------------------------------------

uint8_t handleReceive(uint8_t state) {
  bool tt = tickTock;
  static bool prevtt = false;
  static uint16_t newSyncCounter;

  switch (state) {
    case RECEIVE_INIT:
      newSyncCounter = 0;
      return RECEIVE_IDLE;

    case RECEIVE_IDLE:
      if (syncReq) {
        syncReq = false;
        syncComplete = false;
#ifdef SERIALDEBUG
        DEBUG_PRINTLN("Sync: Started");
#endif
        dcf.request();
        return RECEIVE_RECEIVING;
      }
      if (prevtt != tt) {
        newSyncCounter++;
        prevtt = tt;
      }
      if (newSyncCounter > newSyncSeconds && dt.second() == 50) syncReq = true;
      break;

    case RECEIVE_RECEIVING:
      if (dcf.checkComplete()) {
#ifdef SERIALDEBUG
        DEBUG_PRINTLN("Receive: Receive complete");
#endif
        return RECEIVE_COMPLETE;
      }
      break;

    case RECEIVE_COMPLETE:
      if (dcf.decode(&dcfTime) == dcf77::result::SUCCESS) {
#ifdef SERIALDEBUG
        DEBUG_PRINTLN("Receive: Decode successful");
#endif
        // update local time
        dt = DateTime(dcfTime.year, dcfTime.month, dcfTime.day, dcfTime.hour, dcfTime.minute, 0);
#ifdef RTCEXT
        rtc.adjust(dt);
#endif
#ifdef SERIALDEBUG
        DEBUG_PRINTLN("Receive: RTC synced to DCF-time");
#endif
        // job done
        syncComplete = true;
        newSyncCounter = 0;
        return RECEIVE_IDLE;
      }
      else {
#ifdef SERIALDEBUG
        DEBUG_PRINTLN("Receive: Decode failed!");
#endif
        // trigger new DCF receiving cycle
        dcf.request();
        return RECEIVE_RECEIVING;
      }
  }
  return state;
}

//----------------------------------------------------------------------------------

uint8_t handleTime(uint8_t state) {

  bool tt = tickTock;
  static bool prevtt = false;

  if (prevtt == tt) return state;
  prevtt = tt;

  // increase local time by one second
  if (tt) dt = dt + 1;

  switch (state) {
    case TIME_NOTIME:
      // with no RTC-time, wait for DCF-time
      syncReq = true;
      return TIME_SYNC;

#ifdef RTCEXT
    case TIME_RTC:
      // initial start with RTC time
      dt = rtc.now();
      if (syncAfterStart) syncReq = true;
      return TIME_SYNCED;
#endif
    case TIME_SYNC:
      if (syncComplete) {
        syncComplete = false;
        return TIME_SYNCED;
      }
      break;

    case TIME_SYNCED:
      showTime(showState, dt.hour(), dt.minute(), dt.second(), dt.month(), dt.day(), tt, dcf.getRequestState());
      break;
  }
  return state;
}

//----------------------------------------------------------------------------------

#ifdef BUTTON
uint8_t handleButton(uint8_t state) {
  bool button;
  static bool lastLowBatt = false;
  static bool stableState = false;
  static uint8_t confidenceCounter = 0;
  static uint8_t stableThreshold = 50;

  // read low-active button state
  button = (PINA & pinBitmaskButton) ? false : true;

  // debounce
  if (button == stableState) confidenceCounter = 0;
  else {
    confidenceCounter++;
    if (confidenceCounter >= stableThreshold) {
      stableState = button;
      if (stableState == true) buttonPressed = true;
    }
  }
  if (buttonPressed) {
    buttonPressed = false;
    switch (state) {
      case SHOW_TIMEDATE:
        lastLowBatt = false;
        return SHOW_TIMEFULL;

      case SHOW_TIMEFULL:
        if (sens) return SHOW_TIMETEMP;
        if (voltage) return SHOW_LOWBATT;
        return SHOW_TIMEDATE;

      case SHOW_TIMETEMP:
        if (voltage) return SHOW_LOWBATT;
        return SHOW_TIMEDATE;

      case SHOW_LOWBATT:
        return SHOW_TIMEDATE;
    }
  }
  // trigger display voltage only once
  if (lowBatt && lastLowBatt == false) {
    lastLowBatt = true;
    return SHOW_LOWBATT;
  }
  return state;
}
#endif

//----------------------------------------------------------------------------------
#if defined (ONEWIRE) || defined (RTCTEMP) || defined (VOLTAGE)
uint8_t handleReadTempVcc(uint8_t state) {

  bool tt = tickTock;
  static bool prevtt = false;
  static uint8_t delayCounter = 0;

  if (prevtt == tt) return state;
  prevtt = tt;

  // don't read values during DCF signal processing
  if (dcf.getRequestState()) return READ_IDLE;

  if (tt) {
    // this is called once a second
    switch (state) {
      case READ_IDLE:
        if (delayCounter++ >= readDelay) {

#if defined (ONEWIRE)
          return READ_STARTCONV;
#elif defined (RTCTEMP)
          return READ_RTCTEMP;
#elif defined (VOLTAGE)
          return READ_VCC;
#else
          return READ_IDLE;
#endif
        }
        break;

#ifdef ONEWIRE
      case READ_STARTCONV:
        ow.startConversion();
        return READ_TEMP;

      case READ_TEMP:
        temp = ow.readTemperature();
        if (temp != TEMP_ERROR) sens = true;
#ifdef SERIALDEBUG
        DEBUG_PRINTLN("DS18B20: Temperature: ");
        Serial.println(temp, DEC);
#endif
        return READ_VCC;
#endif

#ifdef RTCTEMP
      case READ_RTCTEMP:
        if (rtcOk) {
          temp = rtc.getTemperature();
          sens = true;
#ifdef SERIALDEBUG
          DEBUG_PRINT("DS3231: Temperature: ");
          Serial.println(temp, DEC);
#endif
        }
        return READ_VCC;
#endif

      case READ_VCC:
#ifdef VOLTAGE
        vcc = measureVoltage();
        voltage = true;
        lowBatt = false;
        if (vcc < voltageLimit) lowBatt = true;
#ifdef SERIALDEBUG
        DEBUG_PRINT("ADC: Voltage: ");
        Serial.println(vcc, DEC);
#endif
#endif
        delayCounter = 0;
        return READ_IDLE;
    }
  }
  return state;
}
#endif

//----------------------------------------------------------------------------------

void showSync(uint8_t mode) {
  bool dot;
  char buffer[DISPLAY_DIGITS + 1];
  uint8_t posRev;
  dcf77::dcfPulse pulse;
  dcf77::dcfPulseType pulseType;
  static dcf77::dcfPulse lastPulse = dcf77::dcfPulse::NONE;

  switch (mode) {
    // show receive bits during wait for minute marker
    case SHOWSYNC_MINUTEMARKER:
      pulse = dcf.getLastPulse();
      pulseType = dcf.getLastPulseType();
      strcpy(buffer, "SYNC    ");
      dot = false;
      if (pulseType == dcf77::dcfPulseType::START) dot = true;
      if (pulseType == dcf77::dcfPulseType::END) lastPulse = pulse;
      switch (lastPulse) {
        case dcf77::dcfPulse::BIT0: buffer[7] = '0'; break;
        case dcf77::dcfPulse::BIT1: buffer[7] = '1'; break;
        case dcf77::dcfPulse::NONE:
        case dcf77::dcfPulse::INVALID: buffer[7] = 'F'; break;
        case dcf77::dcfPulse::PAUSE: buffer[7] = 'P'; break;
      }
      buffer[7] |= (dot ? DOT : 0);
      break;

    // show countdown with number of receiving bits
    case SHOWSYNC_RECEIVING:
    case SHOWSYNC_RECEIVECOMPL:
      posRev = DCF_SIZE - dcf.getPos() - 1;
      strcpy(buffer, "RECV    ");
      if (posRev >= 10) buffer[6] = '0' + posRev / 10;
      buffer[7] = '0' + posRev % 10;
      break;

    case SHOWSYNC_WAITSIGNAL:
      strcpy(buffer, "WAIT    ");
      break;

    case SHOWSYNC_NOSIGNAL:
      strcpy(buffer, "NOSIGNAL");
  }
  buffer[DISPLAY_DIGITS] = 0;
  alpha.print(buffer);
}

//----------------------------------------------------------------------------------

void showTime(uint8_t mode, uint8_t hr, uint8_t min, uint8_t sec, uint8_t m, uint8_t d, bool dot, bool sync) {
  char buffer[DISPLAY_DIGITS + 1];

  // show time
  buffer[0] = hr >= 10 ? ('0' + hr / 10) : ' ';
  buffer[1] = ('0' + hr % 10) | (dot ? DOT : 0);
  buffer[2] = '0' + min / 10;
  buffer[3] = ('0' + min % 10) | (sync ? DOT : 0);
  buffer[DISPLAY_DIGITS] = 0;

  switch (mode) {
    case SHOW_TIMEDATE:
      if (d >= 10 && m >= 10) {
        buffer[4] = '0' + d / 10;
        buffer[5] = ('0' + d % 10) | DOT;
        buffer[6] = '0' + m / 10;
        buffer[7] = ('0' + m % 10) | DOT;
      }
      if (d >= 10 && m < 10) {
        buffer[4] = ' ';
        buffer[5] = '0' + d / 10;
        buffer[6] = ('0' + d % 10) | DOT;
        buffer[7] = ('0' + m) | DOT;
      }
      if (d < 10 && m >= 10) {
        buffer[4] = ' ';
        buffer[5] = ('0' + d) | DOT;
        buffer[6] = '0' + m / 10;
        buffer[7] = ('0' + m % 10) | DOT;
      }
      if (d < 10 && m < 10) {
        buffer[4] = ' ';
        buffer[5] = ' ';
        buffer[6] = ('0' + d) | DOT;
        buffer[7] = ('0' + m) | DOT;
      }
      break;

    case SHOW_TIMEFULL:
      buffer[3] |= DOT;
      buffer[4] = '0' + sec / 10;
      buffer[5] = ('0' + sec % 10) | (sync ? DOT : 0);
      buffer[6] = ' ';
      buffer[7] = ' ';
      break;

#if defined (ONEWIRE) || defined (RTCTEMP)
    case SHOW_TIMETEMP:
      {
        bool neg = false;
        int16_t temp2 = temp;

        if (temp < -990 || temp > 9999) {
          // temp reading error
          buffer[4] = '-';
          buffer[5] = '-' | DOT;
          buffer[6] = '-';
          buffer[7] = 'C';
          break;
        }
        if (temp < 0) {
          neg = true;
          temp2 = -temp;
        }
        buffer[4] = ' ';
        if (temp2 >= 1000) buffer[4] = '0' + temp2 / 1000;
        if (neg) buffer[4] = '-';
        buffer[5] = ('0' + ((temp2 / 100) % 10)) | DOT;
        buffer[6] = '0' + (temp2 / 10) % 10;
        buffer[7] = 'C';
        break;
      }
#endif
#ifdef VOLTAGE
    case SHOW_LOWBATT:
      buffer[4] = ' ';
      buffer[5] = ('0' + ((vcc + 50) / 1000)) | DOT;
      buffer[6] = '0' + (((vcc + 50) / 100) % 10);
      buffer[7] = 'V';
      break;
#endif
  }
  alpha.print(buffer);
}

//----------------------------------------------------------------------------------

#ifdef VOLTAGE
// configure internal ADC
void ADCinit(void) {
  // prescaler / clock setup for ADC
  ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1);
}

// read voltage, accumulate 16 samples
uint16_t measureVoltage(void) {
  uint8_t low, high;
  uint16_t adc, adcSum = 0;

  ADMUX = _BV(MUX5) | _BV(MUX0);
  delay(2);

  for (uint8_t i = 0; i < 16; i++) {
    ADCSRA |= _BV(ADSC);
    while (ADCSRA & (1 << ADSC));

    low  = ADCL;
    high = ADCH;
    adc = (high << 8) | low;
    adcSum += adc;
  }
  if (adcSum == 0) return 0;

  // calculate Vcc (in mV); 1125300 = 1.1 * 1023 * 1000
  return (uint16_t)(1125300L / (adcSum / 16));
}
#endif

// DCF77 interrupt handler, is called on every pulse edge
ISR(PCINT0_vect) {
  dcf.handleInt((PINA & dcf.pinBitmaskDcf) ? true : false);
}

// the Timer1 interrupt is called at 256 Hz
ISR(TIMER1_COMPA_vect) {
  static uint8_t tickDivider = 0;

  dcf.timerTick();

  // sub-divider to create 2 Hz
  tickDivider++;
  if (tickDivider >= 128) {
    tickDivider = 0;
    tickTock = !tickTock;
  }
}

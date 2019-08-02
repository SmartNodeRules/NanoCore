#if P005
//#######################################################################################################
//#################################### Plugin 005: Hue Kaku #############################################
//#######################################################################################################

// Custom dimmer using Ikea Zigbee module, bridge to Kaku transmitter to turn old light into Hue enabled
// Removed the module from the cheapest Tradfri GU10 spot and connected the output to an Arduino Nano that used to control a classic Kaku light

// This code could also be used as a starting point to create custom Hue/Tradfri compatible custom devices.

// Pin D2 connect to out pin from Zigbee module, used IRQ to measure the PWM width output from the module
// Pin D5 connect to TX pin on 433 MHz RF Transmitter

/*
 * Commands:
 * Hue2Kaku <address>,<channel>                         Use input PWM signal to control a Kaku receiver using given address and channel 
 * KakuSend <address>,<channel>,<value>,<retries>       Send an RF Kaku code from serial or rule command
*/

#define P005_HUE_THRESHOLD 25
#define P005_MAXVALUE 1400
#define P005_STABLECOUNTTHRESHOLD 5

unsigned long P005_address;
byte P005_channel;
boolean P005_init = false;
int P005_prevValue = 0;
int P005_setValue = 0;
byte P005_sendPrevValue = 0;
byte P005_sendValue = 0;
int P005_Tlow = 0;
int P005_Thigh = 0;
byte P005_stableCount = P005_STABLECOUNTTHRESHOLD + 1;

volatile unsigned long P005_pulseCount;
unsigned long P005_pulseCountPrevious;
volatile unsigned long P005_pulseTimer;
volatile unsigned long P005_pulseTimerPrevious;

// Kaku
#define PIN_RF_TX_VCC                4
#define PIN_RF_TX_DATA               5

#define PLUGIN_005
#define PLUGIN_ID_005         5

boolean Plugin_005(byte function, String& cmd, String& params)
{
  boolean success = false;

  switch (function)
  {
    case PLUGIN_INFO:
      {
        Serial.println(F("              P005 - Hue2Kaku"));
        break;
      }
          
    case PLUGIN_WRITE:
      {
        if (cmd.equalsIgnoreCase(F("Hue2Kaku")))
        {
          success = true;

          // kaku
          pinMode(PIN_RF_TX_DATA, OUTPUT);
          pinMode(PIN_RF_TX_VCC,  OUTPUT);
          P005_address = parseString(params,1).toInt();
          P005_channel = parseString(params,2).toInt();

          P005_init = true;
          pinMode(2, INPUT);
          attachInterrupt(0, P005_count, CHANGE);
        }

        if (cmd.equalsIgnoreCase(F("KakuSend")))
        {
          success = true;
          unsigned long address = parseString(params,1).toInt();
          byte channel = parseString(params,2).toInt();
          byte value = parseString(params,3).toInt();
          byte retries = parseString(params,4).toInt();
          P005_SendKaku(address, channel, value, retries);
        }

        break;
      }

    case PLUGIN_TEN_PER_SECOND:
      {
        if (P005_init)
        {
          P005_Handle_Hue();
        }
        break;
      }
  }
  return success;
}

void P005_Handle_Hue() {
  byte state = digitalRead(2);
  if (P005_pulseCount != P005_pulseCountPrevious) {
    P005_pulseCountPrevious = P005_pulseCount;
    // adjust value using threshold window
    int value = P005_pulseTimer;
    if (value > P005_MAXVALUE)
      value = P005_MAXVALUE;
    if (value < P005_Tlow || value > P005_Thigh) {
      P005_setValue = value;
      P005_Tlow = P005_setValue - P005_HUE_THRESHOLD;
      if (P005_Tlow < 0) P005_Tlow = 0;
      P005_Thigh = P005_setValue + P005_HUE_THRESHOLD;
      if (P005_Thigh > P005_MAXVALUE) P005_Thigh = P005_MAXVALUE;
    }
  }
  else {
    // We have solid 0 or 1
    if (state == 0) {
      P005_setValue = 0;
      P005_Tlow = 0;
      P005_Thigh = 0;
    }
    else {
      P005_setValue = P005_MAXVALUE;
    }
  }

  if (P005_setValue != P005_prevValue) {
    P005_stableCount = 0;
    P005_prevValue = P005_setValue;
  }
  else {
    if (P005_stableCount < P005_STABLECOUNTTHRESHOLD)
      P005_stableCount++;
    // wait for two equal readings before sending a new value
    if (P005_stableCount == 2){
      P005_sendValue = P005_setValue/100;
      // only send if the value is different from the previously send value
      if (P005_sendValue != P005_sendPrevValue)
        P005_SendKaku(P005_address, P005_channel, P005_sendValue, 1);
    }
    if (P005_stableCount == P005_STABLECOUNTTHRESHOLD) {
      // only send if the value is different from the previously send value
      if (P005_sendValue != P005_sendPrevValue)
        P005_SendKaku(P005_address, P005_channel, P005_sendValue, 10);
      // Store the new value
      P005_sendPrevValue = P005_sendValue;
      P005_stableCount++;
    }
  }
}

void P005_count() {
  byte state = digitalRead(2);
  if (state == 1) {
    P005_pulseCount++;
    P005_pulseTimerPrevious = micros();
  }
  else {
    P005_pulseTimer = micros() - P005_pulseTimerPrevious;
  }
}


// KAKU send stuff
void P005_SendKaku(unsigned long address, byte channel, byte value, byte retries) {

    Serial.print(F("KakuSend"));
    Serial.print(',');
    Serial.print(address);
    Serial.print(',');
    Serial.print(channel);
    Serial.print(',');
    Serial.print(value);
    Serial.print(',');
    Serial.println(retries);
    
    unsigned long bitstream = 0L;
    unsigned long tempaddress = 0L;
    byte cmd = 0;

    channel--;
    tempaddress = (address << 6) + channel; // Complete transmitted address
    bitstream = tempaddress & 0xFFFFFFCF;    // adres geheel over nemen behalve de twee bits 5 en 6 die het schakel commando bevatten.

    cmd = value;
    if (value == 0) {
      bitstream |= 0 << 4;  // bit-5 is the on/off command in the KAKU signal
      cmd = 0xff;
    }
    P005_AC_Send(bitstream, cmd, retries);
}


void P005_AC_Send(unsigned long data, byte cmd, byte retries) {

  int fpulse = 260;                               // Pulse width in microseconds
  int fretrans = retries;                              // Number of code retransmissions

  unsigned long bitstream = 0L;
  byte command;
  // prepare data to send
  for (unsigned short i = 0; i < 32; i++) {       // reverse data bits
    bitstream <<= 1;
    bitstream |= (data & B1);
    data >>= 1;
  }
  if (cmd != 0xff) {                              // reverse dim bits
    for (unsigned short i = 0; i < 4; i++) {
      command <<= 1;
      command |= (cmd & B1);
      cmd >>= 1;
    }
  }
  // Prepare transmit
  digitalWrite(PIN_RF_TX_VCC, HIGH);              // Enable the 433Mhz transmitter
  // send bits
  for (int nRepeat = 0; nRepeat <= fretrans; nRepeat++) {
    data = bitstream;
    if (cmd != 0xff) cmd = command;
    digitalWrite(PIN_RF_TX_DATA, HIGH);
    delayMicroseconds(335);
    digitalWrite(PIN_RF_TX_DATA, LOW);
    delayMicroseconds(fpulse * 10 + (fpulse >> 1)); //335*9=3015 //260*10=2600
    for (unsigned short i = 0; i < 32; i++) {
      if (i == 27 && cmd != 0xff) { // DIM command, send special DIM sequence TTTT replacing on/off bit
        digitalWrite(PIN_RF_TX_DATA, HIGH);
        delayMicroseconds(fpulse);
        digitalWrite(PIN_RF_TX_DATA, LOW);
        delayMicroseconds(fpulse);
        digitalWrite(PIN_RF_TX_DATA, HIGH);
        delayMicroseconds(fpulse);
        digitalWrite(PIN_RF_TX_DATA, LOW);
        delayMicroseconds(fpulse);
      } else
        switch (data & B1) {
          case 0:
            digitalWrite(PIN_RF_TX_DATA, HIGH);
            delayMicroseconds(fpulse);
            digitalWrite(PIN_RF_TX_DATA, LOW);
            delayMicroseconds(fpulse);
            digitalWrite(PIN_RF_TX_DATA, HIGH);
            delayMicroseconds(fpulse);
            digitalWrite(PIN_RF_TX_DATA, LOW);
            delayMicroseconds(fpulse * 5); // 335*3=1005 260*5=1300  260*4=1040
            break;
          case 1:
            digitalWrite(PIN_RF_TX_DATA, HIGH);
            delayMicroseconds(fpulse);
            digitalWrite(PIN_RF_TX_DATA, LOW);
            delayMicroseconds(fpulse * 5);
            digitalWrite(PIN_RF_TX_DATA, HIGH);
            delayMicroseconds(fpulse);
            digitalWrite(PIN_RF_TX_DATA, LOW);
            delayMicroseconds(fpulse);
            break;
        }
      //Next bit
      data >>= 1;
    }
    // send dim bits when needed
    if (cmd != 0xff) {                          // need to send DIM command bits
      for (unsigned short i = 0; i < 4; i++) { // 4 bits
        switch (cmd & B1) {
          case 0:
            digitalWrite(PIN_RF_TX_DATA, HIGH);
            delayMicroseconds(fpulse);
            digitalWrite(PIN_RF_TX_DATA, LOW);
            delayMicroseconds(fpulse);
            digitalWrite(PIN_RF_TX_DATA, HIGH);
            delayMicroseconds(fpulse);
            digitalWrite(PIN_RF_TX_DATA, LOW);
            delayMicroseconds(fpulse * 5); // 335*3=1005 260*5=1300
            break;
          case 1:
            digitalWrite(PIN_RF_TX_DATA, HIGH);
            delayMicroseconds(fpulse);
            digitalWrite(PIN_RF_TX_DATA, LOW);
            delayMicroseconds(fpulse * 5);
            digitalWrite(PIN_RF_TX_DATA, HIGH);
            delayMicroseconds(fpulse);
            digitalWrite(PIN_RF_TX_DATA, LOW);
            delayMicroseconds(fpulse);
            break;
        }
        //Next bit
        cmd >>= 1;
      }
    }
    //Send termination/synchronisation-signal. Total length: 32 periods
    digitalWrite(PIN_RF_TX_DATA, HIGH);
    delayMicroseconds(fpulse);
    digitalWrite(PIN_RF_TX_DATA, LOW);
    delayMicroseconds(fpulse * 40); //31*335=10385 40*260=10400
  }
  // End transmit
  digitalWrite(PIN_RF_TX_VCC, LOW);               // Turn thew 433Mhz transmitter off
}
#endif


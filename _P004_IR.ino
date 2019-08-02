#if P004
//#######################################################################################################
//#################################### Plugin 004: IR   ###############################################
//#######################################################################################################

// Use this plugin to read specific IR codes and store/clean/replay to output (in another room)

/*
 * Commands:
 * IRInit <IR out pin>,<mode>               Set an the output IR pin and the mode of working
 *                                            mode 0 = direct repeat input signal, rebuil 38KHz from internal oscillator
 *                                            mode 1 = rebuild IR signal using known protocol, limited to known devices
 * IRSend <device>,<code>                   Send an IR code from serial or rule command
*/
 
#define PLUGIN_004
#define PLUGIN_ID_004         4

#define P004_DEFAULT_IR_OUTPUT_PIN         11
#define P004_MIN_PULSE_LENGTH              50
#define P004_SIGNAL_TIMEOUT                 7
#define P004_RAW_BUFFER_SIZE              256
#define P004_IR_BUFFER_SIZE                80
#define P004_RAWSIGNAL_MULTIPLY             1

#define P004_INPUT_BUFFER_SIZE             80

#define P004_IR_SAMSUNG32_ID                1
#define P004_IR_SAMSUNG32_PULSECOUNT       67
#define P004_IR_SAMSUNG32_START          3000
#define P004_IR_SAMSUNG32_THRESHOLD       700 // 350/1500

#define P004_IR_HUMAX32_ID                  2
#define P004_IR_MARMITEK32_ID               3

#define P004_IR_SONY20_ID                   4
#define P004_IR_SONY20_PULSECOUNT          41
#define P004_IR_SONY20_START             2000
#define P004_IR_SONY20_THRESHOLD          900 // 650/1250

#define P004_IR_KAKU_ID                     5
#define P004_KAKU_CodeLength               12
#define P004_KAKU_T                       350

#define PIN_IR_RX_DATA                 3
#define PIN_IR_TX_DATA                11

// IR global vars
//volatile unsigned int RawSignal_pulses[IR_BUFFER_SIZE];
//volatile unsigned int RawSignal_RFBuffer[RAW_BUFFER_SIZE];
volatile unsigned int* RawSignal_pulses;
volatile unsigned int* RawSignal_RFBuffer;
volatile byte counter = 0;
volatile byte start = 0;
volatile byte size = 0;
volatile unsigned long lastPulse = 0;
volatile boolean enableIR = true;
volatile boolean startIR = false;
volatile boolean availableIR = false;
volatile byte counterMain = 0;
volatile boolean repeatIR = false;
unsigned long lastMessage = 0;
unsigned long lastCode = 0;
unsigned long test = 0;

byte P004_IROutPin = 11;
byte P004_modeIR = 1;
boolean P004_init = false;

boolean Plugin_004(byte function, String& cmd, String& params)
{
  boolean success = false;
  switch (function)
  {
    case PLUGIN_INFO:
      {
        Serial.println(F("              P004 - IR"));
        break;
      }

    case PLUGIN_WRITE:
      {
        if (cmd.equalsIgnoreCase(F("IRInit"))) // IRInit <pin>,<mode>
        {
          success = true;
          RawSignal_pulses = new volatile unsigned int [P004_IR_BUFFER_SIZE];
          RawSignal_RFBuffer = new volatile unsigned int [P004_RAW_BUFFER_SIZE];
          P004_init = true;
          P004_IROutPin = parseString(params, 1).toInt();
          P004_modeIR = parseString(params, 2).toInt();
          // Setup an 38kHz timer output to control IR LED, using timer 1
          if (P004_IROutPin == 9) {
            pinMode (9, OUTPUT);
            TCCR1A = 0;
            TCCR1B = _BV(WGM12) | _BV (CS10);   // CTC, No prescaler
            OCR1A =  209;          // compare A register value (210 * clock speed)
            //  = 13.125 nS , so frequency is 1 / (2 * 13.125) = 38095
          }
          if (P004_IROutPin == 11) {
            pinMode (11, OUTPUT);
            TCCR2A = 0;
            TCCR2B = _BV(WGM12) | _BV (CS10);   // CTC, No prescaler
            OCR2A =  209;          // compare A register value (210 * clock speed)
            //  = 13.125 nS , so frequency is 1 / (2 * 13.125) = 38095
          }
          pinMode(PIN_IR_RX_DATA, INPUT);
          pinMode(PIN_IR_TX_DATA, OUTPUT);
          digitalWrite(PIN_IR_TX_DATA, LOW);
          digitalWrite(PIN_IR_RX_DATA, INPUT_PULLUP);
          attachInterrupt(1, P004_IR_ISR, CHANGE);

          // Setup another ISR routine, hitch hike on existing millis() ISR frequency
          OCR0A = 0xAF;
          TIMSK0 |= _BV(OCIE0A);
          
          // Attach a handler extension to the millis() irq routine
          millisIRQCall_ptr = &P004_millis_irq;

        }

        if (cmd.equalsIgnoreCase(F("IRsend")))
        {
          success = true;
          String Device = parseString(params, 1);
          unsigned long Par2 = parseString(params, 2).toInt();
          if (Device.equalsIgnoreCase(F("samsung")))
            P004_sendIR(1, Par2);
          if (Device.equalsIgnoreCase(F("humax")))
            P004_sendIR(2, Par2);
          if (Device.equalsIgnoreCase(F("marmitek")))
            P004_sendIR(3, Par2);
          if (Device.equalsIgnoreCase(F("sony")))
            P004_sendIR(4, Par2);
          if (Device.equalsIgnoreCase(F("kaku")))
            P004_sendIR(5, Par2);
        }
        break;
      }

    case PLUGIN_100_PER_SECOND:
      {
        if (P004_init)
        {
          P004_handle_IR();
        }
        break;
      }

  }
  return success;
}


void P004_handle_IR() {
  if (availableIR) {
    unsigned long code = P004_decode();
    // Resend the message
    if (code) {
      if (P004_modeIR == 1)
        P004_RawSendIR();
    }
    availableIR = false;
  }
  if (repeatIR) {
    if (P004_modeIR == 1)
      P004_RawSendRepeatIR();
    repeatIR = false;
  }
}


void P004_sendIR(byte protocol, unsigned long bitstream) {

  // Samsung 32 bit protocol
  if (protocol == P004_IR_SAMSUNG32_ID)
  {
    RawSignal_pulses[0] = 4500 / P004_RAWSIGNAL_MULTIPLY;
    RawSignal_pulses[1] = 4500 / P004_RAWSIGNAL_MULTIPLY;
    for (byte x = 64; x >= 2; x = x - 2)
    {
      RawSignal_pulses[x] = 600 / P004_RAWSIGNAL_MULTIPLY;
      if ((bitstream & 1) == 1) RawSignal_pulses[x + 1] = 1600 / P004_RAWSIGNAL_MULTIPLY;
      else RawSignal_pulses[x + 1] = 500 / P004_RAWSIGNAL_MULTIPLY;

      bitstream = bitstream >> 1;
    }
    RawSignal_pulses[66] = 635 / P004_RAWSIGNAL_MULTIPLY;
    RawSignal_pulses[67] = 0;
    counterMain = 68;
    P004_RawSendIR();
  }


  // Humax or MARMITEK 32 bit protocol
  if (protocol == P004_IR_HUMAX32_ID || protocol == P004_IR_MARMITEK32_ID)
  {
    RawSignal_pulses[0] = 9000 / P004_RAWSIGNAL_MULTIPLY;
    RawSignal_pulses[1] = 4500 / P004_RAWSIGNAL_MULTIPLY;
    for (byte x = 64; x >= 2; x = x - 2)
    {
      RawSignal_pulses[x] = 635 / P004_RAWSIGNAL_MULTIPLY;
      if ((bitstream & 1) == 1) RawSignal_pulses[x + 1] = 1600 / P004_RAWSIGNAL_MULTIPLY;
      else RawSignal_pulses[x + 1] = 500 / P004_RAWSIGNAL_MULTIPLY;

      bitstream = bitstream >> 1;
    }
    RawSignal_pulses[66] = 635 / P004_RAWSIGNAL_MULTIPLY;
    RawSignal_pulses[67] = 0;
    counterMain = 68;
    P004_RawSendIR();
  }

  // Sony 20 Bit protocol
  if (protocol == P004_IR_SONY20_ID)
  {
    RawSignal_pulses[0] = 2200 / P004_RAWSIGNAL_MULTIPLY;
    for (byte x = 39; x >= 1 && x != 255; x = x - 2)
    {
      RawSignal_pulses[x] = 450 / P004_RAWSIGNAL_MULTIPLY;
      if ((bitstream & 1) == 1) RawSignal_pulses[x + 1] = 1000 / P004_RAWSIGNAL_MULTIPLY;
      else RawSignal_pulses[x + 1] = 500 / P004_RAWSIGNAL_MULTIPLY;

      bitstream = bitstream >> 1;
    }
    RawSignal_pulses[41] = 0;
    counterMain = 42;
    P004_RawSendIR();
  }
}


//********************************************************************************
//  Interrupt handler for IR messages
//********************************************************************************
void P004_IR_ISR()
{
  // in case of direct mode, toggle output based on input
  if (P004_modeIR == 0) {
    byte state = PIND & 8;
    if (state == 8) { // input high, output low
      if (P004_IROutPin == 9)
        TCCR1A = 0;
      if (P004_IROutPin == 11)
        TCCR2A = 0;
    }
    else { // input low, output high
      if (P004_IROutPin == 9)
        TCCR1A = 64;
      if (P004_IROutPin == 11)
        TCCR2A = 64;
    }
  }

  unsigned long TimeElapsedMillis = millis() - lastPulse;
  lastPulse = millis();
  static unsigned long TimeStamp = 0;
  unsigned long TimeElapsed = 0;

  TimeElapsed = micros() - TimeStamp;
  TimeStamp = micros();

  if (!startIR) {
    startIR = true;
    start = counter;
    return;
  }
  RawSignal_RFBuffer[counter] = TimeElapsed / P004_RAWSIGNAL_MULTIPLY;
  counter++;
}


//********************************************************************************
//  Will run every millisecond using ISR, check IR timeout
//********************************************************************************
//SIGNAL(TIMER0_COMPA_vect)
void P004_millis_irq()
{
  if (!P004_init) return;

  byte state = PIND & 8;

  // In case signal ends, detect timeout
  if (startIR && state == 8 && millis() - lastPulse > P004_SIGNAL_TIMEOUT) {
    size = counter - start;
    byte pos = start;

    if (size == 3) {
      if (RawSignal_RFBuffer[pos] > 8000 && RawSignal_RFBuffer[pos + 1] > 2000)
        repeatIR = true;
    }

    if (size > 8) {
      // copy ISR buffer to local buffer when free
      if (!availableIR) {
        for (byte x = 0; x < size; x++)
          RawSignal_pulses[x] = RawSignal_RFBuffer[pos++];
        counterMain = size;

        // signal main loop that a message is present
        availableIR = true;
      }
    }
    startIR = false;
  }
}


//********************************************************************************
//  Decode message
//********************************************************************************
unsigned long P004_decode() {

  byte protocol = 0;
  unsigned long bitstream = 0L;

  // Samsung 32 bit protocol
  if ((counterMain == P004_IR_SAMSUNG32_PULSECOUNT) && (RawSignal_pulses[0]*P004_RAWSIGNAL_MULTIPLY > P004_IR_SAMSUNG32_START) && (RawSignal_pulses[1]*P004_RAWSIGNAL_MULTIPLY > P004_IR_SAMSUNG32_START))
  {
    for (byte x = 3; x <= 65; x = x + 2)
    {
      if (RawSignal_pulses[x]*P004_RAWSIGNAL_MULTIPLY > P004_IR_SAMSUNG32_THRESHOLD) bitstream = (bitstream << 1) | 0x1;
      else bitstream = bitstream << 1;
    }
    byte device = bitstream >> 24;
    if (device == 0xE0) protocol = P004_IR_SAMSUNG32_ID;
    if (device == 0x40) protocol = P004_IR_MARMITEK32_ID;
    if (device == 0) protocol = P004_IR_HUMAX32_ID;
  }

  // Sony 20 bit protocol
  if ((counterMain == P004_IR_SONY20_PULSECOUNT) && (RawSignal_pulses[0]*P004_RAWSIGNAL_MULTIPLY > P004_IR_SONY20_START))
  {
    for (byte x = 2; x <= 40; x = x + 2)
    {
      if (RawSignal_pulses[x]*P004_RAWSIGNAL_MULTIPLY > P004_IR_SONY20_THRESHOLD) bitstream = (bitstream << 1) | 0x1;
      else bitstream = bitstream << 1;
    }
    protocol = P004_IR_SONY20_ID;
  }

  // Kaku protocol used by Harmony Remote
  if (counterMain == 49 || counterMain == 99) {
    int j;
    for (byte i = 0; i < P004_KAKU_CodeLength; i++)
    {
      j = (P004_KAKU_T * 2) / P004_RAWSIGNAL_MULTIPLY;

      if (RawSignal_pulses[4 * i] < j && RawSignal_pulses[4 * i + 1] > j && RawSignal_pulses[4 * i + 2] < j && RawSignal_pulses[4 * i + 3] > j) {
        bitstream = (bitstream >> 1);
      } // 0
      else if (RawSignal_pulses[4 * i] < j && RawSignal_pulses[4 * i + 1] > j && RawSignal_pulses[4 * i + 2] > j && RawSignal_pulses[4 * i + 3] < j) {
        bitstream = (bitstream >> 1 | (1 << (P004_KAKU_CodeLength - 1)));
      } // 1
      else if (RawSignal_pulses[4 * i] < j && RawSignal_pulses[4 * i + 1] > j && RawSignal_pulses[4 * i + 2] < j && RawSignal_pulses[4 * i + 3] < j) {
        bitstream = (bitstream >> 1);
      } // Short 0, Groep commando op 2e bit.
      else {
      } // foutief signaal
    }
    protocol = P004_IR_KAKU_ID;
    if (lastCode == bitstream && (millis() - lastMessage) < 100) {
      // Suppress repeated Kaku messages
      protocol = 0;
      bitstream = 0;
    }
  }

  lastMessage = millis();

  if (protocol == 0 || bitstream == 0)
    return 0;

  String event = "IR#";

  switch (protocol) {
    case P004_IR_SAMSUNG32_ID:
      {
        event += F("Samsung");
        break;
      }
    case P004_IR_SONY20_ID:
      {
        event += F("Sony");
        break;
      }
    case P004_IR_MARMITEK32_ID:
      {
        event += F("Marmitek");
        break;
      }
    case P004_IR_HUMAX32_ID:
      {
        event += F("Humax");
        break;
      }
    case P004_IR_KAKU_ID:
      {
        event += F("KAKU");
        break;
      }
  }

  event += ",";
  event += String(bitstream, HEX);

  rulesProcessing('r', event);

  lastCode = bitstream;
  return bitstream;
}


//********************************************************************************
//  Send IR message
//********************************************************************************

void P004_RawSendIR(void)
{
  RawSignal_pulses[counterMain] = 1;
  for (byte x = 0; x < counterMain; x += 2) {
    if (P004_IROutPin == 9)
      TCCR1A = 64;
    if (P004_IROutPin == 11)
      TCCR2A = 64;
    delayMicroseconds(RawSignal_pulses[x]*P004_RAWSIGNAL_MULTIPLY);
    if (P004_IROutPin == 9)
      TCCR1A = 0;
    if (P004_IROutPin == 11)
      TCCR2A = 0;
    delayMicroseconds(RawSignal_pulses[x + 1]*P004_RAWSIGNAL_MULTIPLY);
  }
}


void P004_RawSendRepeatIR(void)
{
  if (P004_IROutPin == 9)
    TCCR1A = 64;
  if (P004_IROutPin == 11)
    TCCR2A = 64;
  delayMicroseconds(9000);
  if (P004_IROutPin == 9)
    TCCR1A = 0;
  if (P004_IROutPin == 11)
    TCCR2A = 0;
  delayMicroseconds(2200);
  if (P004_IROutPin == 9)
    TCCR1A = 64;
  if (P004_IROutPin == 11)
    TCCR2A = 64;
  delayMicroseconds(600);
  if (P004_IROutPin == 9)
    TCCR1A = 0;
  if (P004_IROutPin == 11)
    TCCR2A = 0;
}

#endif

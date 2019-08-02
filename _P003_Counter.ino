#if P003
//#######################################################################################################
//#################################### Plugin 003 Counter ###############################################
//#######################################################################################################

/*
 * Commands:
 * counterSet <pin>                           Set an interrupt driven counter in input Ax (it uses the analog ports as digital counter)
 * counterGet <var count>,<var time>,<pin>    Read the count value and elapsed time between two pulses
*/

#define PLUGIN_003
#define PLUGIN_ID_003         3

// Counter global vars
volatile uint8_t *_receivePortRegister;
volatile unsigned long PulseCounter[4] = {0, 0, 0, 0};
volatile unsigned long PulseTimer[4] = {0, 0, 0, 0};
volatile unsigned long PulseTimerPrevious[4] = {0, 0, 0, 0};
volatile byte PulseState[4] = {0, 0, 0, 0};

boolean Plugin_003(byte function, String& cmd, String& params)
{
  boolean success = false;

  switch (function)
  {
    case PLUGIN_INFO:
      {
        Serial.println(F("              P003 - Counter"));
        break;
      }
          
    case PLUGIN_WRITE:
      {
        if (cmd.equalsIgnoreCase(F("CounterSet")))
        {
          success = true;
          byte Par1 = parseString(params, 1).toInt();
          if (Par1 == 1)
            SetPinIRQ(A0);
          if (Par1 == 2)
            SetPinIRQ(A1);
          if (Par1 == 3)
            SetPinIRQ(A2);
          if (Par1 == 4)
            SetPinIRQ(A3);
        }

        if (cmd.equalsIgnoreCase(F("CounterGet")))
        {
          byte Par1 = parseString(params, 1).toInt();
          String varName1 = parseString(params, 2);
          String varName2 = parseString(params, 3);
          
          // Store countervalue into uservar and reset internal counter
          unsigned long pulseCount = PulseCounter[Par1 - 1];
          PulseCounter[Par1 - 1] = 0;

          // If actual time difference > last known pulstime, update pulstimer now.
          if ((millis() - PulseTimerPrevious[Par1 - 1]) > PulseTimer[Par1 - 1])
            PulseTimer[Par1 - 1] = millis() - PulseTimerPrevious[Par1 - 1];
          unsigned long pulseTime = PulseTimer[Par1 - 1];

          if (varName1 != "")
            setNvar(varName1, pulseCount);
          if (varName2 != "")
            setNvar(varName2, pulseTime);

          /*
          String event = F("Counter#PulseCount");
          event += Par1;
          event += "=";
          event += pulseCount;
          Serial.println(event);
          rulesProcessing('r', event);
          delay(100);
          event = F("Counter#PulseTime");
          event += Par1;
          event += "=";
          event += pulseTime;
          Serial.println(event);
          rulesProcessing('r', event);
          */
        }
        break;
      }
  }
  return success;
}

// Counter handling
void SetPinIRQ(byte _receivePin)
{
  uint8_t _receiveBitMask;
  pinMode(_receivePin, INPUT);
  digitalWrite(_receivePin, HIGH);
  _receiveBitMask = digitalPinToBitMask(_receivePin);
  uint8_t port = digitalPinToPort(_receivePin);
  _receivePortRegister = portInputRegister(port);
  if (digitalPinToPCICR(_receivePin))
  {
    *digitalPinToPCICR(_receivePin) |= _BV(digitalPinToPCICRbit(_receivePin));
    *digitalPinToPCMSK(_receivePin) |= _BV(digitalPinToPCMSKbit(_receivePin));
  }
}

/*********************************************************************/
inline void pulse_handle_interrupt()
/*********************************************************************/
{
  byte portstate = *_receivePortRegister & 0xf;
  for (byte x = 0; x < 4; x++)
  {
    if ((portstate & (1 << x)) == 0)
    {
      if (PulseState[x] == 1)
      {
        PulseCounter[x]++;
        PulseTimer[x] = millis() - PulseTimerPrevious[x];
        PulseTimerPrevious[x] = millis();
      }
      PulseState[x] = 0;
    }
    else
      PulseState[x] = 1;
  }
  pulseLED = 1;
}

#if defined(PCINT1_vect)
ISR(PCINT1_vect) {
  pulse_handle_interrupt();
}
#endif
#if defined(PCINT2_vect)
ISR(PCINT2_vect) {
  pulse_handle_interrupt();
}
#endif

#endif

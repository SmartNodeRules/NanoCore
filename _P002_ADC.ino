#if P002
//#######################################################################################################
//#################################### Plugin 002: Analog ###############################################
//#######################################################################################################

/*
 * Commands:
 * analogRead <variable>,<pin>                   Read analog value into variable
 * analogDebug <pin>                             Show analog value every second on serial port
 * analogMonitor <pin>,<threshold>               Monitor analog port and create event if value changes above the set threshold window 
 *                                                 this outputs an event with the analog value
 *                                                 the threshold window can be set to filter out noise
 * levelSwitch <pin>,<value>,<threshold>,<LED>   Monitor analog port and create state event if value changes above the set threshold window 
 *                                                 this outputs an event wirh status 0 or 1 and switch the LED indicator
*/

#define PLUGIN_002
#define PLUGIN_ID_002         2
#define ANALOG_MAX 4

boolean P002_monitor = false;
boolean P002_level = false;
byte P002_monitorLED = 0;

boolean Plugin_002(byte function, String& cmd, String& params)
{
  boolean success = false;
  static int8_t  PinMonitor[ANALOG_MAX];
  static int16_t PinMonitorValue[ANALOG_MAX];
  static int16_t threshold[ANALOG_MAX];
  static int8_t  PinLevel[ANALOG_MAX];
  static boolean PinLevelState[ANALOG_MAX];
  static int8_t  PinDebug = -1;
  
  switch (function)
  {
    case PLUGIN_INFO:
      {
        Serial.println(F("              P002 - ADC"));
        break;
      }

    case PLUGIN_WRITE:
      {
        if (cmd.equalsIgnoreCase(F("AnalogRead")))
        {
          success = true;
          byte pin = parseString(params,2).toInt();
          String varName = parseString(params,1);
          int value = analogRead(pin);
          setNvar(varName, value);
        }

        if (cmd.equalsIgnoreCase(F("AnalogDebug")))
        {
          success = true;
          PinDebug = parseString(params,1).toInt();
        }

        if (cmd.equalsIgnoreCase(F("AnalogMonitor")))
        {
          success = true;
          P002_monitor = true;
          byte pin = parseString(params,1).toInt();
          int newthreshold = parseString(params,2).toInt();
          if (pin >= 0 && pin <= ANALOG_MAX)
          {
            threshold[pin] = newthreshold; 
            PinMonitor[pin] = 1;
          }
        }

        if (cmd.equalsIgnoreCase(F("LevelSwitch")))
        {
          success = true;
          P002_level = true;
          byte pin = parseString(params,1).toInt();
          int value = parseString(params,2).toInt();
          int newthreshold = parseString(params,3).toInt();
          byte stateLED = parseString(params,4).toInt();
          if (pin >= 0 && pin <= ANALOG_MAX)
          {
            PinMonitorValue[pin] = value;
            threshold[pin] = newthreshold;
            P002_monitorLED = stateLED;
            if(stateLED){
              pinMode(stateLED, OUTPUT);
            } 
            PinLevel[pin] = 1;
          }
        }

        break;
      }

    case PLUGIN_TEN_PER_SECOND:
      {
        if (PinDebug != -1){
          Serial.println(analogRead(PinDebug));
        }
        break;
      }
      
    case PLUGIN_100_PER_SECOND:
      {
        if (P002_monitor)
        {
          // port monitoring, on request by rule command
          for (byte x=0; x < ANALOG_MAX; x++)
             if (PinMonitor[x] != 0){
               int value = analogRead(x);
               if (abs(PinMonitorValue[x] - value) > threshold){
                 String event = F("Analog#");
                 event += x;
                 event += '=';
                 event += value;
                 rulesProcessing('r', event);
                 PinMonitorValue[x] = value;
               }
             }
        }

        if (P002_level)
        {
          // port level monitoring, on request by rule command
          for (byte x=0; x < ANALOG_MAX; x++)
             if (PinLevel[x] != 0){
               int value = analogRead(x);
               boolean change = false;
               if (!PinLevelState[x] && value > (PinMonitorValue[x] + threshold[x])){
                  PinLevelState[x] = true;
                  change = true;
               }
               if (PinLevelState[x] && value < (PinMonitorValue[x] - threshold[x])){
                  PinLevelState[x] = false;
                  change = true;
               }
               if(change){
                 if(P002_monitorLED)
                   digitalWrite(P002_monitorLED,PinLevelState[x]);
                 String event = F("Level#");
                 event += x;
                 event += '=';
                 event += PinLevelState[x];
                 rulesProcessing('r', event);
               }
             }
        }
        break;
      }
     
  }
  return success;
}
#endif

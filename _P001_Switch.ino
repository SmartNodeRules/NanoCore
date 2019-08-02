#if P001
//#######################################################################################################
//#################################### Plugin 001: Input Switch #########################################
//#######################################################################################################
 
/*
 * Commands:
 * gpio <pin>,<value>               Set gpio output
 * gpioRead <variable>,<pin>        Turn into input and read value
 * gpioDebug <pin>                  Show pinstate every second on serial
 * gpioState <variable>,<pin>       Read pin state without changing to input/ouput
 * gpioMonitor <pin>,<LED>          Monitor pin and create an event on change
*/

#define PLUGIN_001
#define PLUGIN_ID_001         1

#define GPIO_MAX 14
boolean P001_monitor = false;
byte P001_monitorLED = 0;

boolean Plugin_001(byte function, String& cmd, String& params)
{
  boolean success = false;
  static int8_t PinMonitor[GPIO_MAX];
  static int8_t PinMonitorState[GPIO_MAX];
  static int8_t PinDebug = -1;
    
  switch (function)
  {
    case PLUGIN_INFO:
      {
        Serial.println(F("              P001 - Switch"));
        break;
      }

    case PLUGIN_WRITE:
      {
        if (cmd.equalsIgnoreCase(F("gpio")))
        {
          success = true;
          byte pin = parseString(params,1).toInt();
          byte state = parseString(params,2).toInt();
          if (pin >= 0 && pin <= 13)
          {
            pinMode(pin, OUTPUT);
            digitalWrite(pin, state);
          }
        }

        if (cmd.equalsIgnoreCase(F("gpioRead")))
        {
          success = true;
          String varName = parseString(params,1);
          byte pin = parseString(params,2).toInt();
          pinMode(pin, INPUT);
          byte state = digitalRead(pin);
          setNvar(varName, state);
        }
        
        if (cmd.equalsIgnoreCase(F("gpioDebug")))
        {
          success = true;
          PinDebug = parseString(params,1).toInt();
        }
        
        if (cmd.equalsIgnoreCase(F("gpioState")))
        {
          success = true;
          String varName = parseString(params,1);
          byte pin = parseString(params,2).toInt();
          byte state = digitalRead(pin);
          setNvar(varName, state);
        }

        if (cmd.equalsIgnoreCase(F("gpioMonitor")))
        {
          success = true;
          P001_monitor = true;
          byte pin = parseString(params,1).toInt();
          byte stateLED = parseString(params,2).toInt();
          if (pin >= 0 && pin <= 13)
          {
            P001_monitorLED = stateLED;
            if(stateLED){
              pinMode(stateLED, OUTPUT);
            } 
            PinMonitor[pin] = 1;
          }
        }
                
        break;
      }

    case PLUGIN_TEN_PER_SECOND:
      {
        if (PinDebug != -1){
          Serial.println(digitalRead(PinDebug));
        }
        break;
      }
      
    case PLUGIN_100_PER_SECOND:
      {
        if (P001_monitor)
        {
          // port monitoring, on request by rule command
          for (byte x=0; x < GPIO_MAX; x++)
             if (PinMonitor[x] != 0){
               byte state = digitalRead(x);
               if (PinMonitorState[x] != state){
                 if(P001_monitorLED)
                   digitalWrite(P001_monitorLED,state);
                 String event = F("GPIO#");
                 event += x;
                 event += '=';
                 event += state;
                 rulesProcessing('r', event);
                 PinMonitorState[x] = state;
               }
             }
        }
        break;
      }      
  }
  return success;
}
#endif

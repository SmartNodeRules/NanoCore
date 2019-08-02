// NanoCore project
// ATMega328 as a small standalone controller or as coprocessor to ESP8266/ESP32
// Uses serial communication for manual control or by host processor
// Small rule engine that holds boot and event rules in EEPROM

/* List of core commands
Config,Name                                       Set the name of the device
Config,Debug                                      Enable/Disable debug logging
Config,WatchDog                                   Enable hardware watchdog
ledPulse <tenths of seconds>                      Set built-in LED on pin13 for some time
Echo <0|1>                                        Enable/Disable echo commands on serial port
RulesMax                                          Special command for ESP plugin to get EEPROM splitsize
RulesShow <b|r>                                   Show list of boot and rules in EEPROM
RulesLoad <b|r>                                   Load rules using serial port. End with "."
RulesClear <b|r>                                  Clear boot or rules section in EEPROM
SerialSend <msg>                                  Send message on serial port
Event <eventname>                                 Trigger event
ValueSet <name>,<value | expression>,<decimals>   Set a named variable with value or formula expression
ValueList                                         Show stored variables
TimerSet <nr>,<seconds>                           Enable a timer
Reboot                                            Reboot the device (warm restart only)
Settings                                          Show some settings and stats

Plugins commands are explained in the plugin sections

*/


//********************************************************************************
//  USER CONSTANTS, enable one of the predefined plugin sets or create new ones
//********************************************************************************

//#define FEATURE_SET_CORE
//#define FEATURE_SET_BASIC
#define FEATURE_SET_FULL

#ifdef FEATURE_SET_CORE
#endif

#ifdef FEATURE_SET_BASIC
  #define FEATURE_CALCULATE              true
  #define P001                           true
  #define P002                           true
#endif

#ifdef FEATURE_SET_FULL
  #define FEATURE_CALCULATE              true
  #define P001                           true
  #define P002                           true
  #define P003                           true
  #define P004                           true
  #define P005                           true
#endif

//********************************************************************************
//  SYSTEM CONSTANTS
//********************************************************************************

#define PROJECT "NanoCore Build 0.8"

#define EEPROM_BOOT_START_POS               0
#define EEPROM_BOOT_SIZE                  256
#define EEPROM_RULES_START_POS            256
#define EEPROM_RULES_SIZE                 768

#define RULES_TIMER_MAX                     4
#define PLUGIN_MAX                          8
#define USER_VAR_MAX                        4

#define PLUGIN_INIT                         2
#define PLUGIN_READ                         3
#define PLUGIN_ONCE_A_SECOND                4
#define PLUGIN_TEN_PER_SECOND               5
#define PLUGIN_WRITE                       13
#define PLUGIN_100_PER_SECOND          25
#define PLUGIN_INFO                       255

#include <EEPROM.h>
#include <avr/wdt.h>


//********************************************************************************
//  Global variables
//********************************************************************************
struct SettingsStruct
{
  char          Name[26];
  byte          Debug=0;  
  byte          WatchDog=0;
} Settings;

struct nvarStruct
{
  String Name;
  float Value;
  byte Decimals;
} nUserVar[USER_VAR_MAX];

void(*Reboot)(void)=0;
void setNvar(String varName, float value, int decimals = -1);
String parseString(String& string, byte indexFind, char separator = ',');

boolean (*Plugin_ptr[PLUGIN_MAX])(byte, String&, String&);
byte Plugin_id[PLUGIN_MAX];
String dummyString="";

volatile byte timer100PS = 0;
volatile byte timer10PS = 0;
volatile byte timer1PS = 0;
volatile boolean run100PS = false;
volatile boolean run10PS = false;
volatile boolean run1PS = false;
volatile boolean run1PM = false;

unsigned long RulesTimer[RULES_TIMER_MAX];
unsigned long uptime = 0;
unsigned long loopCounter = 0;
unsigned long loopCounterLast=0;

volatile byte pulseLED = 0;
boolean LEDstate = false;

boolean loadEEPROM = false;
int loadEEPROMPos = 0;
boolean echo = false;

void (*millisIRQCall_ptr)(void);


//********************************************************************************
//  Setup
//********************************************************************************
void setup()
{
  Serial.begin(115200);
  String event = F("System#Config");
  rulesProcessing('b', event);

  PluginInit();

  event = F("System#Boot");
  rulesProcessing('b', event);
  rulesProcessing('r', event);

  // Setup another ISR routine, hitch hike on existing millis() ISR frequency
  OCR0A = 0xAF;
  TIMSK0 |= _BV(OCIE0A);

}


//********************************************************************************
//  Main Loop
//********************************************************************************
void loop()
{
  if (Serial.available()) serial();
  if (run100PS)           hundred_per_second();
  if (run10PS)            ten_per_second();
  if (run1PS)             once_per_second();
  if (run1PM)             once_per_minute();
  loopCounter++;
}


//********************************************************************************
//  Will run every millisecond using ISR
//********************************************************************************
SIGNAL(TIMER0_COMPA_vect){
  static byte count = 0;
  count++;
  if(count == 10){
    count=0;
    run100PS = true;
    timer100PS++;
    if(timer100PS == 10){
      timer100PS = 0;
      run10PS = true;
      timer10PS++;
      if(timer10PS == 10){
        timer10PS = 0;
        run1PS = true;
        timer1PS++;
        if(timer1PS == 60){
          timer1PS = 0;
          run1PM = true;
        }
      }
    }
  }

  if(millisIRQCall_ptr)
    millisIRQCall_ptr();
  // sample how to use in a plugin:  millisIRQCall_ptr=&P004_millis_irq;

}


//********************************************************************************
//  Will run 100 times/sec
//********************************************************************************
void hundred_per_second(){
  run100PS = false;
  PluginCall(PLUGIN_100_PER_SECOND, dummyString,  dummyString);
}


//********************************************************************************
//  Will run 10 times/sec
//********************************************************************************
void ten_per_second(){
  run10PS = false;
  PluginCall(PLUGIN_TEN_PER_SECOND, dummyString,  dummyString);

  // When countdown finished and LED is on: Turn off
  if(pulseLED == 0 && LEDstate){
    digitalWrite(13,0);
    LEDstate = false;
  }

  // LED pulse counter set
  if(pulseLED != 0){
    if(!LEDstate){ // if LED off: turn LED on
      pinMode(13,OUTPUT);
      digitalWrite(13,1);
      LEDstate = true;
    }
    pulseLED--; // decrease counter by one
  }
}


//********************************************************************************
//  Will run 1 time/sec
//********************************************************************************
void once_per_second(){
  run1PS = false;
  if(Settings.WatchDog)
    wdt_reset();
  PluginCall(PLUGIN_ONCE_A_SECOND, dummyString,  dummyString);
  rulesTimers();
  loopCounterLast = loopCounter;
  loopCounter = 0;
}


//********************************************************************************
//  Will run 1 time/minute
//********************************************************************************
void once_per_minute(){
  run1PM = false;
  uptime++;
}


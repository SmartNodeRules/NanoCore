#define PROJECT "NanoCore Build 0.5"

#define DEBUG false

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
#endif

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
#define PLUGIN_UNCONDITIONAL_POLL          25
#define PLUGIN_INFO                       255

#include <EEPROM.h>
#include <avr/wdt.h>
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

unsigned long timer100ms = 0;
unsigned long timer1s = 0;
unsigned long timer60s = 0;
unsigned long RulesTimer[RULES_TIMER_MAX];
unsigned long uptime = 0;
unsigned long loopCounter = 0;
unsigned long loopCounterLast=0;

boolean loadEEPROM = false;
int loadEEPROMPos = 0;
boolean echo = false;

void setup()
{
  Serial.begin(115200);
  String event = F("System#Config");
  rulesProcessing('b', event);

  timer100ms = millis() + 100;
  timer1s = millis() + 1000;
  timer60s = millis() + 60000;
  
  PluginInit();

  event = F("System#Boot");
  rulesProcessing('b', event);
  rulesProcessing('r', event);
}

void loop()
{
  if (Serial.available())
    serial();

  PluginCall(PLUGIN_UNCONDITIONAL_POLL, dummyString,  dummyString);

  if (millis() > timer100ms)
  {
    timer100ms = millis() + 100;
    PluginCall(PLUGIN_TEN_PER_SECOND, dummyString,  dummyString);
  }

  if (millis() > timer1s)
  {
    timer1s = millis() + 1000;
    if(Settings.WatchDog)
      wdt_reset();
    PluginCall(PLUGIN_ONCE_A_SECOND, dummyString,  dummyString);
    rulesTimers();
    loopCounterLast = loopCounter;
    loopCounter = 0;
  }

  if (millis() > timer60s)
  {
    timer60s = millis() + 60000;
    uptime++;
  }
  loopCounter++;
}


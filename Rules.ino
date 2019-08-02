//********************************************************************************
//  Rule engine section
//********************************************************************************


//********************************************************************************
//  List Rules to serial port
//********************************************************************************
void listRules(char section) {
  String rule = "";

  int eepromPos = EEPROM_BOOT_START_POS;
  int maxSize = EEPROM_BOOT_SIZE;
  if (section == 'r'){
    eepromPos = EEPROM_RULES_START_POS;
    maxSize = EEPROM_RULES_SIZE;
  }

  int x = 0;
  for (x = eepromPos; x < eepromPos + maxSize; x++) {
    byte data = EEPROM.read(x);
    if (data == 3)
      break;
    if (data != 0)
      rule += (char)data;
    else
    { // rule complete
      Serial.println(rule);
      delay(10);
      rule = "";
    }
  }
  Serial.write(0); // to assist automated read/program, terminate with zero byte
}


//********************************************************************************
//  Load Rules from serial port (also uses logic contained in serial section)
//********************************************************************************
void loadRules(char section, boolean clear) {
  if (section != 'b' && section != 'r'){
    loadEEPROM = false;
    return;
  }

  int eepromPos = EEPROM_BOOT_START_POS;
  int maxSize = EEPROM_BOOT_SIZE;
  if (section == 'r'){
    eepromPos = EEPROM_RULES_START_POS;
    maxSize = EEPROM_RULES_SIZE;
  }

  if(clear){
    EEPROM.update(eepromPos, 3);
    return;
  }
  
  loadEEPROM = true;
  int x = 0;
  for (x = eepromPos; x < eepromPos + maxSize; x++) {
    byte data = EEPROM.read(x);
    if (data == 3){
      loadEEPROMPos = x;
      break;
    }
  }
}


//********************************************************************************************
//  Check rule timers
//********************************************************************************************
void rulesTimers()
{
  for (byte x = 0; x < RULES_TIMER_MAX; x++)
  {
    if (RulesTimer[x] != 0L) // timer active?
    {
      if (RulesTimer[x] < millis()) // timer finished?
      {
        RulesTimer[x] = 0L; // turn off this timer
        String event = F("Timer#");
        event += x + 1;
        rulesProcessing('r',event);
      }
    }
  }
}


//********************************************************************************************
//  Process an event
//********************************************************************************************
void rulesProcessing(char section, String& event) {
  unsigned long timer=millis();
  boolean match = false;
  boolean codeBlock = false;
  boolean isCommand = false;

  if(Settings.Debug){
    Serial.print(F("EVT: "));
    Serial.println(event);
  }

  String rule = "";
  rule.reserve(40);
  String action = "";

  int eepromPos = EEPROM_BOOT_START_POS;
  int maxSize = EEPROM_BOOT_SIZE;
  if (section == 'r'){
    eepromPos = EEPROM_RULES_START_POS;
    maxSize = EEPROM_RULES_SIZE;
  }
  
  int x = 0;
  for (x = eepromPos; x < eepromPos + maxSize; x++) {
    byte data = EEPROM.read(x);
    if (data == 3)
      break;
    if (data != 0)
      rule += (char)data;
    else
    { // rule complete
      if(rule.indexOf('%') != -1)
        rule = parseTemplate(rule, rule.length());
      action = rule;
      isCommand = true;
      if (!codeBlock)
      {
        rule.toLowerCase();
        if (rule.startsWith(F("on ")))
        {
          rule = rule.substring(3);
          int split = rule.indexOf(F(" do"));
          if (split != -1)
            rule = rule.substring(0, split);

          match = ruleMatch(event, rule);
          if (match) {
            isCommand = false;
            codeBlock = true;
          }
        }
      }
      
      if (rule == F("endon"))
        {
          isCommand = false;
          codeBlock = false;
          match = false;
        }

      if (match && isCommand){
        action.trim();
        action.replace(F("%event%"), event);
        if(Settings.Debug){
          Serial.print(F("ACT: "));
          Serial.println(action);
        }
        ExecuteCommand(action.c_str());
      }
      rule = "";
    } // rule processing
  }
  if(Settings.Debug){
    Serial.print(F("Rules time:"));
    Serial.println(millis()-timer);
  }
}


//********************************************************************************************
//  Check if an event matches a specific rule
//********************************************************************************************
boolean ruleMatch(String& event, String& rule)
{
  if (rule == "*")
    return true;
    
  boolean match = false;
  String tmpEvent = event;
  String tmpRule = rule;

  int pos = rule.indexOf('*');
  if (pos != -1) // a * sign in rule, so use a'wildcard' match on message
    {
      tmpEvent = event.substring(0, pos);
      tmpRule = rule.substring(0, pos);
      return tmpEvent.equalsIgnoreCase(tmpRule);
    }
 return rule.equalsIgnoreCase(event);
}
  

//********************************************************************************************
//  Parse string template
//********************************************************************************************
String parseTemplate(String &tmpString, byte lineSize)
{
  String newString = tmpString;

  // check named uservars
  for (byte x = 0; x < USER_VAR_MAX; x++) {
    String varname = "%" + nUserVar[x].Name + "%";
    String svalue = toString(nUserVar[x].Value, nUserVar[x].Decimals);
    newString.replace(varname, svalue);
  }

  // check named uservar strings
//  for (byte x = 0; x < USER_STRING_VAR_MAX; x++) {
//    String varname = "%" + sUserVar[x].Name + "%";
//    String svalue = String(sUserVar[x].Value);
//    newString.replace(varname, svalue);
//  }

  newString.replace(F("%sysname%"), Settings.Name);
//  newString.replace(F("%systime%"), getTimeString(':'));

  return newString;
}


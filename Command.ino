#define MAX_ARG_SIZE 40
#define MAX_COMMAND_SIZE 20

void ExecuteCommand(const char *Line)
{
#if DEBUG
  Serial.print(F("C1 "));
  Serial.println(freeRam());
#endif
  // first check the plugins
  String cmd = Line;
  String params = "";
  int parampos = getParamStartPos(cmd, 2);
  if (parampos != -1) {
    params = cmd.substring(parampos);
    cmd = cmd.substring(0, parampos - 1);
  }
  if (PluginCall(PLUGIN_WRITE, cmd, params)) {
    return;
  }
  cmd = "";
  params = "";

#if DEBUG
  Serial.print("C2 ");
  Serial.println(freeRam());
#endif
  boolean success = false;
  char TmpStr1[MAX_ARG_SIZE + 2];
  TmpStr1[0] = 0;
  char Command[MAX_COMMAND_SIZE + 2];
  Command[0] = 0;
  unsigned long Par1 = 0;
  unsigned long Par2 = 0;
  unsigned long Par3 = 0;
  GetArgv(Line, Command, 1, 20);
  if (GetArgv(Line, TmpStr1, 2, MAX_ARG_SIZE)) Par1 = str2int(TmpStr1);
  if (GetArgv(Line, TmpStr1, 3, MAX_ARG_SIZE)) Par2 = str2int(TmpStr1);
  if (GetArgv(Line, TmpStr1, 4, MAX_ARG_SIZE)) Par3 = str2int(TmpStr1);

  // Config commands
  if (strcasecmp_P(Command, PSTR("Config")) == 0)
  {
    success = true;
    if (GetArgv(Line, TmpStr1, 2, MAX_ARG_SIZE)) {

      if (strcasecmp_P(TmpStr1, PSTR("Name")) == 0) {
        if (GetArgv(Line, TmpStr1, 3, MAX_ARG_SIZE)) {
          strcpy(Settings.Name, TmpStr1);
        }
      }

      if (strcasecmp_P(TmpStr1, PSTR("Debug")) == 0) {
        Settings.Debug = Par2;
      }

      if (strcasecmp_P(TmpStr1, PSTR("WatchDog")) == 0) {
        Settings.WatchDog = 1;
        wdt_enable(WDTO_8S);
      }
    }
    String strLine = Line;
  }

  if (strcasecmp_P(Command, PSTR("Echo")) == 0)
  {
    success = true;
    echo = Par1;
  }

  if (strcasecmp_P(Command, PSTR("RulesMax")) == 0 || strcasecmp_P(Command, PSTR("RM")) == 0)
  {
    success = true;
    if (GetArgv(Line, TmpStr1, 2, MAX_ARG_SIZE))
      if (TmpStr1[0] == 'b')
        Serial.println(EEPROM_BOOT_SIZE);
      else
        Serial.println(EEPROM_RULES_SIZE);
  }

  if (strcasecmp_P(Command, PSTR("RulesShow")) == 0 || strcasecmp_P(Command, PSTR("RS")) == 0)
  {
    success = true;
    if (GetArgv(Line, TmpStr1, 2, MAX_ARG_SIZE))
      listRules(TmpStr1[0]);
    else {
      Serial.println(F("Boot:"));
      Serial.println(F("====="));
      listRules('b');
      Serial.println(F("Rules:"));
      Serial.println(F("======"));
      listRules('r');
    }
  }

  if (strcasecmp_P(Command, PSTR("RulesLoad")) == 0 || strcasecmp_P(Command, PSTR("RL")) == 0)
  {
    success = true;
    if (GetArgv(Line, TmpStr1, 2, MAX_ARG_SIZE))
      loadRules(TmpStr1[0], false);
  }

  if (strcasecmp_P(Command, PSTR("RulesClear")) == 0 || strcasecmp_P(Command, PSTR("RC")) == 0)
  {
    success = true;
    if (GetArgv(Line, TmpStr1, 2, MAX_ARG_SIZE))
      loadRules(TmpStr1[0], true);
  }

  if (strcasecmp_P(Command, PSTR("SerialSend")) == 0)
  {
    success = true;
    Serial.println(Line + 11);
  }

  if (strcasecmp_P(Command, PSTR("Event")) == 0)
  {
    success = true;
    String event = Line;
    event = event.substring(6);
    rulesProcessing('r', event);
  }

  if (strcasecmp_P(Command, PSTR("ValueSet")) == 0)
  {
    success = true;
    if (GetArgv(Line, TmpStr1, 3, MAX_ARG_SIZE))
    {
      float result = 0;
#if FEATURE_CALCULATE
      Calculate(TmpStr1, &result);
#else
      result = atof(TmpStr1);
#endif

      if (GetArgv(Line, TmpStr1, 2, MAX_ARG_SIZE)) {
        String varName = TmpStr1;
        if (GetArgv(Line, TmpStr1, 4, MAX_ARG_SIZE))
          setNvar(varName, result, Par3);
        else
          setNvar(varName, result);
      }
    }
  }

  if (strcasecmp_P(Command, PSTR("ValueList")) == 0)
  {
    success = true;
    for (byte x = 0; x < USER_VAR_MAX; x++) {
      if (nUserVar[x].Name != "") {
        Serial.print(nUserVar[x].Name);
        Serial.print(":");
        Serial.println(nUserVar[x].Value);
      }
    }
  }

  if (strcasecmp_P(Command, PSTR("TimerSet")) == 0)
  {
    success = true;
    if (Par2)
      RulesTimer[Par1 - 1] = millis() + (1000 * Par2);
    else
      RulesTimer[Par1 - 1] = 0;
  }

  if (strcasecmp_P(Command, PSTR("Reboot")) == 0)
  {
    success = true;
    delay(10);
    Reboot();
  }

  if (strcasecmp_P(Command, PSTR("Settings")) == 0)
  {
    success = true;
    Serial.print(F("Project     : ")); Serial.println(F(PROJECT));
    Serial.print(F("Device Name : ")); Serial.println(Settings.Name);
    Serial.print(F("Free RAM    : ")); Serial.println(freeRam());
    Serial.print(F("Uptime      : ")); Serial.println(uptime);
    Serial.print(F("LoopCount   : ")); Serial.println(loopCounterLast);
    Serial.println(F("Plugins     : "));
    PluginCall(PLUGIN_INFO, dummyString,  dummyString);
  }

  if (Settings.Debug) {
    Serial.println("OK");
  }
}

//********************************************************************************************
//  Find positional parameter in a char string
//********************************************************************************************
boolean GetArgv(const char *string, char *argv, int argc, byte max)
{
  int string_pos = 0, argv_pos = 0, argc_pos = 0;
  char c, d;

  while (string_pos < strlen(string))
  {
    c = string[string_pos];
    d = string[string_pos + 1];

    if       (c == ' ' && d == ' ') {}
    else if  (c == ' ' && d == ',') {}
    else if  (c == ',' && d == ' ') {}
    else if  (c == ' ' && d >= 33 && d <= 126) {}
    else if  (c == ',' && d >= 33 && d <= 126) {}
    else
    {
      // fix to avoid overrun of arg string
      if (argv_pos > max) {
        argv[0] = 0;
        if (Settings.Debug)
          Serial.println(F("Buffer Overrun"));
        return false;
      }

      argv[argv_pos++] = c;
      argv[argv_pos] = 0;

      if (d == ' ' || d == ',' || d == 0)
      {
        argv[argv_pos] = 0;
        argc_pos++;

        if (argc_pos == argc)
        {
          return true;
        }

        argv[0] = 0;
        argv_pos = 0;
        string_pos++;
      }
    }
    string_pos++;
  }
  return false;
}


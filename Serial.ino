//********************************************************************************
//  Serial input handler
//********************************************************************************
#define INPUT_BUFFER_SIZE          80

byte SerialInByte;
int SerialInByteCounter = 0;
char InputBuffer_Serial[INPUT_BUFFER_SIZE + 2];

void serial()
{
  while (Serial.available())
  {
    SerialInByte = Serial.read();

    if(SerialInByte == 0x30){
      delay(1);
      if (Serial.available() == 1 && Serial.peek() == 0x20){
        pinMode(6, OUTPUT);
        digitalWrite(6, LOW);
      }
    }
      
    if (SerialInByte == 255) // binary data...
    {
      Serial.flush();
      return;
    }

    if (loadEEPROM && SerialInByte == 0){ // load to eeprom will terminate on 0 byte
      EEPROM.update(loadEEPROMPos, 3); // write end of text marker
      loadEEPROM = false;
      Serial.flush();
      return;
    }

    if (isprint(SerialInByte))
    {
      if (SerialInByteCounter < INPUT_BUFFER_SIZE) // add char to string if it still fits
        InputBuffer_Serial[SerialInByteCounter++] = SerialInByte;
    }

    if (SerialInByte == '\n')
    {
      InputBuffer_Serial[SerialInByteCounter] = 0; // serial data completed
      if(!loadEEPROM && echo){
        Serial.write('>');
        Serial.println(InputBuffer_Serial);
      }
      if (loadEEPROM){
        if (InputBuffer_Serial[0] == '.'){
          EEPROM.update(loadEEPROMPos, 3); // write end of text marker
          loadEEPROM = false;
          Serial.println(F("End Upload"));
        }
        else{
          for(int x = 0; x < SerialInByteCounter; x++)
            EEPROM.update(loadEEPROMPos++, InputBuffer_Serial[x]);
          EEPROM.update(loadEEPROMPos++, 0);
          Serial.write(0x0D);
        }
      }
      else{
        InputBuffer_Serial[SerialInByteCounter] = 0;
        ExecuteCommand(InputBuffer_Serial);
      }
      SerialInByteCounter = 0;
      InputBuffer_Serial[0] = 0; // serial data processed, clear buffer
    }
  }
}


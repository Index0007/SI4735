/*
  This sketch stores the SI4735 SSB patch content on an EEPROM.
  It might useful for Board or MCU with few memory.
  The Idea is saving memory of your MCU by storing a SI47XX patche in an external memory (EEPROM)

  The PU2CLR Si4735 Arduino Library has the downloadPatchFromEeprom methods that can be used to load the eeprom 
  content generated by this sketch.

  Attention:The full ssb patch needs about 16KB on eeprom. 
            All data that you have stored before into eeprom will be lost after the execution of this sketch. 
            Follow the comments written below. 

  This sketch was tested on "AT24C256 Serial I2C Interface EEPROM Data Storage Module"          

  Ricardo Lima Caratti 2020
*/

#include <Wire.h>
#include <SI4735.h>

// What SSB patch do you want? 
// Uncoment the line with the patch you want to use.
#include "patch_init.h"    // SSB patch for whole SSBRX initialization string
// #include "patch_full.h" // SSB patch for whole SSBRX initialization string

const uint16_t size_content = sizeof ssb_patch_content; // see ssb_patch_content in patch_full.h or patch_init.h

//defines the EEPROM I2C addresss.
#define EEPROM_I2C_ADDR 0x50 // You might need to change this value

char buffer(80);

si4735_eeprom_patch_header eep; // EEPROM header structure

const int header_size = sizeof eep;

const uint8_t status_id[] = " 4735"; // Not used  for while. 
const uint8_t content_id[] = "patch-init   \0"; // 14 bytes
// ATTENTION: Comment the line above and uncomment the line below if you want to use the full patch
// const uint8_t content_id[] = "patch-full   \0"; // 16 bytes
const uint16_t size_id = sizeof content_id - 1;

void setup()
{
  Serial.begin(9600);
  while (!Serial)
    ;

  // Storing the patch header information
  strcpy((char *)eep.refined.patch_id, (char *)content_id);
  eep.refined.patch_size = size_content;

  showMsg("Storing the patch file..");

  showMsgText("Patch name.............: %s", eep.refined.patch_id);
  showMsgValue("Size of patch header...: %u bytes.", header_size);
  showMsgValue("Size of patch content: %u bytes.", eep.refined.patch_size);

  uint32_t t1 = millis();
  eepromWritePatch();
  // Comment the line above and uncomment the line below if you want to clean your eeprom.
  // clearEeprom();
  uint32_t t2 = millis();
  showMsgValue("Finish! Elapsed time: %ul milliseconds.", t2 - t1);
  showMsg("Checking the values stored");
  checkPatch();
  showMsg("Finish");
}

void showMsg(const char *msg)
{
  Serial.println(msg);
}

void showMsgValue(const char *msg, uint16_t value)
{
  char buffer[80];
  sprintf(buffer, msg, value);
  Serial.println(buffer);
}

void showMsgText(const char *msg, uint8_t *text)
{
  char buffer[80];
  sprintf(buffer, msg, text);
  Serial.println(buffer);
}

void eepromWritePatch()
{
  uint8_t content[8];

  eepromWriteBlock(EEPROM_I2C_ADDR, 16, &(eep.raw[16]), header_size - 16); 
   
  
  // Reads patch array content and stores it into the EEPROM
  for (int i = 0; i < (int)size_content; i += 8)
  {
    for (int k = 0; k < 8; k++)
      content[k] = pgm_read_byte_near(ssb_patch_content + i + k);
      
    eepromWriteBlock(EEPROM_I2C_ADDR, (i + header_size), content, 8);
  }
}

void eepromWriteBlock(uint8_t i2c_address, uint16_t offset, uint8_t const *pData, uint8_t blockSize)
{
  Wire.beginTransmission(i2c_address);
  Wire.write((int)offset >> 8);   // Most significant Byte
  Wire.write((int)offset & 0xFF); // Less significant Byte
  Wire.write(pData, blockSize);
  Wire.endTransmission();
  delay(5);
}

void checkPatch()
{

  int offset;
  uint8_t bufferAux[8];
  uint8_t content;
  int lin = 16; // See patch_init.h and patch_full.h
  bool error;
  int errorCount = 0;

  delay(500);

  // Shows the patch identification stored in the eeprom
  showMsg("Showing stored Patch information......");

  Wire.beginTransmission(EEPROM_I2C_ADDR);
  Wire.write(0x00);
  Wire.write(0x00);
  Wire.endTransmission();
  delay(5);
  Wire.requestFrom(EEPROM_I2C_ADDR, header_size); // Gets header information
  for (int k = 0; k < header_size; k++)
    eep.raw[k] = Wire.read();

  delay(5);

  // Showing information read from eeprom.

  showMsgText("Stored patch status. ..: %s", eep.refined.status);
  showMsgText("Stored patch name......: %s", eep.refined.patch_id);
  showMsgValue("Stored patch size......: %u bytes.", eep.refined.patch_size);

  showMsg("Showing the stored first 8 lines of content patch.");
  dumpEeprom(header_size, 8);

  showMsg("Showing the stored last 8 lines of content patch.");
  dumpEeprom(header_size + size_content - 64, 8);

  // Compare the EEPROM Content with patch content
  showMsg("Comparring EEPROM content with the original patch contents...");


  offset = header_size;

  for (int i = 0; i < (int) eep.refined.patch_size; i += 8)
  {
    // Reads patch content from EEPROM
    Wire.beginTransmission(EEPROM_I2C_ADDR);
    Wire.write((int)offset >> 8);   // header_size >> 8 wil be always 0 in this case
    Wire.write((int)offset & 0XFF); // offset Less significant Byte
    Wire.endTransmission();
    delay(1);

    error = false;

    Wire.requestFrom(EEPROM_I2C_ADDR, 8);
    for (int j = 0; j < 8; j++)
    {
      content = pgm_read_byte_near(ssb_patch_content + (i + j));
      bufferAux[j] = Wire.read();
      error = content != bufferAux[j];
    }

    if (error)
    {
      errorCount++;
      Serial.print("\nLine ");
      Serial.print(lin);
      Serial.print(" patch -> ");
      for (int j = 0; j < 8; j++)
      {
        content = pgm_read_byte_near(ssb_patch_content + (i + j));
        Serial.print(content, HEX);
        Serial.print(" ");
      }

      Serial.print(" eeprom -> ");
      for (int j = 0; j < 8; j++)
      {
        Serial.print(bufferAux[j], HEX);
        Serial.print(" ");
      }
    }

    if (errorCount > 15) break;

    offset += 8;
    lin++;
    delay(5);
  }

  if (errorCount)
  {
    showMsgValue("The patch was not successfully stored in the eeprom. Errors: %u", errorCount);
  } else {
    showMsg("The patch was stored in the eeprom with success.");
  }
}

void dumpEeprom(uint16_t offset, int sample)
{
  for (int i = 0; i < sample; i++)
  {

    Wire.beginTransmission(EEPROM_I2C_ADDR);
    Wire.write(offset >> 8); // offset Most significant Byte
    Wire.write(offset & 0xFF);  // offset Less significant Byte
    Wire.endTransmission();
    delay(5);

    Wire.requestFrom(EEPROM_I2C_ADDR, 8);
    for (int k = 0; k < sample; k++)
    {
      uint8_t c = Wire.read();
      Serial.print(c, HEX); // SHows patch id
      Serial.print(" ");
    }
    Serial.print("\n");
    offset += 8;
    delay(5);
  }
}

// Erases all eeprom content
void clearEeprom()
{
  uint8_t content[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  for (int i = 0; i < 16384; i += 8)
  {
    Wire.beginTransmission(EEPROM_I2C_ADDR);
    eepromWriteBlock(EEPROM_I2C_ADDR, i, content, 8);
  }
}

void loop()
{
}

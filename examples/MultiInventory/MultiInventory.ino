/**
 * Detect multiple simultaneous RFID cards using PN5180 reader
 * Copyright (c) 2024 Alastair Aitchison, Playful Technology
 */

// PN5180 defines
// See https://www.nxp.com/docs/en/data-sheet/PN5180A0XX-C1-C2.pdf 
// 11.4.3.3 Host Interface Command List
#define PN5180_WRITE_REGISTER           (0x00)
#define PN5180_WRITE_REGISTER_OR_MASK   (0x01)
#define PN5180_WRITE_REGISTER_AND_MASK  (0x02)
#define PN5180_READ_REGISTER            (0x04)
#define PN5180_WRITE_EEPROM             (0x06)
#define PN5180_READ_EEPROM              (0x07)
#define PN5180_SEND_DATA                (0x09)
#define PN5180_READ_DATA                (0x0A)
#define PN5180_LOAD_RF_CONFIG           (0x11)
#define PN5180_RF_ON                    (0x16)
#define PN5180_RF_OFF                   (0x17)
// 11.9.1, Table 73 PN5180 Register Address Overview
#define SYSTEM_CONFIG       (0x00)
#define IRQ_ENABLE          (0x01)
#define IRQ_STATUS          (0x02)
#define IRQ_CLEAR           (0x03)
#define RX_STATUS           (0x13)
#define TX_WAIT_CONFIG      (0x17)
#define TX_CONFIG           (0x18)
// 11.9.1, Table 76 IRQ_STATUS Register
#define RX_IRQ_STAT         	(1<<0)  // End of RF reception IRQ
#define TX_IRQ_STAT         	(1<<1)  // End of RF transmission IRQ
#define IDLE_IRQ_STAT       	(1<<2)  // Idle IRQ
#define RFOFF_DET_IRQ_STAT  	(1<<6)  // RF Field OFF detection IRQ
#define RFON_DET_IRQ_STAT   	(1<<7)  // RF Field ON detection IRQ
#define TX_RFOFF_IRQ_STAT   	(1<<8)  // RF Field OFF in PCD IRQ
#define TX_RFON_IRQ_STAT    	(1<<9)  // RF Field ON in PCD IRQ
// 11.9.1 Table 92 RX_STATUS Register
#define RX_COLL_POS (1<<19) // These bits show the bit position of the first detected collision in a received frame (7 bits)
#define RX_COLLISION_DETECTED (1<<18) // This flag is set to 1, when a collision has occurred 
// 11.9.1 Table 97 TX_CONFIG Register
#define TX_DATA_ENABLE (1<<10) // If set to 1, transmission of data is enabled otherwise only symbols are transmitted.

// INCLUDES
#include <SPI.h>
// OLED display. 
#include <U8g2lib.h>

// CONSTANTS
constexpr uint8_t PN5180_NSS = D8;
constexpr uint8_t PN5180_BUSY = D0;
constexpr uint8_t PN5180_RST = D4;
constexpr uint8_t maxTags = 16;
constexpr uint16_t commandTimeout = 500;
const SPISettings SPI_SETTINGS = SPISettings(7000000, MSBFIRST, SPI_MODE0);

// GLOBALS
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE, /* clock=*/ D1, /* data= */ D2);
uint8_t uids[8*maxTags];
uint8_t lastNumTagsInRange = 0;
uint8_t readBuffer[508]; // static stack buffer

bool IsNotBusy(){
  unsigned long startTime = millis();
  while (digitalRead(PN5180_BUSY) != LOW) {
    if (millis() - startTime > commandTimeout) {
      return false;
    };
  };
  return true;
}
bool IsBusy(){
  unsigned long startTime = millis();
  while (digitalRead(PN5180_BUSY) != HIGH) {
    if (millis() - startTime > commandTimeout) {
      return false;
    };
  };
  return true;
}
// Recommended SPI transceive with BUSY handling
// As described in 11.4.1 Physical Host Interface
// https://www.nxp.com/docs/en/data-sheet/PN5180A0XX-C1-C2.pdf
bool sendBytes(uint8_t *sendBuffer, size_t sendBufferLen){
  // 0. Wait until PN5180 is not busy
  if(!IsNotBusy()){ 
    Serial.print("Error in SPI send (0)");
    digitalWrite(PN5180_NSS, HIGH);
    return false;
  }
  // 1. Assert NSS LOW
  digitalWrite(PN5180_NSS, LOW); delay(5);
  // 2. Perform data exchange
  SPI.beginTransaction(SPI_SETTINGS);
  SPI.transfer((uint8_t*)sendBuffer, sendBufferLen);
  // 3. Wait for BUSY to go HIGH
  if(!IsBusy()){ 
    Serial.print("Error in SPI send (Step 3)");
    SPI.endTransaction();
    digitalWrite(PN5180_NSS, HIGH);
    return false;
  }
  // 4. Deassert NSS
  digitalWrite(PN5180_NSS, HIGH); delay(5);
  // 5. Wait until BUSY goes LOW
  if(!IsNotBusy()){ 
    Serial.print("Error in SPI send (Step 5)");
    SPI.endTransaction();
    digitalWrite(PN5180_NSS, HIGH);
    return false;
  }
  // Cleanup
  SPI.endTransaction();
  digitalWrite(PN5180_NSS, HIGH);
  return true;
}
bool readBytes(uint8_t *recvBuffer, size_t recvBufferLen){
  // Clear buffer
  memset(recvBuffer, 0x00, recvBufferLen);
  // 0. Wait until PN5180 is not busy
  if(!IsNotBusy()){ 
    Serial.print("Error in SPI receive (0)");
    digitalWrite(PN5180_NSS, HIGH);
    return false;
  }
  // 1. Assert NSS low
  digitalWrite(PN5180_NSS, LOW); delay(5);
  // 2. Perform data exchange
  SPI.beginTransaction(SPI_SETTINGS);
  SPI.transfer(recvBuffer, recvBufferLen);
  // 3. Wait until BUSY is HIGH
  if(!IsBusy()){ 
    Serial.print("Error in SPI receive (3)");
    SPI.endTransaction();
    digitalWrite(PN5180_NSS, HIGH);
    return false;
  }
  // 4. Deassert NSS
  digitalWrite(PN5180_NSS, HIGH); delay(5);
  // 5. Wait until BUSY is LOW
  if(!IsNotBusy()){ 
    Serial.print("Error in SPI receive (5)");
    SPI.endTransaction();
    return false;
  }
  SPI.endTransaction();
  return true;
}
bool SPItransceive(uint8_t *sendBuffer, size_t sendBufferLen, uint8_t *recvBuffer = 0, size_t recvBufferLen = 0){
  bool result = sendBytes(sendBuffer, sendBufferLen);
  if(result) {
    if((recvBuffer != 0) && (recvBufferLen != 0)) { result = readBytes(recvBuffer, recvBufferLen); }
  }
  return result;
}
// Loads the ISO15693 Tx/Rx protocol into the RF registers
bool loadISO15693config(){
  uint8_t cmd[] = {PN5180_LOAD_RF_CONFIG, 0x0D, 0x8D};
  SPItransceive(cmd, sizeof(cmd));
  return true;
}
// Reads 4-byte value from specified register
uint32_t readRegister(uint8_t regAddress){
  uint32_t regValue;
  uint8_t cmd[] = {PN5180_READ_REGISTER, regAddress};
  SPItransceive(cmd, sizeof(cmd), (uint8_t*)(&regValue), 4);
  return regValue;
}
// Switches the RF field ON
bool activateRF(){
  uint8_t cmd[] = {PN5180_RF_ON, 0x00};
  SPItransceive(cmd, sizeof(cmd));
  // Check to ensure the RF_ON bit has been activated
  unsigned long startedWaiting = millis();
  while (!(readRegister(IRQ_STATUS) & TX_RFON_IRQ_STAT)) { 
    if (millis() - startedWaiting > 500) {
      return false; 
    }
  };
  // And now we can clear the RF bit again
  uint32_t regValue = TX_RFON_IRQ_STAT;
  // Register value is 32 bit, which we need to shift and send as 4 separate 8-byte values.
  uint8_t cmdClear[] = {PN5180_WRITE_REGISTER, IRQ_CLEAR, (regValue>>24)&0xFF, (regValue>>16)&0xFF, (regValue>>8)&0xFF, (regValue)&0xFF};
  SPItransceive(cmdClear, sizeof(cmdClear));
  return true;
}
// Switch off RF field
bool disableRF(){
  uint8_t cmd[] = {PN5180_RF_OFF, 0x00};
  SPItransceive(cmd, sizeof(cmd));
  // Wait for RF field to shut down
  unsigned long startedWaiting = millis();
  while (!(readRegister(IRQ_STATUS) & TX_RFOFF_IRQ_STAT)) { 
    if (millis() - startedWaiting > 500) {
      return false;
    }
  };
  // And now clear the RF IRQ bit again
  uint32_t regValue = TX_RFOFF_IRQ_STAT;
  // Register value is 32 bit, which we need to send as 4 separate 8-byte values.
  uint8_t *regBytes =  (uint8_t*)(&regValue);
  uint8_t cmdClear[] = {PN5180_WRITE_REGISTER, IRQ_CLEAR, regBytes[0], regBytes[1], regBytes[2], regBytes[3]};
  SPItransceive(cmdClear, sizeof(cmdClear));
  return true;
}
// Checks the PN5180 is in IDLE state
bool checkIdle(){
  unsigned long startedWaiting = millis();
  while ((readRegister(IRQ_STATUS) & IDLE_IRQ_STAT) == 0 ) {
    if (millis() - startedWaiting > commandTimeout) {
      return false;
    }
  }
  return true;
}
// Clears the IRQ_STATUS register
void clearIRQ(){
  uint8_t cmd[] = {PN5180_WRITE_REGISTER, IRQ_CLEAR, 0xFF, 0xFF, 0x0F, 0x00};
  SPItransceive(cmd, sizeof(cmd));
}
// Sets the PN5180 into IDLE state
void setIdle(){
	uint8_t cmd[] = {PN5180_WRITE_REGISTER_AND_MASK, SYSTEM_CONFIG, 0xF8, 0xFF, 0xFF, 0xFF};
  SPItransceive(cmd, sizeof(cmd));
}
// Activates TRANSCEIVE routine
void activateTransceive(){   
	uint8_t cmd[] = {PN5180_WRITE_REGISTER_OR_MASK, SYSTEM_CONFIG, 0x03, 0x00, 0x00, 0x00};
  SPItransceive(cmd, sizeof(cmd));
}
// Sends ISO15693 inventory comand (0x06 0x01)
void sendInventory(){
  uint8_t cmd[] = {PN5180_SEND_DATA, 0x00, 0x06, 0x01, 0x00};
  SPItransceive(cmd, sizeof(cmd));
}
// Sends EOF symbol
void sendEndOfFrame(){
  // Set config to only transmit end of frame symbol without any data at next transmission
	uint8_t cmdcfg[] = {PN5180_WRITE_REGISTER_AND_MASK, TX_CONFIG, 0b00111111, 0b11111011, 0xFF, 0xFF};
  SPItransceive(cmdcfg, sizeof(cmdcfg));
  uint8_t cmd[] = {PN5180_SEND_DATA, 0x00};
  SPItransceive(cmd, sizeof(cmd));  // Send EOF
}
// Receives contents of the PN5180 reception buffer
uint8_t* readReceptionBuffer(int16_t len){
  if(len<0 || len>508) {
    Serial.println("Error: invalid length request for read buffer");
    return 0;
  }
  uint8_t cmdRead[] = { PN5180_READ_DATA, 0x00 };
  //uint8_t *readBuffer;
  //readBuffer = (uint8_t *) malloc(len);
  SPItransceive(cmdRead, sizeof(cmdRead), readBuffer, len);
  if(readBuffer == nullptr) {
    Serial.println("Invalid or unexpected response length.");
  }
  return readBuffer;
}
// As per 4.2.2. ISO/IEC 15693 - Inventory
// https://www.nxp.com/docs/en/application-note/AN12650.pdf
uint8_t getInventory(){
  uint8_t numTagsDetected = 0;
  // 1. Load the ISO15693 protocol into the RF register
  loadISO15693config();
  // 2. Switch the RF field ON
  activateRF();
  // 3. Clear the IRQ_STATUS register
  clearIRQ();
  // 4. Sets the PN5180 into IDLE state
	setIdle();
  // 5. Activates TRANSCEIVE routine       
	activateTransceive();
  // 6. Send an inventory command
  sendInventory();
  uint8_t foundTags = 0;
  // 7. Loop over all 16 time slots
  for(int slot=0; slot<16; slot++) {
    // Prevent watchdog from tripping
    yield();
    // 8. Has card responded? Read 4 bytes from the RX_STATUS register
    uint32_t rxStatus = readRegister(RX_STATUS);
    // Lowest 9 bits of RX_STATUS register are num bytes received (up to 508)
    uint16_t len = (uint16_t)(rxStatus & 0x000001ff);
    // Detect if a collision occurred
    if (rxStatus & RX_COLLISION_DETECTED) {
      // Serial.println("Collision detected");
    }
    // If no card has responded in this slot
    else if (len == 0) {
      // Serial.println("No card in this time slot.");
    }
    // 9. Read reception buffer
    else {
      uint8_t *readBuffer = readReceptionBuffer(len);
      if (readBuffer == nullptr) {
        // Serial.println("Invalid response");
        continue;
      }
      // First byte of valid response should be 0x00 (no flags)
      if(readBuffer[0] == 0x00) {
        if (numTagsDetected < maxTags && len >= 10) {
          memcpy(&uids[numTagsDetected * 8], &readBuffer[2], 8);
          numTagsDetected++;
        }
      }
    }
    // While there are other time slots to check
    if(slot < 15) {
      // 12. Sets the PN5180 into IDLE state
      setIdle();
      // 13. Activates TRANSCEIVE routine        
      activateTransceive();
      // 14. Clear the IRQ register
      clearIRQ();
      // 15. Send EOF
      sendEndOfFrame();
    }
  }
  // 16. Switch off RF field
  disableRF();
  return numTagsDetected;
}
void setup() {
  // Serial interface setup  
  Serial.begin(115200);
  // Clear the serial monitor from nonsense on boot-up
  for(int i=0; i<3; i++) {Serial.println("");}
  delay(1000);
  Serial.println(__FILE__ __DATE__);
  delay(100);
  SPI.begin();
  // OLED display  
  Serial.println("Setting up OLED");
  u8g2.setI2CAddress(0x3C<<1);
  u8g2.begin();
  u8g2.clearBuffer();					// clear the internal memory
  u8g2.setFont(u8g2_font_ncenB08_tr);	// choose a suitable font
  u8g2.drawStr(0,10,"Multitag RFID Demo");	// write something to the internal memory
  u8g2.sendBuffer();					// transfer internal memory to the display
  // PN5180 setup
  pinMode(PN5180_NSS, OUTPUT);
  digitalWrite(PN5180_NSS, HIGH);
  pinMode(PN5180_RST, OUTPUT);
  digitalWrite(PN5180_RST, HIGH);
  delay(100);
  // PN5180 reset
  digitalWrite(PN5180_RST, LOW);
  delay(10);
  digitalWrite(PN5180_RST, HIGH);
  delay(50);  
  Serial.println("Setup complete.");
}

void loop() {
  uint8_t numTagsInRange = getInventory();
  // Only update display if tags have changed
  if(numTagsInRange != lastNumTagsInRange) {
    u8g2.clearBuffer();					// clear the internal memory
    u8g2.setFont(u8g2_font_logisoso58_tn );	// choose a suitable font
    char buffer[3];
    snprintf(buffer, 3, "%u", numTagsInRange);
    u8g2.drawStr(40, 64, buffer);	// write something to the internal memory
    u8g2.sendBuffer();					// transfer internal memory to the display

    Serial.print(numTagsInRange);
    Serial.println(" tags detected:");
    for(int i=0; i<numTagsInRange; i++){
      for (int j=7; j>=0; j--) { // UIDs are LSB!
        uint8_t startAddr = (i*8) + j;
        if(uids[startAddr] < 0x10) Serial.print("0"); // zero-pad
        Serial.print(uids[startAddr], HEX);
      }
      Serial.println();
    }
    Serial.println("---");
  }
  lastNumTagsInRange = numTagsInRange;
  delay(5);
}
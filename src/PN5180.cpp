#include "PN5180.h"

PN5180::PN5180(uint8_t nssPin, uint8_t busyPin, uint8_t rstPin) 
  : _nss(nssPin), _busy(busyPin), _rst(rstPin), _spiSettings(125000, MSBFIRST, SPI_MODE0) {}

/***************************
// INITIALISATION AND CONFIGURATION
***************************/

// Startup procedure
void PN5180::begin() {
  pinMode(_nss, OUTPUT);
  digitalWrite(_nss, HIGH);
  pinMode(_rst, OUTPUT);
  digitalWrite(_rst, HIGH);
  pinMode(_busy, INPUT);
  SPI.begin();
  hardReset();
}

// Hard reset
void PN5180::hardReset() {
  digitalWrite(_rst, LOW);
  delay(10);
  digitalWrite(_rst, HIGH);
  delay(50);
}

// Load the ISO15693 Tx/Rx protocol into the RF registers
void PN5180::loadISO15693config(){
  uint8_t cmd[] = {PN5180_LOAD_RF_CONFIG, 0x0D, 0x8D};
  sendBytes(cmd, sizeof(cmd));
}

// Set the PN5180 into IDLE state
void PN5180::setIdle(){
	uint8_t cmd[] = {PN5180_WRITE_REGISTER_AND_MASK, SYSTEM_CONFIG, 0xF8, 0xFF, 0xFF, 0xFF};
  sendBytes(cmd, sizeof(cmd));
}

// Activate TRANSCEIVE routine
void PN5180::activateTransceive(){   
	uint8_t cmd[] = {PN5180_WRITE_REGISTER_OR_MASK, SYSTEM_CONFIG, 0x03, 0x00, 0x00, 0x00};
  sendBytes(cmd, sizeof(cmd));
}

/***************************
// SPI COMMUNICATION
***************************/

// Recommended SPI transceive routine with BUSY handling
// As described in 11.4.1 Physical Host Interface
// https://www.nxp.com/docs/en/data-sheet/PN5180A0XX-C1-C2.pdf
bool PN5180::sendBytes(uint8_t *sendBuffer, size_t sendBufferLen){
  // 0. Wait until PN5180 is not busy
  if(!waitUntilAvailable()){ 
    Serial.print("SPI send error 0");
    digitalWrite(_nss, HIGH);
    return false;
  }
  // 1. Assert NSS LOW
  digitalWrite(_nss, LOW); delay(5);
  // 2. Perform data exchange
  SPI.beginTransaction(_spiSettings);
  SPI.transfer((uint8_t*)sendBuffer, sendBufferLen);
  // 3. Wait for BUSY to go HIGH
  if(!waitUntilBusy()){ 
    Serial.print("SPI send error 3");
    SPI.endTransaction();
    digitalWrite(_nss, HIGH);
    return false;
  }
  // 4. Deassert NSS
  digitalWrite(_nss, HIGH); delay(5);
  // 5. Wait until BUSY goes LOW
  if(!waitUntilAvailable()){ 
    Serial.print("SPI send error 5");
    SPI.endTransaction();
    digitalWrite(_nss, HIGH);
    return false;
  }
  // Cleanup
  SPI.endTransaction();
  digitalWrite(_nss, HIGH);
  return true;
}

bool PN5180::readBytes(uint8_t *recvBuffer, size_t recvBufferLen){
  // Clear buffer
  memset(recvBuffer, 0x00, recvBufferLen);
  // 0. Wait until PN5180 is not busy
  if(!waitUntilAvailable()){ 
    Serial.print("SPI receive error 0");
    digitalWrite(_nss, HIGH);
    return false;
  }
  // 1. Assert NSS low
  digitalWrite(_nss, LOW); delay(5);
  // 2. Perform data exchange
  SPI.beginTransaction(_spiSettings);
  SPI.transfer(recvBuffer, recvBufferLen);
  // 3. Wait until BUSY is HIGH
  if(!waitUntilBusy()){ 
    Serial.print("SPI receive error 3");
    SPI.endTransaction();
    digitalWrite(_nss, HIGH);
    return false;
  }
  // 4. Deassert NSS
  digitalWrite(_nss, HIGH); delay(5);
  // 5. Wait until BUSY is LOW
  if(!waitUntilAvailable()){ 
    Serial.print("SPI receive error 5");
    SPI.endTransaction();
    return false;
  }
  SPI.endTransaction();
  return true;
}

/***************************
// REGISTER MANIPULATION
***************************/

// Read 4-byte value from specified register
uint32_t PN5180::readRegister(uint8_t regAddress){
  uint8_t cmd[] = {PN5180_READ_REGISTER, regAddress};
  sendBytes(cmd, sizeof(cmd));
  uint32_t regValue;
  readBytes((uint8_t*)(&regValue), 4);
  return regValue;
}

// Clear the IRQ_STATUS register
void PN5180::clearIRQ(){
  uint8_t cmd[] = {PN5180_WRITE_REGISTER, IRQ_CLEAR, 0xFF, 0xFF, 0x0F, 0x00};
  sendBytes(cmd, sizeof(cmd));
}

/***************************
// RF FIELD MANAGEMENT
***************************/
// Switch the RF field ON
bool PN5180::activateRF(){
  uint8_t cmd[] = {PN5180_RF_ON, 0x00};
  sendBytes(cmd, sizeof(cmd));
  // Check to ensure the RF_ON bit has been activated
  unsigned long startedWaiting = millis();
  while (!(readRegister(IRQ_STATUS) & TX_RFON_IRQ_STAT)) { 
    if (millis() - startedWaiting > 500) {
      return false; 
    }
  };
  // And now we can clear the RF bit again
  uint32_t regValue = TX_RFON_IRQ_STAT;
  // Use bitshifting/masking to separate 32bit value into 4 separate byte values.
  uint8_t cmdClear[] = {PN5180_WRITE_REGISTER, IRQ_CLEAR, (regValue>>24)&0xFF, (regValue>>16)&0xFF, (regValue>>8)&0xFF, (regValue)&0xFF};
  sendBytes(cmdClear, sizeof(cmdClear));
  return true;
}
// Switch off RF field
bool PN5180::disableRF(){
  uint8_t cmd[] = {PN5180_RF_OFF, 0x00};
  sendBytes(cmd, sizeof(cmd));
  // Wait for RF field to shut down
  unsigned long startedWaiting = millis();
  while (!(readRegister(IRQ_STATUS) & TX_RFOFF_IRQ_STAT)) { 
    if (millis() - startedWaiting > 500) {
      return false;
    }
  };
  // And now clear the RF IRQ bit again
  uint32_t regValue = TX_RFOFF_IRQ_STAT;
  // Register value is 32bit, which we need to send as 4 separate byte values.
  uint8_t *regBytes =  (uint8_t*)(&regValue);
  uint8_t cmdClear[] = {PN5180_WRITE_REGISTER, IRQ_CLEAR, regBytes[0], regBytes[1], regBytes[2], regBytes[3]};
  sendBytes(cmdClear, sizeof(cmdClear));
  return true;
}

/**************************
// ISO15693 Command Handling
***************************/

// Send ISO15693 inventory comand (0x06 0x01)
void PN5180::sendInventoryCmd(){
  uint8_t cmd[] = {PN5180_SEND_DATA, 0x00, 0x06, 0x01, 0x00};
  sendBytes(cmd, sizeof(cmd));
}

// As per 4.2.2. ISO/IEC 15693 - Inventory
// https://www.nxp.com/docs/en/application-note/AN12650.pdf
bool PN5180::getInventory(uint8_t *uid) {
  bool tagDetected = false;
  // 1. Load the ISO15693 protocol into the RF register
  loadISO15693config();
  // 2. Switch the RF field ON
  if (!activateRF()) return false;
  // 3. Clear the IRQ_STATUS register
  clearIRQ();
  // 4. Set the PN5180 into IDLE state
  setIdle();
  // 5. Activate TRANSCEIVE routine
  activateTransceive();
  // 6. Send an inventory command
  sendInventoryCmd();
  // 7. Loop over all 16 time slots until a tag is detected
  for(int slot=0; slot<16 && !tagDetected; slot++) {
    // Prevent system watchdog from timing out
    yield();
	// 8. Has a card responded? Read 4 bytes from the RX_STATUS register  
    uint32_t rxStatus = readRegister(RX_STATUS);
    // Lowest 9 bits of RX_STATUS register are num bytes received (up to 508)
    uint16_t len = (uint16_t)(rxStatus & 0x000001ff);
	// Ensure we don't read mor data than we can hold in the buffer
	if(len > READ_BUFFER_SIZE){ len = READ_BUFFER_SIZE; }
	// Check that some data has been retrieved
    if (len > 0) {
      // 9. Read reception buffer
      readReceptionBuffer(buffer, len);
      if(buffer == nullptr) {
        Serial.println("Invalid response");
      }
      // First byte of valid response should be 0x00 (no flags)
      if(buffer[0] == 0x00) {
        // Ignore the first two bytes in the response, then copy 8-byte UID
        memcpy(uid, &buffer[2], 8);
        // Once we have found a tag, no need to check additional timeslots
        tagDetected = true;
        break;
      }
      // Error flag has been set
      else if(buffer[0] & (1<<0)){
        // Retrieve specific error from second byte
        uint8_t errorCode = (buffer)[1];
        errorHandler((ISO15693ErrorCode)errorCode);
      }
    }
    // While there are other time slots to check
    if(slot < 15) {
    // 12. Set the PN5180 into IDLE state  
    setIdle();
    // 13. Activate TRANSCEIVE routine   
    activateTransceive();
    // 14. Clear the IRQ register
    clearIRQ();
    // 15. Send EOF
    sendEndOfFrame();
    }
  }
  // 16. Switch off RF field
  disableRF();
  return tagDetected;
}

// Send EOF symbol
void PN5180::sendEndOfFrame(){
  // Set config to only transmit end of frame symbol without any data at next transmission
  uint8_t cmdcfg[] = {PN5180_WRITE_REGISTER_AND_MASK, TX_CONFIG, 0b00111111, 0b11111011, 0xFF, 0xFF};
  sendBytes(cmdcfg, sizeof(cmdcfg));
  uint8_t cmd[] = {PN5180_SEND_DATA, 0x00};
  sendBytes(cmd, sizeof(cmd));
}

// Receive contents of the PN5180 reception buffer
bool PN5180::readReceptionBuffer(uint8_t* buffer, int16_t len){
  if(len<0 || len>508) {
    Serial.print("Invalid read length request (");
    Serial.print(len);
	Serial.println("bytes requested)");
    return false;
  }
  uint8_t cmdRead[] = { PN5180_READ_DATA, 0x00 };
  sendBytes(cmdRead, sizeof(cmdRead));
  readBytes(buffer, len);

  if(buffer == nullptr) {
    hardReset();
    Serial.print("Invalid response. Attempted to read ");
    Serial.print(len);
    Serial.println("bytes.");
    return false;
  }
  return true;
}

/*******************
// UTILITY FUNCTIONS
********************/

// Check if PN5180 is in IDLE state
bool PN5180::checkIdle(){
  unsigned long startedWaiting = millis();
  while ((readRegister(IRQ_STATUS) & IDLE_IRQ_STAT) == 0 ) {
    if (millis() - startedWaiting > _commandTimeout) {
      return false;
    }
  }
  return true;
}

// Wait until PN5180 is available to receive command
bool PN5180::waitUntilAvailable(){
  unsigned long startTime = millis();
  while (digitalRead(_busy) != LOW) {
    if (millis() - startTime > _commandTimeout) {
      return false;
    };
  };
  return true;
}

// Wait until PN5180 is processing
bool PN5180::waitUntilBusy(){
  unsigned long startTime = millis();
  while (digitalRead(_busy) != HIGH) {
    if (millis() - startTime > _commandTimeout) {
      return false;
    };
  };
  return true;
}

// Handle any errors received
void PN5180::errorHandler(ISO15693ErrorCode errorCode){
  switch (errorCode) {
    case EC_NO_CARD: Serial.println("No card present"); break;
    case ISO15693_EC_OK: Serial.println("OK!"); break;
    case ISO15693_EC_NOT_SUPPORTED: Serial.println("Command not supported"); break;
    case ISO15693_EC_NOT_RECOGNIZED: Serial.println("Command not recognized"); break;
    case ISO15693_EC_OPTION_NOT_SUPPORTED: Serial.println("Option not supported"); break;
    case ISO15693_EC_UNKNOWN_ERROR: Serial.println("Unknown error"); break;
    case ISO15693_EC_BLOCK_NOT_AVAILABLE: Serial.println("Block not available"); break;
    case ISO15693_EC_BLOCK_ALREADY_LOCKED: Serial.println("Block is already locked"); break;
    case ISO15693_EC_BLOCK_IS_LOCKED: Serial.println("Block is locked"); break;
    case ISO15693_EC_BLOCK_NOT_PROGRAMMED: Serial.println("Block not successfully programmed"); break;
    case ISO15693_EC_BLOCK_NOT_LOCKED: Serial.println("Block not successfully locked"); break;
    default:
      if ((errorCode >= 0xA0) && (errorCode <= 0xDF)) {
        Serial.println("Custom error code");
      }
      else Serial.println("Undefined error code");
  }
}
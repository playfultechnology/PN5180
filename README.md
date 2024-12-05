# PN5180
![Arduino UNO](https://img.shields.io/badge/Arduino&nbsp;UNO-Supported-brgreen)
![Arduino Nano](https://img.shields.io/badge/Arduino&nbsp;Nano-Supported-brgreen)
![ESP](https://img.shields.io/badge/ESP8266-Supported-brgreen)
![ESP](https://img.shields.io/badge/ESP32-Supported-brgreen)

Arduino library for reading ISO15693 RFID tags using the PN5180 NFC Module from NXP Semiconductors

## Introduction
This library was created to read the UID of tags presented to a PN5180 RFID reader.

The PN5180 implements the [ISO15693 "vicinity" protocol](https://en.wikipedia.org/wiki/ISO/IEC_15693), which typically enables tags to be read at a distance of ~20-30cm. This is a significant improvement on the common MFRC522 boards that use the [ISO14443 "proximity" protocol](https://en.wikipedia.org/wiki/ISO/IEC_14443) instead, which has a typical read distance of only a few centimetres.  

While the functionality of this library is fully compliant with the published ISO15693 protocol, it is **not** intended to implement the complete ISO15693 specification, nor all functionality of the PN5180 chip. Instead, it is optimised for the specific (most common) use case of performing an inventory request to read the 10-byte UID from any cards in range.

If you require additional functionality (writing blocks, accessing security-protected cards, etc.), you may prefer to check out [this library](https://github.com/ATrappmann/PN5180-Library) instead. 

## Usage
The library exposes a single method `getInventory()`. The return value is the number of tags that are detected in the range of the reader, while the 8-byte ID of the first detected tag is assigned to an 8 byte integer array passed as a parameter to the method.

### Example

```
uint8_t uid[8];
uint8_t numberOfCardsDetected = pn5180.getInventory(uid));

Serial.print(numberOfCardsDetected);
Serial.println(" cards detected.");

Serial.print("UID: ");
for(int i=7; i>=0; i--) {
  if(uid[i] < 0x10) Serial.print("0");
  Serial.print(uid[i], HEX);
}
```
___
Output:
```
1 cards detected
UID: E00401502A49F6D0
```
Note that the UID is stored in LSB order, so the bytes are printed in reverse-order.

## Hardware
The PN5180 uses an SPI interface additional BUSY and RESET lines, and operates at 3.3V logic. It also requires a separate 5V supply, which is solely used to power the RF antenna.

### Suggested Wiring
The following table gives recommended connections to the host controller. 
Arduino and ESP8266 boards _require_ the use of the specific pins named for MOSI, MISO, and SCK lines. The ESP32 allows the SPI interface to be mapped to other pins if desired (although this comes with a slight performance cost, it is unlikely to cause an issue).
All other connections may be assigned to any available GPIO pin as desired.

| PN5180 | Arduino UNO / Nano | ESP8266 | ESP32 |
| ------ | ------------------ | ------- |------ |
| ![5V](https://img.shields.io/badge/5V-red) | 5V | 5V | 5V | 
| ![3.3V](https://img.shields.io/badge/3.3V-orange) | 3.3V | 3.3V | 3.3V |
| ![RST](https://img.shields.io/badge/RST-brown)  **\*** | 8 <sub>(via 5V-3.3V LLC)</sub> | D4 | 16 |
| ![NSS](https://img.shields.io/badge/NSS-white)  **\*** | 10 <sub>(via 5V-3.3V LLC)</sub> | D8 | 5 | 
| ![MOSI](https://img.shields.io/badge/MOSI-brgreen)  **\*** | 11  <sub>(via 5V-3.3V LLC)</sub> | D7 | 23 |
| ![MISO](https://img.shields.io/badge/MISO-blue)  | 12 | D6 | 19 |
| ![SCK](https://img.shields.io/badge/SCK-yellow)  **\*** | 13 <sub>(via 5V-3.3V LLC)</sub> | D5 | 18 |
| ![BUSY](https://img.shields.io/badge/BUSY-purple)  | 9 | D0 | 17 |
| ![GND](https://img.shields.io/badge/GND-black)  | GND | GND | GND |

### Notes
- If using a 5V controller (e.g. Arduino UNO, Nano, or Mega), those pins marked * (RST, NSS, MOSI, SCK) *must* be connected via a logic level shifter between the controller and the PN5180. This ensures that any outputs that are set to 5V HIGH by the Arduino are reduced to 3.3V inputs for the controller. 
- Other signal lines (MISO, BUSY), which are set to HIGH by the PN5180 *may* be shifted up to 5V before the Arduino, but this is not generally necessary since the 3.3V output by the PN5180 will still generally be read as a HIGH input on a 5V device.  
- The GPIO, IRQ, AUX and REQ pins exposed on the board are not required and not used by this library.
- It is recommended to use a dedicated logic level shifter such as TXB0108 or a logic gate such as a 74LVC1G125. This library has also been tested with simpler transistor-based "I2C level shifter boards", but these have much slower switching speeds which can affect data integrity.

### Suggested Wiring
![Wiring](https://github.com/playfultechnology/PN5180/blob/main/extras/PN5180%20Wiring_bb.jpg)

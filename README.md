# PN5180
![C++](https://img.shields.io/badge/C++-11-green)
![Arduino UNO](https://img.shields.io/badge/Arduino-UNO-green)
![Arduino Nano](https://img.shields.io/badge/Arduino-Nano-green)
![ESP](https://img.shields.io/badge/ESP-8266-green)
![ESP](https://img.shields.io/badge/ESP-32-green)

Arduino library for reading ISO15693 RFID tags using the PN5180 NFC Module from NXP Semiconductors

## Usage
This library was created to read the UID of tags presented to a PN5180 RFID reader, principally for use in an escape room object identificiation puzzle.

While the funcionality is fully compliant with the published ISO15693 protocol, this library is **not** intended to implement the complete ISO15693 specification, nor all functionality of the PN5180. 
Instead, it is optimised for the specific (most common) use case of performing an inventory request to read the 10-byte UID from any cards in range.  

## Hardware
The PN5180 uses an SPI interface additional BUSY and RESET lines, and operates at 3.3V logic. It also requires a separate 5V supply, which is solely used to power the RF antenna.
It therefore requires the following connections to the host controller:
- 5V
- 3.3V
- RST *
- NSS *
- MOSI *
- MISO
- SCK *
- BUSY
- GND

### Notes
- If using a 5V controller (e.g. Arduino UNO, Nano, or Mega), those pins marked * (RST, NSS, MOSI, SCK) *must* be connected via a logic level shifter between the controller and the PN5180. This ensures that any outputs that are set to 5V HIGH by the Arduino are reduced to 3.3V inputs for the controller. 
- Other signal lines (MISO, BUSY), which are set to HIGH by the PN5180 *may* be shifted up to before the Arduino, but this is not generally necessary, since the 3.3V signal will still generally be read as a HIGH input on a 5V device.  
- The GPIO, IRQ, AUX and REQ pins exposed on the board are not required and not used by this library.
- It is recommended to use a dedicated logic level shifter such as TXB0108 or a logic gate such as a 74LVC1G125. This library has also been tested with simpler transistor-based "I2C level shifter boards", but these have much slower switching speeds which can affect data integrity.

### Suggested Wiring
![Wiring](https://github.com/playfultechnology/PN5180/blob/main/extras/PN5180%20Wiring_bb.jpg)

/**
 * ISO15693 getInventory() example
 *
 * Part of the PN5180 library (https://github.com/playfultechnology/PN5180)
 * Copyright (c) 2024 Alastair Aitchison, Playful Technology
 */

// Include the PN5180 library
#include "PN5180.h"

// Create a PN5180 instance specifying appropriate pins here
// PN5180 pn5180(/* NSS=*/ D8, /* BUSY=*/ D0, /* RST=*/ D4); // ESP8266
// PN5180 pn5180(/* NSS=*/ 10, /* BUSY=*/ 9, /* RST=*/ 8); // Arduino
PN5180 pn5180(/* NSS=*/ 5, /* BUSY=*/ 17, /* RST=*/ 16); // ESP32

void setup() {
  // Start serial connection to print UIDs of any tags read
  Serial.begin(115200);
  delay(1000);
  Serial.println("PN5180 getInventory() example");  
  
  // Initialise PN5180 interface
  pn5180.begin();
}

void loop() {
  // Create an 8-byte integer array to store UID of any detected tag
  uint8_t uid[8];
  
  // Call the getInventory() method
  if (pn5180.getInventory(uid)) {
	  
	// If tag was found, print its ID to the serial monitor
    Serial.print("Tag UID: ");
	// Format each byte as HEX, padded with leading zeroes if required
    for (int i = 7; i >= 0; i--) {
      if (uid[i] < 0x10) Serial.print("0");
      Serial.print(uid[i], HEX);
    }
    Serial.println();
  } 
  // No tag in range
  else {
    Serial.println("No tag detected.");
  }
  delay(500);
}
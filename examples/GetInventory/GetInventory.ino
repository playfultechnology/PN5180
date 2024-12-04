#include "src/PN5180.h"

// Define your pin connections
#define PN5180_NSS  10
#define PN5180_BUSY 9
#define PN5180_RST  8

PN5180 pn5180(PN5180_NSS, PN5180_BUSY, PN5180_RST);

void setup() {
  Serial.begin(115200);
  pn5180.begin();
  Serial.println("PN5180 Initialized.");
}

void loop() {
  uint8_t uid[8];
  if (pn5180.getInventory(uid)) {
    Serial.print("Tag UID: ");
    for (int i = 7; i >= 0; i--) {
      if (uid[i] < 0x10) Serial.print("0");
      Serial.print(uid[i], HEX);
    }
    Serial.println();
  } else {
    Serial.println("No tag detected.");
  }
  delay(1000);
}

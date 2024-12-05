#include "PN5180.h"

// Define your pin connections here
// Suggested examples are as follows, though any available GPIO pins may be used:
#ifdef ESP32
  // MOSI 23, MISO 19, CLK 18  (default VSPI interface, can be reassigned)
  #define PN5180_NSS  5
  #define PN5180_BUSY 17
  #define PN5180_RST  16
#elif defined(ESP8266)
  // MOSI D7, MISO D6, CLK D5
  #define PN5180_NSS  D8
  #define PN5180_BUSY D0
  #define PN5180_RST  D4
// For Arduino UNO, Nano, etc.
#else
  // MOSI 11, MISO 12, CLK 13
  #define PN5180_NSS 10
  #define PN5180_BUSY 9
  #define PN5180_RST  8
#endif

constexpr byte beepPin = 2;

PN5180 pn5180(PN5180_NSS, PN5180_BUSY, PN5180_RST);

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println(__FILE__ __DATE__);
  pn5180.begin();
  Serial.println("PN5180 Initialized.");

  pinMode(beepPin, OUTPUT);

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

    digitalWrite(beepPin, HIGH);
    delay(50);
    digitalWrite(beepPin, LOW);

  } else {
    Serial.println("No tag detected.");
  }
  delay(1000);
}

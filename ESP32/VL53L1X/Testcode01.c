#include <Wire.h>

void setup() {
  Serial.begin(115200);
  delay(1000);

  Wire.begin(21, 22);
  Wire.setClock(100000);

  Serial.println("I2C SCAN START");
}

void loop() {

  int count = 0;

  for (int address = 1; address < 127; address++) {

    Wire.beginTransmission(address);
    byte error = Wire.endTransmission();

    if (error == 0) {
      Serial.print("FOUND: 0x");

      if (address < 16)
        Serial.print("0");

      Serial.println(address, HEX);
      count++;
    }
  }

  if (count == 0)
    Serial.println("NO DEVICE");

  Serial.println("-----");

  delay(2000);
}

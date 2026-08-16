#include <Wire.h>

void setup() {
  Serial.begin(115200);
  delay(1000);

  Wire.begin(21, 22);   // SDA=21, SCL=22
  Wire.setClock(100000);

  Serial.println("MLX90640 I2C TEST START");
}

void loop() {
  Wire.beginTransmission(0x33);
  byte error = Wire.endTransmission();

  if (error == 0) {
    Serial.println("MLX90640 FOUND at 0x33");
  } else {
    Serial.print("MLX90640 NOT FOUND / ERROR CODE: ");
    Serial.println(error);
  }

  delay(2000);
}

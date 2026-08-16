#include <Wire.h>
#include <VL53L1X.h>
#include <ESP32Servo.h>

VL53L1X sensor;
Servo servo;

int angle = 0;

void beep(int t) {
  digitalWrite(25, HIGH);
  delay(100);
  digitalWrite(25, LOW);
  delay(t - 100);
}

void setup() {
  Serial.begin(115200);

  pinMode(25, OUTPUT);
  servo.attach(18);
  servo.write(0);

  Wire.begin(21, 22);

  sensor.setTimeout(500);

  if (!sensor.init()) {
    Serial.println("VL53L1X ERROR");
    while (1);
  }

  sensor.setDistanceMode(VL53L1X::Short);
  sensor.startContinuous(50);
}

void loop() {
  int distance = sensor.read();

  Serial.print(distance);
  Serial.println(" mm");

  if (distance < 100) {
    angle = 120 - angle;
    servo.write(angle);
    beep(500);
  }
  else {
    beep(2000);
  }
}

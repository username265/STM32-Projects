#include <Wire.h>
#include <Adafruit_MLX90640.h>

Adafruit_MLX90640 mlx;
float frame[32 * 24];

void setup() {
  // Serial 시작
  Serial.begin(115200);
  delay(1000);

  Serial.println("MLX90640 TEST START");

  // ESP32 I2C 설정
  // SDA = GPIO21
  // SCL = GPIO22
  Wire.begin(21, 22);
  Wire.setClock(400000);

  Serial.println("I2C STARTED");

  // MLX90640 시작
  if (!mlx.begin(MLX90640_I2CADDR_DEFAULT, &Wire)) {
    Serial.println("MLX90640 NOT FOUND!");
    
    while (1) {
      delay(1000);
    }
  }

  Serial.println("MLX90640 FOUND!");

  // 센서 설정
  mlx.setMode(MLX90640_CHESS);
  mlx.setResolution(MLX90640_ADC_18BIT);
  mlx.setRefreshRate(MLX90640_2_HZ);

  Serial.println("MLX90640 READY!");
}

void loop() {

  // 온도 프레임 읽기
  if (mlx.getFrame(frame) != 0) {
    Serial.println("FRAME READ FAILED");
    delay(500);
    return;
  }

  // 768개 픽셀 중 최고 온도 찾기
  float maxTemp = frame[0];

  for (int i = 1; i < 768; i++) {
    if (frame[i] > maxTemp) {
      maxTemp = frame[i];
    }
  }

  // 주변 온도
  Serial.print("Ambient: ");
  Serial.print(mlx.getTa(false), 1);
  Serial.print(" C");

  // 최고 온도
  Serial.print("    Max: ");
  Serial.print(maxTemp, 1);
  Serial.println(" C");

  delay(500);
}

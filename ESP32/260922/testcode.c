#include <Wire.h>
#include <VL53L1X.h>

// =====================================================
// 사용자 설정
// =====================================================
#define BUZZER_IS_ACTIVE 1
// 1: 능동형 부저 - HIGH/LOW로 동작
// 0: 수동형 부저 - 2000Hz tone으로 동작

constexpr uint8_t LED_PIN     = 4;
constexpr uint8_t BUZZER_PIN  = 5;
constexpr uint8_t I2C_SDA_PIN = 8;
constexpr uint8_t I2C_SCL_PIN = 9;

constexpr uint16_t NEAR_LIMIT_MM = 100;  // 10cm
constexpr uint16_t FAR_LIMIT_MM  = 300;  // 30cm

// LED 토글 간격
constexpr uint32_t FAST_LED_MS = 120;
constexpr uint32_t SLOW_LED_MS = 500;

// 시리얼 모니터 출력 간격
constexpr uint32_t SERIAL_INTERVAL_MS = 300;

VL53L1X sensor;

enum DistanceZone : uint8_t {
  ZONE_UNKNOWN,
  ZONE_NEAR,     // 10cm 미만
  ZONE_MIDDLE,   // 10cm 이상, 30cm 미만
  ZONE_FAR       // 30cm 이상
};

DistanceZone currentZone = ZONE_UNKNOWN;

// LED 상태
bool ledState = true;
uint32_t lastLedChangeMs = 0;

// 부저 패턴 상태
bool buzzerPatternRunning = false;
bool buzzerPhaseOn = false;
uint8_t buzzerTargetCount = 0;
uint8_t buzzerCompletedCount = 0;
uint16_t buzzerOnTimeMs = 0;
uint16_t buzzerOffTimeMs = 0;
uint32_t buzzerPhaseStartMs = 0;

// 센서 및 시리얼 상태
uint32_t lastSerialPrintMs = 0;
uint32_t lastSensorDataMs = 0;
bool sensorNoDataReported = false;


// =====================================================
// 부저 제어
// =====================================================
void buzzerOutput(bool on) {
#if BUZZER_IS_ACTIVE
  digitalWrite(BUZZER_PIN, on ? HIGH : LOW);
#else
  if (on) {
    tone(BUZZER_PIN, 2000);
  } else {
    noTone(BUZZER_PIN);
  }
#endif
}

void stopBuzzerPattern() {
  buzzerOutput(false);
  buzzerPatternRunning = false;
  buzzerPhaseOn = false;
  buzzerCompletedCount = 0;
}

void startBuzzerPattern(uint8_t count,
                        uint16_t onTime,
                        uint16_t offTime,
                        uint32_t now) {
  stopBuzzerPattern();

  buzzerTargetCount = count;
  buzzerCompletedCount = 0;
  buzzerOnTimeMs = onTime;
  buzzerOffTimeMs = offTime;

  buzzerPatternRunning = true;
  buzzerPhaseOn = true;
  buzzerPhaseStartMs = now;

  buzzerOutput(true);
}

void updateBuzzer(uint32_t now) {
  if (!buzzerPatternRunning) {
    return;
  }

  if (buzzerPhaseOn) {
    if ((uint32_t)(now - buzzerPhaseStartMs) >= buzzerOnTimeMs) {
      buzzerOutput(false);
      buzzerPhaseOn = false;
      buzzerPhaseStartMs = now;

      buzzerCompletedCount++;

      if (buzzerCompletedCount >= buzzerTargetCount) {
        buzzerPatternRunning = false;
      }
    }
  } else {
    if ((uint32_t)(now - buzzerPhaseStartMs) >= buzzerOffTimeMs) {
      buzzerOutput(true);
      buzzerPhaseOn = true;
      buzzerPhaseStartMs = now;
    }
  }
}


// =====================================================
// 거리 구간 처리
// =====================================================
DistanceZone classifyDistance(uint16_t distanceMm) {
  if (distanceMm < NEAR_LIMIT_MM) {
    return ZONE_NEAR;
  }

  if (distanceMm < FAR_LIMIT_MM) {
    return ZONE_MIDDLE;
  }

  return ZONE_FAR;
}

const char* zoneName(DistanceZone zone) {
  switch (zone) {
    case ZONE_NEAR:
      return "NEAR (< 10 cm)";

    case ZONE_MIDDLE:
      return "MIDDLE (10 cm ~ under 30 cm)";

    case ZONE_FAR:
      return "FAR (>= 30 cm)";

    default:
      return "UNKNOWN";
  }
}

void changeZone(DistanceZone newZone, uint32_t now) {
  if (newZone == currentZone) {
    return;
  }

  Serial.print("[ZONE] ");
  Serial.print(zoneName(currentZone));
  Serial.print(" -> ");
  Serial.println(zoneName(newZone));

  currentZone = newZone;

  // 새 구간 진입 시 LED를 켜진 상태부터 시작
  ledState = true;
  digitalWrite(LED_PIN, HIGH);
  lastLedChangeMs = now;

  // 이전 부저 패턴 중지
  stopBuzzerPattern();

  switch (currentZone) {
    case ZONE_NEAR:
      // 빠르게 5회: 100ms ON / 100ms OFF
      startBuzzerPattern(5, 100, 100, now);
      break;

    case ZONE_MIDDLE:
      // 3회: 200ms ON / 250ms OFF
      startBuzzerPattern(3, 200, 250, now);
      break;

    case ZONE_FAR:
    case ZONE_UNKNOWN:
      // 부저 정지
      break;
  }
}


// =====================================================
// LED 제어
// =====================================================
void updateLed(uint32_t now) {
  uint32_t intervalMs;

  if (currentZone == ZONE_NEAR) {
    intervalMs = FAST_LED_MS;
  } else if (currentZone == ZONE_MIDDLE) {
    intervalMs = SLOW_LED_MS;
  } else {
    // 30cm 이상 또는 첫 측정 전에는 LED 계속 켜짐
    if (!ledState) {
      ledState = true;
      digitalWrite(LED_PIN, HIGH);
    }
    return;
  }

  if ((uint32_t)(now - lastLedChangeMs) >= intervalMs) {
    lastLedChangeMs = now;
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState ? HIGH : LOW);
  }
}


// =====================================================
// 오류 처리
// =====================================================
void fatalSensorError(const char* message) {
  Serial.print("[FATAL] ");
  Serial.println(message);
  Serial.println("배선과 전원을 확인하세요.");

  stopBuzzerPattern();

  // 센서 초기화 실패 표시: LED 빠른 점멸
  while (true) {
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState ? HIGH : LOW);
    delay(250);
  }
}


// =====================================================
// 초기 설정
// =====================================================
void setup() {
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(LED_PIN, HIGH);
  digitalWrite(BUZZER_PIN, LOW);
  ledState = true;

  // 전원이 들어오면 LED ON + 짧은 부저음
  buzzerOutput(true);
  delay(150);
  buzzerOutput(false);

  Serial.begin(115200);
  delay(300);

  Serial.println();
  Serial.println("====================================");
  Serial.println(" ESP32-S3 VL53L1X Distance Alarm");
  Serial.println("====================================");
  Serial.println("LED    : GPIO4");
  Serial.println("Buzzer : GPIO5");
  Serial.println("SDA    : GPIO8");
  Serial.println("SCL    : GPIO9");
  Serial.println("Serial : 115200 baud");
  Serial.println();

  // ESP32-S3 사용자 지정 I2C 핀
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(400000);

  sensor.setTimeout(500);

  if (!sensor.init()) {
    fatalSensorError("VL53L1X sensor initialization failed");
  }

  // 30cm 이하 측정에 적합한 Short 모드
  if (!sensor.setDistanceMode(VL53L1X::Short)) {
    fatalSensorError("Could not set Short distance mode");
  }

  // 측정 시간 30ms
  if (!sensor.setMeasurementTimingBudget(30000)) {
    fatalSensorError("Could not set timing budget");
  }

  // 50ms마다 연속 측정
  sensor.startContinuous(50);

  lastSensorDataMs = millis();

  Serial.println("[OK] VL53L1X sensor ready");
  Serial.println("[OK] Measurement started");
  Serial.println();
}


// =====================================================
// 메인 루프
// =====================================================
void loop() {
  const uint32_t now = millis();

  // 센서 데이터가 준비되었을 때만 읽음
  if (sensor.dataReady()) {
    const uint16_t distanceMm = sensor.read(false);

    lastSensorDataMs = now;
    sensorNoDataReported = false;

    const DistanceZone measuredZone =
        classifyDistance(distanceMm);

    const bool zoneChanged =
        (measuredZone != currentZone);

    changeZone(measuredZone, now);

    // 구간 변경 시 즉시 출력하거나, 300ms마다 출력
    if (zoneChanged ||
        (uint32_t)(now - lastSerialPrintMs) >=
            SERIAL_INTERVAL_MS) {
      lastSerialPrintMs = now;

      Serial.print("[DIST] ");
      Serial.print(distanceMm);
      Serial.print(" mm / ");
      Serial.print(distanceMm / 10.0f, 1);
      Serial.print(" cm | Zone: ");
      Serial.print(zoneName(currentZone));
      Serial.print(" | Sensor status: ");
      Serial.print(
          VL53L1X::rangeStatusToString(
              sensor.ranging_data.range_status
          )
      );
      Serial.print(" | LED: ");

      if (currentZone == ZONE_NEAR) {
        Serial.print("FAST BLINK");
      } else if (currentZone == ZONE_MIDDLE) {
        Serial.print("SLOW BLINK");
      } else {
        Serial.print("ON");
      }

      Serial.print(" | Buzzer: ");
      Serial.println(
          buzzerPatternRunning ? "ACTIVE" : "OFF"
      );
    }
  }

  // 센서 데이터가 1초 이상 들어오지 않을 때 알림
  if ((uint32_t)(now - lastSensorDataMs) >= 1000 &&
      !sensorNoDataReported) {
    sensorNoDataReported = true;
    Serial.println(
        "[WARNING] VL53L1X 데이터가 1초 이상 없습니다."
    );
  }

  updateBuzzer(now);
  updateLed(now);

  delay(1);
}

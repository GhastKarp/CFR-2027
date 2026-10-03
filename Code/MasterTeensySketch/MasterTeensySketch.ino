#include <SD.h>
#include <SPI.h>
#include <SoftwareSerial.h>
#include <math.h>

const int ACCEL_LINK_RX_PIN = 23;
const int ACCEL_LINK_TX_PIN = 22;
const unsigned long ACCEL_LINK_BAUD = 115200;
SoftwareSerial accelLink(ACCEL_LINK_RX_PIN, ACCEL_LINK_TX_PIN);

const float ACCEL_SCALE = 1000.0;
const int MAX_ACCEL_MG = 8000;
const float MIN_ACCEL_VECTOR_G = 0.0;
const float MAX_ACCEL_VECTOR_G = 4.0;
const int PIN_FRONT_RIGHT = A3;
const int PIN_FRONT_LEFT  = A2;
const int PIN_REAR_RIGHT  = A1;
const int PIN_REAR_LEFT   = A0;
const int PIN_STEERING    = A6;
const int PIN_DD5         = A10;
const int PIN_DD6         = A11;
const int PIN_DD7         = A12;
const int PIN_DD8         = A13;
const int BUTTON_PIN      = A7;
const int RECORDING_LED_PIN = LED_BUILTIN;
const unsigned long SAMPLE_PERIOD_US = 5000;
const int CHIP_SELECT = BUILTIN_SDCARD;
const float ADC_MAX_COUNT = 1027.0;
const float ADC_REF_VOLTAGE = 3.3;
const unsigned long SD_RETRY_INTERVAL_MS = 1000;
const unsigned long ZERO_TRIGGER_WINDOW_MS = 1000;
const unsigned long ZERO_CAPTURE_DURATION_MS = 1000;
const unsigned long ZERO_CAPTURE_INTERVAL_MS = 5;
const char *ZERO_FILE_NAME = "zeros.csv";
File dataFile;
String fileName;

unsigned long lastSampleUs = 0;
unsigned long lastSDInitAttemptMs = 0;
unsigned long zeroWindowStartMs = 0;
unsigned long pendingRecordingStartMs = 0;
unsigned long lastRecordingStateBroadcastMs = 0;
unsigned long lastDebugPrintMs = 0;
bool isRecording = false;
bool pendingRecordingStart = false;
bool sdReady = false;
int zeroFlipCount = 0;
float latestAxG = 0.0;
float latestAyG = 0.0;
float latestAzG = 0.0;
uint16_t latestAccelSequence = 0;
unsigned long latestAccelReceivedUs = 0;
bool hasAccelData = false;

const int BUFFER_SIZE = 512;
char dataBuffer[BUFFER_SIZE];
int dataBufferPosition = 0;
char accelLinkLine[40];
int accelLinkLinePosition = 0;

bool lastReading = false;
bool lastStableState = false;
unsigned long lastDebounceTime = 0;
const unsigned long DEBOUNCE_DELAY = 50;

float zeroFrontRightAngleDeg = 0.0;
float zeroFrontLeftAngleDeg = 0.0;
float zeroRearRightTravel = 0.0;
float zeroRearLeftTravel = 0.0;
float zeroSteeringAngleDeg = 0.0;
bool hasSavedZeros = false;

void sortFloatArray(float values[], int count) {
  for (int i = 1; i < count; i++) {
    float key = values[i];
    int j = i - 1;

    while (j >= 0 && values[j] > key) {
      values[j + 1] = values[j];
      j--;
    }

    values[j + 1] = key;
  }
}

float trimmedAverage(float values[], int count) {
  if (count <= 0) {
    return 0.0;
  }

  sortFloatArray(values, count);

  int trimCount = count / 5;  // Trim 20% from each side.
  int startIndex = trimCount;
  int endIndex = count - trimCount;

  if (endIndex <= startIndex) {
    startIndex = 0;
    endIndex = count;
  }

  float sum = 0.0;
  int usedCount = 0;
  for (int i = startIndex; i < endIndex; i++) {
    sum += values[i];
    usedCount++;
  }

  if (usedCount == 0) {
    return values[count / 2];
  }

  return sum / usedCount;
}

String generateFileName() {
  char filename[24];
  unsigned int count = 0;

  while (true) {
    snprintf(filename, sizeof(filename), "DAQ%03u.csv", count);
    if (!SD.exists(filename)) break;
    count++;
  }

  return String(filename);
}

void handleAccelLinkLine(const char *line) {
  unsigned int sequence = 0;
  int axMilliG = 0;
  int ayMilliG = 0;
  int azMilliG = 0;
  int consumed = 0;

  if (sscanf(line, "A,%u,%d,%d,%d%n", &sequence, &axMilliG, &ayMilliG, &azMilliG, &consumed) != 4) {
    return;
  }

  if (line[consumed] != '\0') {
    return;
  }

  if (
    axMilliG < -MAX_ACCEL_MG || axMilliG > MAX_ACCEL_MG ||
    ayMilliG < -MAX_ACCEL_MG || ayMilliG > MAX_ACCEL_MG ||
    azMilliG < -MAX_ACCEL_MG || azMilliG > MAX_ACCEL_MG
  ) {
    return;
  }

  float axG = axMilliG / ACCEL_SCALE;
  float ayG = ayMilliG / ACCEL_SCALE;
  float azG = azMilliG / ACCEL_SCALE;
  float accelVectorG = sqrt(axG * axG + ayG * ayG + azG * azG);

  if (accelVectorG < MIN_ACCEL_VECTOR_G || accelVectorG > MAX_ACCEL_VECTOR_G) {
    return;
  }

  latestAxG = axG;
  latestAyG = ayG;
  latestAzG = azG;
  latestAccelSequence = (uint16_t)sequence;
  latestAccelReceivedUs = micros();
  hasAccelData = true;
}

void receiveAccelLink() {
  while (accelLink.available()) {
    char incoming = accelLink.read();

    if (incoming == '\r') {
      continue;
    }

    if (incoming == '\n') {
      accelLinkLine[accelLinkLinePosition] = '\0';
      handleAccelLinkLine(accelLinkLine);
      accelLinkLinePosition = 0;
      continue;
    }

    if (accelLinkLinePosition < (int)sizeof(accelLinkLine) - 1) {
      accelLinkLine[accelLinkLinePosition++] = incoming;
    } else {
      accelLinkLinePosition = 0;
    }
  }
}

void sendRecordingStateLink() {
  accelLink.print("R,");
  accelLink.println(isRecording ? 1 : 0);
  lastRecordingStateBroadcastMs = millis();
}

void broadcastRecordingStateLink() {
  if (millis() - lastRecordingStateBroadcastMs >= 250) {
    sendRecordingStateLink();
  }
}

void printDebugStatus() {
  if (millis() - lastDebugPrintMs < 500) {
    return;
  }

  lastDebugPrintMs = millis();
  Serial.print("recording=");
  Serial.print(isRecording ? "YES" : "NO");
  Serial.print(", accelRx=");
  Serial.print(hasAccelData ? "YES" : "NO");
  Serial.print(", ax=");
  Serial.print(latestAxG, 5);
  Serial.print(", ay=");
  Serial.print(latestAyG, 5);
  Serial.print(", az=");
  Serial.print(latestAzG, 5);
  Serial.print(", seq=");
  Serial.print((unsigned int)latestAccelSequence);
  Serial.print(", steering=");
  Serial.print((int) PIN_STEERING);
  Serial.print(", accelAgeUs=");
  Serial.println(hasAccelData ? (micros() - latestAccelReceivedUs) : 0);
}

void writeBufferToSD() {
  if (dataFile && dataBufferPosition > 0) {
    dataFile.write(dataBuffer, dataBufferPosition);
    dataFile.flush();
    dataBufferPosition = 0;
  }
}

bool ensureSDReady() {
  if (sdReady) {
    return true;
  }

  unsigned long nowMs = millis();
  if (nowMs - lastSDInitAttemptMs < SD_RETRY_INTERVAL_MS) {
    return false;
  }

  lastSDInitAttemptMs = nowMs;
  Serial.println("Attempting SD initialization...");

  if (SD.begin(CHIP_SELECT)) {
    sdReady = true;
    Serial.println("SD initialized");
    return true;
  }

  Serial.println("SD initialization FAILED");
  return false;
}

float mapToRange(int rawValue, int rawMin, int rawMax, float outMin, float outMax) {
  if (rawMax == rawMin) {
    return outMin;
  }

  float ratio = (float)(rawValue - rawMin) / (float)(rawMax - rawMin);

  if (ratio < 0.0) ratio = 0.0;
  if (ratio > 1.0) ratio = 1.0;

  return outMin + ratio * (outMax - outMin);
}

void readZeroSensors(
  float &frontRightAngleDeg,
  float &frontLeftAngleDeg,
  float &rearRightTravel,
  float &rearLeftTravel,
  float &steeringAngleDeg
) {
  int rawFrontRight = analogRead(PIN_FRONT_RIGHT);
  int rawFrontLeft  = analogRead(PIN_FRONT_LEFT);
  int rawRearRight  = analogRead(PIN_REAR_RIGHT);
  int rawRearLeft   = analogRead(PIN_REAR_LEFT);
  int rawSteering   = analogRead(PIN_STEERING);

  frontRightAngleDeg = mapToRange(rawFrontRight, 0, 1023, 0.0, 300.0);
  frontLeftAngleDeg  = mapToRange(rawFrontLeft, 0, 1023, 0.0, 300.0);
  rearRightTravel    = mapToRange(rawRearRight, 0, 1023, 0.0, 100.0);
  rearLeftTravel     = mapToRange(rawRearLeft, 0, 1023, 0.0, 100.0);
  steeringAngleDeg   = mapToRange(rawSteering, 0, 1023, -180.0, 180.0);
}

void appendZerosToDataFile() {
  if (!dataFile || !hasSavedZeros) {
    return;
  }

  dataFile.println();
  dataFile.println("ZERO_REFERENCE");
  dataFile.println("Front_Right_Zero_Deg,Front_Left_Zero_Deg,Rear_Right_Zero,Rear_Left_Zero,Steering_Zero_Deg");
  dataFile.print(zeroFrontRightAngleDeg, 4);
  dataFile.print(",");
  dataFile.print(zeroFrontLeftAngleDeg, 4);
  dataFile.print(",");
  dataFile.print(zeroRearRightTravel, 4);
  dataFile.print(",");
  dataFile.print(zeroRearLeftTravel, 4);
  dataFile.print(",");
  dataFile.println(zeroSteeringAngleDeg, 4);
  dataFile.flush();
}

void loadZerosFromSD() {
  if (!ensureSDReady() || !SD.exists(ZERO_FILE_NAME)) {
    return;
  }

  File zeroFile = SD.open(ZERO_FILE_NAME, FILE_READ);
  if (!zeroFile) {
    return;
  }

  zeroFile.readStringUntil('\n');
  String valuesLine = zeroFile.readStringUntil('\n');
  zeroFile.close();

  float parsedFrontRight = 0.0;
  float parsedFrontLeft = 0.0;
  float parsedRearRight = 0.0;
  float parsedRearLeft = 0.0;
  float parsedSteering = 0.0;

  int parsed = sscanf(
    valuesLine.c_str(),
    "%f,%f,%f,%f,%f",
    &parsedFrontRight,
    &parsedFrontLeft,
    &parsedRearRight,
    &parsedRearLeft,
    &parsedSteering
  );

  if (parsed == 5) {
    zeroFrontRightAngleDeg = parsedFrontRight;
    zeroFrontLeftAngleDeg = parsedFrontLeft;
    zeroRearRightTravel = parsedRearRight;
    zeroRearLeftTravel = parsedRearLeft;
    zeroSteeringAngleDeg = parsedSteering;
    hasSavedZeros = true;
    Serial.println("Loaded zeros from zeros.csv");
  }
}

void saveZerosToSD() {
  if (!ensureSDReady()) {
    Serial.println("Zero capture blocked: SD not ready");
    return;
  }

  const int maxSamples = (ZERO_CAPTURE_DURATION_MS / ZERO_CAPTURE_INTERVAL_MS) + 4;
  float frontRightSamples[maxSamples];
  float frontLeftSamples[maxSamples];
  float rearRightSamples[maxSamples];
  float rearLeftSamples[maxSamples];
  float steeringSamples[maxSamples];
  int sampleCount = 0;

  Serial.println("Capturing calibrated zeros for 1 second...");

  unsigned long captureStartMs = millis();
  while ((millis() - captureStartMs) < ZERO_CAPTURE_DURATION_MS && sampleCount < maxSamples) {
    float frontRightAngleDeg;
    float frontLeftAngleDeg;
    float rearRightTravel;
    float rearLeftTravel;
    float steeringAngleDeg;

    readZeroSensors(
      frontRightAngleDeg,
      frontLeftAngleDeg,
      rearRightTravel,
      rearLeftTravel,
      steeringAngleDeg
    );

    frontRightSamples[sampleCount] = frontRightAngleDeg;
    frontLeftSamples[sampleCount] = frontLeftAngleDeg;
    rearRightSamples[sampleCount] = rearRightTravel;
    rearLeftSamples[sampleCount] = rearLeftTravel;
    steeringSamples[sampleCount] = steeringAngleDeg;
    sampleCount++;

    delay(ZERO_CAPTURE_INTERVAL_MS);
  }

  if (sampleCount <= 0) {
    Serial.println("Zero capture failed: no samples collected");
    return;
  }

  zeroFrontRightAngleDeg = trimmedAverage(frontRightSamples, sampleCount);
  zeroFrontLeftAngleDeg = trimmedAverage(frontLeftSamples, sampleCount);
  zeroRearRightTravel = trimmedAverage(rearRightSamples, sampleCount);
  zeroRearLeftTravel = trimmedAverage(rearLeftSamples, sampleCount);
  zeroSteeringAngleDeg = trimmedAverage(steeringSamples, sampleCount);

  if (SD.exists(ZERO_FILE_NAME)) {
    SD.remove(ZERO_FILE_NAME);
  }

  File zeroFile = SD.open(ZERO_FILE_NAME, FILE_WRITE);
  if (!zeroFile) {
    Serial.println("Failed to create zeros.csv");
    sdReady = false;
    return;
  }

  zeroFile.println("Front_Right_Zero_Deg,Front_Left_Zero_Deg,Rear_Right_Zero,Rear_Left_Zero,Steering_Zero_Deg");
  zeroFile.print(zeroFrontRightAngleDeg, 4);
  zeroFile.print(",");
  zeroFile.print(zeroFrontLeftAngleDeg, 4);
  zeroFile.print(",");
  zeroFile.print(zeroRearRightTravel, 4);
  zeroFile.print(",");
  zeroFile.print(zeroRearLeftTravel, 4);
  zeroFile.print(",");
  zeroFile.println(zeroSteeringAngleDeg, 4);
  zeroFile.close();

  hasSavedZeros = true;
  Serial.println("Saved averaged zero reference to zeros.csv");
}

bool registerZeroFlip() {
  unsigned long nowMs = millis();

  if (nowMs - zeroWindowStartMs > ZERO_TRIGGER_WINDOW_MS) {
    zeroWindowStartMs = nowMs;
    zeroFlipCount = 1;
  } else {
    zeroFlipCount++;
  }

  if (zeroFlipCount >= 3) {
    zeroFlipCount = 0;
    zeroWindowStartMs = nowMs;
    saveZerosToSD();
    return true;
  }

  return false;
}

void startRecording() {
  if (!ensureSDReady()) {
    Serial.println("Recording blocked: SD not ready");
    return;
  }

  fileName = generateFileName();
  Serial.print("Creating ");
  Serial.println(fileName);

  dataFile = SD.open(fileName.c_str(), FILE_WRITE);
  if (!dataFile) {
    Serial.println("File open FAILED");
    sdReady = false;
    return;
  }

  dataBufferPosition = 0;
  dataFile.println("Time_us,Front_Right_Angle_Deg,Front_Left_Angle_Deg,Rear_Right_Travel,Rear_Left_Travel,Steering_Angle_Deg,TPS_Percent,Coolant_Temp_F,RPM,DD8_Volts,ax_g,ay_g,az_g,Accel_Seq,Accel_Age_us");
  dataFile.flush();
  isRecording = true;
  pendingRecordingStart = false;
  lastSampleUs = micros() - SAMPLE_PERIOD_US;
  digitalWrite(RECORDING_LED_PIN, HIGH);
  sendRecordingStateLink();
  Serial.println("Recording STARTED");
}

void stopRecording() {
  if (!isRecording) return;

  writeBufferToSD();
  appendZerosToDataFile();

  if (dataFile) {
    dataFile.close();
  }

  isRecording = false;
  pendingRecordingStart = false;
  digitalWrite(RECORDING_LED_PIN, LOW);
  sendRecordingStateLink();
  Serial.println("Recording STOPPED");
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000);

  pinMode(BUTTON_PIN, INPUT_PULLDOWN);
  pinMode(RECORDING_LED_PIN, OUTPUT);
  digitalWrite(RECORDING_LED_PIN, LOW);

  accelLink.begin(ACCEL_LINK_BAUD);

  delay(200);
  ensureSDReady();
  loadZerosFromSD();
}

void loop() {
  receiveAccelLink();
  broadcastRecordingStateLink();
  printDebugStatus();

  bool reading = digitalRead(BUTTON_PIN);

  if (reading != lastReading) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
    if (reading != lastStableState) {
      lastStableState = reading;
      bool zeroTriggered = registerZeroFlip();

      if (zeroTriggered) {
        pendingRecordingStart = false;
      } else if (lastStableState && !isRecording) {
        pendingRecordingStart = true;
        pendingRecordingStartMs = millis();
        Serial.println("Recording start pending...");
      }

      if (!lastStableState && isRecording) {
        stopRecording();
      } else if (!lastStableState && !isRecording) {
        pendingRecordingStart = false;
      }
    }
  }

  lastReading = reading;
  if (pendingRecordingStart && !isRecording) {
    if (!lastStableState) {
      pendingRecordingStart = false;
    } else if (millis() - pendingRecordingStartMs >= ZERO_TRIGGER_WINDOW_MS) {
      startRecording();
    }
  }

  if (!isRecording) {
    return;
  }

  unsigned long nowUs = micros();

  if (nowUs - lastSampleUs < SAMPLE_PERIOD_US) {
    return;
  }

  lastSampleUs = nowUs;

  int rawFrontRight = analogRead(PIN_FRONT_RIGHT);
  int rawFrontLeft  = analogRead(PIN_FRONT_LEFT);
  int rawRearRight  = analogRead(PIN_REAR_RIGHT);
  int rawRearLeft   = analogRead(PIN_REAR_LEFT);
  int rawSteering   = analogRead(PIN_STEERING);

  int rawDD5 = analogRead(PIN_DD5);
  int rawDD6 = analogRead(PIN_DD6);
  int rawDD7 = analogRead(PIN_DD7);
  int rawDD8 = analogRead(PIN_DD8);

  float frontRightAngleDeg = mapToRange(rawFrontRight, 0, 1023, 0.0, 300.0);
  float frontLeftAngleDeg  = mapToRange(rawFrontLeft, 0, 1023, 0.0, 300.0);
  float rearRightTravel    = mapToRange(rawRearRight, 0, 1023, 0.0, 100.0);
  float rearLeftTravel     = mapToRange(rawRearLeft, 0, 1023, 0.0, 100.0);
  float steeringAngleDeg   = mapToRange(rawSteering, 0, 1023, -180.0, 180.0);

  float tpsPercent = mapToRange(rawDD5, 0, 1027, 0.0, 100.0);
  float coolantTempF = mapToRange(rawDD6, 0, 1027, 30.0, 300.0);
  float rpmValue = mapToRange(rawDD7, 0, 1027, 0.0, 13500.0);
  float dd8Volts = rawDD8 * ADC_REF_VOLTAGE / ADC_MAX_COUNT;

  float axG = latestAxG;
  float ayG = latestAyG;
  float azG = latestAzG;
  unsigned long accelAgeUs = hasAccelData ? (nowUs - latestAccelReceivedUs) : 0;

  int len = snprintf(
    dataBuffer + dataBufferPosition,
    BUFFER_SIZE - dataBufferPosition,
    "%lu,%.4f,%.4f,%.4f,%.4f,%.4f,%.2f,%.2f,%.0f,%.4f,%.5f,%.5f,%.5f,%u,%lu\n",
    nowUs,
    frontRightAngleDeg,
    frontLeftAngleDeg,
    rearRightTravel,
    rearLeftTravel,
    steeringAngleDeg,
    tpsPercent,
    coolantTempF,
    rpmValue,
    dd8Volts,
    axG,
    ayG,
    azG,
    (unsigned int)latestAccelSequence,
    accelAgeUs
  );

  if (len <= 0) {
    return;
  }

  if (len >= (BUFFER_SIZE - dataBufferPosition)) {
    writeBufferToSD();
    len = snprintf(
      dataBuffer + dataBufferPosition,
      BUFFER_SIZE - dataBufferPosition,
      "%lu,%.4f,%.4f,%.4f,%.4f,%.4f,%.2f,%.2f,%.0f,%.4f,%.5f,%.5f,%.5f,%u,%lu\n",
      nowUs,
      frontRightAngleDeg,
      frontLeftAngleDeg,
      rearRightTravel,
      rearLeftTravel,
      steeringAngleDeg,
      tpsPercent,
      coolantTempF,
      rpmValue,
      dd8Volts,
      axG,
      ayG,
      azG,
      (unsigned int)latestAccelSequence,
      accelAgeUs
    );

    if (len <= 0 || len >= BUFFER_SIZE) {
      return;
    }
  }

  dataBufferPosition += len;

  if (dataBufferPosition >= BUFFER_SIZE - 96) {
    writeBufferToSD();
  }
}

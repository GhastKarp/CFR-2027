
bool lastHallState;
uint32_t lastHallStateFlipTime = 0;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600); //9600 baud

  pinMode(4, INPUT);

  lastHallState = digitalRead(4);
}

void loop() {
  // put your main code here, to run repeatedly:
  bool hallState = digitalRead(4);

  if(lastHallState == true && hallState == false){
    uint32_t currentTime = millis();

    float rpm = 1.0 / ((float)(currentTime - lastHallStateFlipTime) / 1000.0 / 60.0);
    Serial.println((int)rpm);
    lastHallStateFlipTime = currentTime;
  }
  
  lastHallState = hallState;
}

#include <CAN.h>

// Pin Assignments

#define DAQ_BUTTON_PIN   4

// -------------------


// Constant Value Assignments

const unsigned long button_Debounce_Millis = 50;
const unsigned long DAQ_Button_Hold_Millis = 1000;

// -------------------


// CAN Settings

const uint32_t DATA_REQUEST_CAN_ID = 0xFF000000; // CAN ID for DAQ Master's transmit request

const uint32_t DAQA_CAN_ID = 0x1CFFF000;         // CAN ID for DAQ Bus A
const uint32_t DAQB_CAN_ID = 0x1CFFF001;         // CAN ID for DAQ Bus B

const uint32_t PE6_CAN_ID = 0x0CFFF548;          // CAN ID for PE6
const uint32_t PE4_CAN_ID = 0x0CFFF348;          // CAN ID for PE4
const uint32_t PE1_CAN_ID = 0x0CFFF048;          // CAN ID for PE1

const int CAN_Bit_Rate = 500e3;

// -------------------


// Serial IDs

#define START_BYTE    0x91
#define RPM_ID        0x81
#define Gear_ID       0x82
#define BattVolt_ID   0x83
#define ECT_ID        0x84
#define Speed_ID      0x85
#define Button_ID     0x86
#define DAQStart_ID   0x87
#define END_BYTE      '\n'

enum dataType {
  RPM,
  GEAR,
  BATTERYVOLTS,
  ECT,
  SPEED,
  BUTTON,
  DAQTOGGLE
};

// -------------------


void setup() {

  // Serial startup
  Serial.begin(9600);
  while(!Serial);
  Serial1.begin(500e3);
  while(!Serial1);

  // CAN Startup
  if(!CAN.begin(CAN_Bit_Rate)){
    Serial.println("CAN Initialization Failed");
  }

  // Pinmode Assignments
  pinMode(DAQ_BUTTON_PIN, INPUT);
}



bool collectingData = false;

bool prevDAQButtonState = false;
bool buttonDebouncePeriod = false;
unsigned long buttonPressStartTime = 0;

void loop() {
  unsigned long loopTime = millis();

  // Button Press Logic
  bool DAQButtonState = digitalRead(DAQ_BUTTON_PIN);

  if(DAQButtonState && !prevDAQButtonState && !buttonDebouncePeriod){
    buttonDebouncePeriod = true;
    buttonPressStartTime = loopTime
  }

  // Button Debounce Logic (makes sure the button does not rebound and immediately turn off)
  if(buttonDebouncePeriod){
    if(loopTime > (buttonPressStartTime + button_Debounce_Millis)){
      buttonDebouncePeriod = false;
    } else {
      DAQButtonState = true;
    }
  }

  // Button Hold Logic, activates DAQ after being held
  if(!DAQButtonState && prevDAQButtonState){

    // Button was held long enough to activate data collection
    if(loopTime > (buttonPressStartTime + DAQ_Button_Hold_Millis)){
      // Update local variable
      collectingData = !collectingData

      // Send DAQ toggle to CAN bus


      // Send DAQ toggle message to dash
      writeSerialData(DAQStart_ID, {0,0});

    } else { // Standard Button Press
      // Send button press message to dash
      writeSerialData(Button_ID, {0,0});
    }
  }


  // CAN receive logic

  // Transmit RPM Logic
  uint16_t rpm = 5;
  uint8_t rpmData[] = {(uint8_t)(rpm>>8), (uint8_t)(rpm)};
  writeSerialData(RPM, rpmData);

  // Prev values refresh
  prevDAQButtonState = DAQButtonState;
}


// Utility functions

// Writes data to serial port, based on CFR protocol
void writeSerialData(dataType type, uint8_t data[2]){
  // Send the start byte to indicate the message has begun
  Serial1.write(START_BYTE);

  switch(type){
    case RPM:
    {
      Serial1.write(RPM_ID);
      Serial1.write(data[0]);
      Serial1.write(data[1]);

      break;
    }

    case GEAR:
    {
      Serial1.write(Gear_ID);
      Serial1.write(data[0]);

      break;
    }

    case BATTERYVOLTS:
    {
      Serial1.write(BattVolt_ID);
      Serial1.write(data[0]);
      Serial1.write(data[1]);

      break;
    }

    case ECT:
    {
      Serial1.write(ECT_ID);
      Serial1.write(data[0]);
      Serial1.write(data[1]);

      break;
    }

    case SPEED:
    {
      Serial1.write(Speed_ID);
      Serial1.write(data[0]);
      Serial1.write(data[1]);
      break;
    }

    case BUTTON:
    {
      Serial1.write(Button_ID);

      break;
    }

    case DAQTOGGLE:
    {
      Serial1.write(DAQStart_ID);

      break;
    }

    default:
    {
      Serial.println("error sending serial data");

      break;
    }
  }

  // Send the newline character to indicate the message has ended
  Serial1.write(END_BYTE);
}

// Returns pointer to saved serial data
uint8_t *readIncomingSerial1(){
  int idx = 0;
  static uint8_t result[4] = {0,0,0,0};

  while(idx < 3 && Serial1.available()){
    uint8_t byteReceived = Serial1.read();

    // Protection to ensure every message is synced to the start byte
    if(idx == 0 && byteReceived != START_BYTE) break;

    // Protection to end the loop when the end byte is reached
    if(byteReceived == END_BYTE){
      break;
    }

    result[idx] = byteReceived;
    idx++;
  }

  return result;
}

// ------------------------------------------------------------
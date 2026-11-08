
const uint32_t DAQA_CAN_ID = 0x1CFFF000; // CAN ID for DAQ Bus A
const uint32_t DAQB_CAN_ID = 0x1CFFF001; // CAN ID for DAQ Bus B

const uint32_t PE6_CAN_ID = 0x0CFFF548;  // CAN ID for PE6
const uint32_t PE4_CAN_ID = 0x0CFFF348;  // CAN ID for PE4
const uint32_t PE1_CAN_ID = 0x0CFFF048;  // CAN ID for PE1

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
  Serial.begin(9600);
  Serial1.begin(500e3);
}

void loop() {
  


  // Transmit RPM Logic
  uint16_t rpm = j * 82;
  uint8_t rpmData[] = {(uint8_t)(rpm>>8), (uint8_t)(rpm)};
  writeSerialData(RPM, rpmData);
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
      Serial1.write(data[0]);

      break;
    }

    case DAQTOGGLE:
    {
      Serial1.write(DAQStart_ID);
      Serial1.write(data[0]);

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
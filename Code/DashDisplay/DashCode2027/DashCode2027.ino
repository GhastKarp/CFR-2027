
/*
*  Digital dash display code designed to take a Serial input from an ESP32 and display to a digital driver-interface TFT screen.
*  Running on a Teensy 4.0 u-controller.
*/

#include <ILI9488_t3.h>
#include <ILI9488_t3_font_Arial.h>
#include <ILI9488_t3_font_ArialBold.h>

#include <SPI.h>



// TFT Pin Definitions
#define TFT_RST   8
#define TFT_DC    9
#define TFT_CS    10
#define TFT_MOSI  11
#define TFT_MISO  12
#define TFT_CLK   13
// -------------------

// Define TFT display
ILI9488_t3 tft = ILI9488_t3(&SPI, TFT_CS,  TFT_DC,  TFT_RST);
//                      24MHz clock, MSB first, Mode 0
SPISettings tftSPISettings(24000000, MSBFIRST, SPI_MODE0);


// Color Definitions
#define RGB565(r, g, b) ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
const uint16_t Black = RGB565(0, 0, 0);
const uint16_t White = RGB565(255, 255, 255);
const uint16_t Red = RGB565(255, 0, 0);
const uint16_t Yellow = RGB565(235, 235, 0);
const uint16_t Green = RGB565(0, 255, 0);
// -------------------


// Serial IDs
#define RPM_ID        0x81
#define Gear_ID       0x82
#define BattVolt_ID   0x83
#define ECT_ID        0x84
#define Speed_ID      0x85
#define Button_ID     0x86
#define DAQStart_ID   0x87
// -------------------


// Visual Spacing
#define RPM_Meter_Spacing     48
#define RPM_Meter_XOffset     24
#define RPM_Meter_YOffset     25
#define RPM_Meter_CDiameter   16

#define RPM_Text_XOffset      20
#define RPM_Text_YOffset      45

#define Gear_Text_Size        15
#define Gear_XOffset         204
#define Gear_YOffset         110
// -------------------


void setup(){

  // Initialize 
  pinMode(TFT_CS, INPUT_PULLUP);
  pinMode(TFT_CLK, INPUT_PULLUP);
  pinMode(TFT_MISO, INPUT_PULLUP);
  pinMode(TFT_MOSI, INPUT_PULLUP);

  // Serial settings
  Serial.begin(9600); // USB Debug Serial
  Serial1.begin(500e3); // Connection to ESP32
  SPI.begin(); // Initialize the SPI bus if not already initialized.
  SPI.beginTransaction(tftSPISettings); // Configure SPI interface for the display

  // Initialize TFT display
  tft.begin();
  tft.setRotation(1); // landscape mode
  tft.fillScreen(Black);

  // TFT display intitialization

  // RPM Meter:
  drawRPMMeter(0);

  // Gear Indicator:
  drawGear(0);

  // Battery Charge
  drawBatteryVolts(0.0);
}

// Display drawing functions

uint8_t rpmDots = 0;
uint8_t prevRPMArray[5] = {0,0,0,0,0};
void drawRPMMeter(uint16_t rpm){
  for(int i = 0; i < 10; i++){
    uint16_t dotColor = Red;
    if(i < 7) dotColor = Yellow;
    if(i < 3) dotColor = Green;

    if((i+1) * 1250 < rpm){
      uint16_t xPos = RPM_Meter_XOffset + i*RPM_Meter_Spacing;
      tft.fillCircle(xPos, RPM_Meter_YOffset, RPM_Meter_CDiameter - 4, dotColor);

      rpmDots = i + 1;
    } else {
      uint16_t xPos = RPM_Meter_XOffset + i*RPM_Meter_Spacing;
      tft.fillCircle(xPos, RPM_Meter_YOffset, RPM_Meter_CDiameter, White);
    }
  }


  uint8_t rpmArray[5];
  rpmArray[0] = (uint8_t)(rpm / 10000);
  rpmArray[1] = (uint8_t)((rpm % 10000) / 1000);
  rpmArray[2] = (uint8_t)((rpm % 1000) / 100);
  rpmArray[3] = (uint8_t)((rpm % 100) / 10);
  rpmArray[4] = (uint8_t)(rpm % 10);

  bool leadingZeros = true; // Protection to not write leading zeros to the screen
  for(int i = 0; i < 5; i++){
    if(rpmArray[i] != 0) leadingZeros = false;

    if(leadingZeros == false && prevRPMArray[i] != rpmArray[i]){
      tft.fillRect(RPM_Text_XOffset, RPM_Text_YOffset, 10, 20, Black);
      writeTextToScreen(rpmArray[i], RPM_Text_XOffset, RPM_Text_YOffset, 4, White);
    }

    prevRPMArray[i] = rpmArray[i];
  }
}

String prevStrGear = "0";
void drawGear(uint8_t gear){

  String strGear;
  if(gear == 0){
    strGear = "N";
  } else {
    strGear = (String)gear;
  }

  if(prevStrGear != strGear){
    tft.fillRect(Gear_XOffset, Gear_YOffset, 75, 107, Black);
    writeTextToScreen(strGear, Gear_XOffset, Gear_YOffset, Gear_Text_Size, White);
  }

  prevStrGear = strGear;
}

void drawBatteryVolts(float batteryVolts){
  // Clear area
  tft.fillRect(25, 220, 110, 100, Black);

  // Calculate individual digits
  uint8_t voltsArray[3];
  voltsArray[0] = (uint8_t)(batteryVolts) / 10;
  voltsArray[1] = (uint8_t)(batteryVolts) % 10;
  voltsArray[2] = ((uint8_t)(batteryVolts * 10.0 + 0.001)) % 10;// Minor offset applied to round up on rounded FP numbers when casting int

  // Write numbers to screen
  if(batteryVolts >= 10.0) writeTextToScreen(voltsArray[0], 25, 220, 4, White);
  writeTextToScreen(voltsArray[1], 47, 220, 4, White);
  writeTextToScreen(voltsArray[2], 82, 220, 4, White);

  // Write decimal pt and V
  writeTextToScreen(".", 62, 220, 4, White);
  writeTextToScreen("V", 107, 220, 4, White);

  // Determine color of battery symbol
  uint16_t voltColor = Green;
  if(batteryVolts < 12.0) voltColor = Yellow;
  if(batteryVolts < 11.5) voltColor = Red;

  // Draw battery symbol
  tft.fillRect(44, 270, 60, 36, voltColor); //box
  tft.fillRect(79, 263, 18, 7, voltColor);  //lead
  tft.fillRect(51, 263, 18, 7, voltColor);  //lead
  tft.fillRect(58, 279, 4, 18, Black);      //plus
  tft.fillRect(51, 286, 18, 4, Black);      //plus
  tft.fillRect(79, 286, 18, 4, Black);      //minus
}

// --------------------------------------------------------

void loop(){

  // RPM Gauge Test Code
  /*
  for(int j = 0; j < 10; j++){
    drawRPMMeter(j*1250+1);
    Serial.println(rpmDots);
    delay(400);
  }
  */

  // Gear Display Test Code
  /*
  for(int j = 0; j < 7; j++){
    drawGear(j);
    delay(800);
  }
  */

  // Battery Voltage Test Code
  /*
  for(int j = 0; j < 15; j++){
    drawBatteryVolts(12.3 - (j * 0.1));
    Serial.println(12.3 - (j * 0.1));
    delay(2500);
  }
  */


  // Serial Loop Test Code
  /*
  while(Serial.available() > 0){
    uint8_t byteReceived = Serial.read();

    Serial1.write(byteReceived);
  }
  uint8_t *testSerialData = readIncomingSerial1();

  for(int i = 0; i < 3; i++){
    Serial.println((char)serialData[i]);
  }

  delay(1000);
  */

  uint8_t *serialData = readIncomingSerial1();

  switch(serialData[0]){

    case RPM_ID:
    {
      uint16_t rpm = ((uint16_t)serialData[1] << 4) | ((uint16_t)serialData[2]);
      drawRPMMeter(rpm);
      break;
    }
    case Gear_ID:
    {
      uint8_t gear = serialData[1];
      drawGear(gear);
      break;
    }
    case BattVolt_ID:
    {
      float batt = (serialData[1] << 8 | serialData[2]) * 0.01;
      drawBatteryVolts(batt);
      break;
    }
    case ECT_ID:
    {
      //something
      break;
    }
    case Speed_ID:
    {
      //something
      break;
    }
    case Button_ID:
    {
      //something
      break;
    }
    case DAQStart_ID:
    {
      //something
      break;
    }
    default:
    {
      Serial.println("Error receiving serial data");
    }
  }
}


// Utility functions

void writeTextToScreen(String text, uint16_t x, uint16_t y, uint8_t txtSize, uint16_t txtColor){
  tft.setCursor(x,y);
  tft.setTextSize(txtSize);
  tft.setTextColor(txtColor);
  tft.print(text);
}

// Returns pointer to saved serial data
uint8_t *readIncomingSerial1(){
  int idx = 0;
  static uint8_t result[3];

  while(Serial1.available() > 0){
    uint8_t byteReceived = Serial1.read();

    if(byteReceived == '\n'){
      break;
    }

    result[idx] = byteReceived;

    idx++;
  }

  return result;
}

// ------------------------------------------------------------


/*
*  Digital dash display code designed to read CAN inputs from a PE3 ECU and display to an ILI9488
*  screen, through a teensy 4.0
*  Also supports analog output via teensy 4.0 PWM output & external filtration
*  Developed by Ethan Karpinski, 2026
*/

#include <circular_buffer.h>
#include <imxrt_flexcan.h>
#include <isotp.h>
#include <isotp_server.h>
#include <kinetis_flexcan.h>

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <ACAN_T4.h>
#include <FlexCAN_T4.h>
#include <Wire.h>
#include <ILI9488_t3.h>
#include <ILI9488_t3_font_Arial.h>
#include <ILI9488_t3_font_ArialBold.h>

#include <avr/wdt.h>

const uint32_t PE6_CAN_ID = 0x0CFFF548;  // CAN ID for PE6
const uint32_t PE4_CAN_ID = 0x0CFFF348;  // CAN ID for PE4
const uint32_t PE1_CAN_ID = 0x0CFFF048;  // CAN ID for PE1

#define TFT_DC      9 
#define TFT_CS      10
#define TFT_RST     8 
#define TFT_CLK     13
#define TFT_IN      12
#define TFT_OUT     11

#define AOut4       A0 // DD-8
#define AOut3       A1 // DD-7    RPM
#define AOut2       A4 // DD-6    ECT
#define AOut1       A5 // DD-5    TPS

const long rpmUpdateInterval = 100; // milliseconds

ILI9488_t3 tft = ILI9488_t3(&SPI, TFT_CS, TFT_DC, TFT_RST);
// Color Pallete

// Convert RGB to RGB565 format
#define RGB565(r, g, b) ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)

const uint16_t Red = RGB565(222, 26, 26);
const uint16_t OffWhite = RGB565(232, 235, 247);
const uint16_t LightBlue = RGB565(100, 110, 255);
const uint16_t Peach = RGB565(242, 211, 152);
const uint16_t Brown = RGB565(215, 133, 33);
const uint16_t Yellow = RGB565(245, 255, 1);
const uint16_t Black = RGB565(0, 0, 0);
const uint16_t White = RGB565(255, 255, 255);
const uint16_t Green = RGB565(0, 150, 0);
const uint16_t Cyan = RGB565(0, 230, 230);

// Placeholder variables
int rpmValue = 88888;
int ectValue = 175;    // Engine Coolant Temperature in degrees Fahrenheit
float batVoltage = 0;  // Battery Voltage in Volts
float oilPressure = 0; // Oil Pressure in PSI 

SPISettings tftSPISettings(24000000, MSBFIRST, SPI_MODE0); // 24MHz clock, MSB first, Mode 0

static bool advancedDisplayOn = true;

void setup() {

  pinMode(TFT_CS, INPUT_PULLUP);
  pinMode(TFT_CLK, INPUT_PULLUP);
  pinMode(TFT_IN, INPUT_PULLUP);
  pinMode(TFT_OUT, INPUT_PULLUP);

  //Serial settings
  Serial.begin(500e3); // 500 kBits/s
  SPI.begin();//Initialize the SPI bus if not already initialized.
  SPI.beginTransaction(tftSPISettings);//Configure SPI interface for the display

  //Initialize the analog outputs to 500kHz & 10 bit accuracy
  analogWriteFrequency(AOut1, 500e3); 
  analogWriteFrequency(AOut2, 500e3); // TPS Output
  analogWriteFrequency(AOut3, 500e3);
  analogWriteResolution(10); // Resolution in bits

  //CAN settings
  ACAN_T4_Settings settings (500E3);//Set CAN bitrate to 500 kbps

  //Initialize CAN1 port
  const uint32_t errorCode = ACAN_T4::can1.begin(settings);
  if (errorCode) {
    Serial.print("CAN1 initialization error: 0x");
    Serial.println(errorCode, HEX);
  } else {
    Serial.println("CAN successfully initialized");
  }

  Serial.print("Exact bitrate: ");
  Serial.println(settings.exactBitRate());

  tft.begin();
  tft.setRotation(1); // landscape mode
  tft.fillScreen(Black);



  if(advancedDisplayOn){

    // RPM Display     Out        Fill  x1 y1   x2   y2
    sectorSetup("RPM", LightBlue, Black, 0, 0, 480, 150);
    tft.fillRect(10, 35, 210, 100, LightBlue);
    tft.fillRect(15, 40, 200, 90, Black);

    // ECT Display     Out  Fill   x1  y1   x2   y2
    sectorSetup("ECT", Red, Black, 0, 150, 240, 235);
    tft.setCursor(70, 180);
    tft.setTextSize(5);
    tft.print(ectValue);
    tft.setCursor(200, 180);
    tft.setTextSize(2);
    tft.print((char)247);  // degree symbol
    tft.print("F");

    // Battery Display   Out   Fill   x1   y1   x2   y2
    sectorSetup("BAT", Green, Black, 240, 150, 480, 235);
    tft.setCursor(300, 180);
    tft.setTextSize(5);
    tft.print(batVoltage, 1);
    tft.setCursor(445, 180);
    tft.setTextSize(3);
    tft.print("V");

    // Gear Display     Out     Fill   x1 y1   x2   y2
    sectorSetup("GEA", Yellow, Black, 0, 235, 240, 320);

    // Oil Pressure     Out   Fill   x1   y1   x2   y2
    sectorSetup("Oil", Cyan, Black, 240, 235, 480, 320);
    tft.setCursor(300, 265);
    tft.setTextSize(5);
    tft.print(oilPressure, 1);
    tft.setTextSize(2);
    tft.setCursor(445, 245);
    tft.print("P");
    tft.setCursor(445, 270);
    tft.print("S");
    tft.setCursor(445, 295);
    tft.print("I");
  } else {
    // Full-screen RPM Display
    sectorSetup("RPM", LightBlue, Black, 0, 0, 480, 320);
    tft.fillRect(20, 45, 440, 150, LightBlue);
    tft.fillRect(30, 55, 420, 130, Black);
  }
}

float prevEctValue = 0.0;
float prevBatVoltage = 0.0;
float prevOilPressure = 0.0;

void loop() {
    CANMessage message;

    //ECT Test Code
    //displayEct(170);
    //delay(1000);
    //displayEct(240);
    //delay(1000);
    //displayEct(300);
    //delay(1000);


    //OilP Test Code
    //displayOilP(4);
    //delay(1000);
    //displayOilP(1.75);
    //delay(1000);
    //displayOilP(0.6);
    //delay(1000);
    

    //RPM Test Code
    //int rotationsperminute = 0;
    //for(int i = 0; i < 66; i++){
    //  displayRpm(rotationsperminute, false);
    //  drawRpmGauge(rotationsperminute, false);
    //  rotationsperminute += 217;
    //  delay(200);
    //}
    //
    //for(int i = 0; i < 66; i++){

    //  rotationsperminute -= 217;
    //  displayRpm(rotationsperminute, false);
    //  drawRpmGauge(rotationsperminute, false);
    //  delay(200);
    //}

    //Analog Output Test
    //double aOut = sin(millis() / 250.0) * 512 + 512;
    //Serial.println(aOut);
    //analogWrite(AOut2, aOut);

    while(ACAN_T4::can1.receive(message)){ // Process all messages in the buffer

      if(ACAN_T4::can1.receiveBufferCount() > 50){
        ACAN_T4::can1.receive(message); // Do nothing, clear the queue
      }

      if (message.id == PE6_CAN_ID && advancedDisplayOn) { // Battery voltage, ECT temp
        float batteryVolt = (message.data[1] << 8 | message.data[0]) * 0.01;
        float ectTemp = (message.data[5] << 8 | message.data[4]) * 0.1;

        if (batteryVolt != prevBatVoltage) {
          tft.fillRect(300, 180, 140, 50, Black);
          tft.setCursor(300, 180);
          tft.setTextSize(5);
          tft.print(batteryVolt, 1);
          tft.setCursor(445, 180);
          tft.setTextSize(3);
          tft.print("V");
          prevBatVoltage = batteryVolt;
        }

        if (ectTemp != prevEctValue) {
          displayEct(ectTemp);
          prevEctValue = ectTemp;
        }

        //Analog data output
        float analogECT = floatMap(ectTemp, 30.0, 300.0, 0, 1023);
        analogWrite(AOut2, analogECT);

      } else if (message.id == PE1_CAN_ID) { // RPM, tps
        uint16_t rpm = ((uint16_t)(message.data[1]) << 8) | (uint16_t)(message.data[0]);
        float tps = floatMap((message.data[3] << 8 | message.data[2]) * 0.1, 0.0, 100.0, 0, 1023); // tps data converted to 0-1023 (t4.0 analog)

        displayRpm(rpm, advancedDisplayOn);
        drawRpmGauge(rpm, advancedDisplayOn);

        //Analog data output
        float analogRPM = floatMap(rpm, 0.0, 14000.0, 0, 1023);
        analogWrite(AOut3, analogRPM);
        analogWrite(AOut1, tps);

      } else if (message.id == PE4_CAN_ID && advancedDisplayOn) { // Oil Pressure
        float oilPressVolts = (message.data[3] << 8 | message.data[2])*0.001;

        if (oilPressVolts != prevOilPressure) {
          displayOilP(oilPressVolts);
          prevOilPressure = oilPressVolts;
        }
      }
    }
}

void sectorSetup(char* title, uint16_t outLineColor, uint16_t fillColor, int x1, int y1, int x2, int y2){
  tft.fillRect(x1, y1, x2-x1, y2-y1, outLineColor);  // RPM Field Color
  tft.fillRect(x1+5, y1+5, x2-x1-10, y2-y1-10, fillColor);  // RPM Field Fill
  tft.fillRect(x1, y1, 45, 25, outLineColor); // RPM Title Color
  tft.fillRect(x1+5, y1+5, 37, 18, fillColor); // RPM Title Fill
  tft.setTextColor(White);
  tft.setTextSize(2);
  tft.setCursor(x1+7, y1+7);
  tft.print(title);
}

int prevEctDigits[3] = {-1,-1,-1};
uint16_t prevEctDanger = White;
void displayEct(int ectTemp){

  uint16_t dangerLevel = Green;
  /*if(ectTemp > 340){
    egress();
  } else */if(ectTemp > 240){
    dangerLevel = Red;
  } else if(ectTemp > 180){
    dangerLevel = Yellow;
  }

  int ectd0 = ectTemp % 10;
  int ectd1 = ((ectTemp % 100) - ectd0) / 10;
  int ectd2 = ((ectTemp % 1000) - (ectd1 * 10) - ectd0) / 100;

  int totalect[3] = {ectd2, ectd1, ectd0};

  bool trailingZeroes = true;
  int x = 70; // start position

  if(dangerLevel != prevEctDanger) tft.fillRect(57, 167, 116, 62, dangerLevel); // only reset color when it changes
  
  for(int i = 0; i < 3; i++){
    if(totalect[i] != 0){
      trailingZeroes = false;
    }

    tft.setTextSize(5);

    if((totalect[i] != prevEctDigits[i]) || dangerLevel != prevEctDanger){// Don't refresh the digit if it's the same as it was before
      tft.fillRect(x-3, 177, 32, 42, Black);
      if(!trailingZeroes){// Don't display trailing zeroes in front
        tft.setCursor(x, 180);
        tft.print(totalect[i]);
      }
    }
    prevEctDigits[i] = totalect[i]; // Refresh the previous value array

    x = x + 32;
  }
}

int prevOilDigits[3] = {-1,-1,-1};
uint16_t prevOilDanger = White;
void displayOilP(float oilP){

  // Convert voltage to pressure using the linear mapping
  int oilPressurePsi = (int)(oilP * 30.0 - 15.0);
  //oilPressurePsi = constrain(oilPressurePsi, 0, 125); // Constrain the value between 0 and 125 PSI

  uint16_t dangerLevel = Green;
  if(oilPressurePsi < 20){
    dangerLevel = Red;
  } else if(oilPressurePsi < 30){
    dangerLevel = Yellow;
  }

  int oild0 = oilPressurePsi % 10;
  int oild1 = ((oilPressurePsi % 100) - oild0) / 10;
  int oild2 = ((oilPressurePsi % 1000) - (oild1 * 10) - oild0) / 100;

  int totaloil[3] = {oild2, oild1, oild0};

  bool trailingZeroes = true;
  int x = 300; // start position

  if(dangerLevel != prevOilDanger) tft.fillRect(287, 252, 116, 62, dangerLevel); // only reset color when it changes
  
  for(int i = 0; i < 3; i++){
    if(totaloil[i] != 0){
      trailingZeroes = false;
    }

    tft.setTextSize(5);

    if((totaloil[i] != prevOilDigits[i]) || dangerLevel != prevOilDanger){// Don't refresh the digit if it's the same as it was before
      tft.fillRect(x-3, 262, 32, 42, Black);
      if(!trailingZeroes || i == 2){// Don't display trailing zeroes in front
        tft.setCursor(x, 265);
        tft.print(totaloil[i]);
      }
    }
    prevOilDigits[i] = totaloil[i]; // Refresh the previous value array

    x = x + 32;
  }
}

int prevRPMDigits[5] = {-1,-1,-1,-1,-1};

void displayRpm(uint16_t rpm, bool advanced){
  //if(rpm > 14500) return;

  tft.setTextColor(White);
  advanced ? tft.setTextSize(8) : tft.setTextSize(12);

  //Get each individual digit from RPM value
  uint16_t rpmd0 = rpm % 10;
  uint16_t rpmd1 = ((rpm % 100) - rpmd0) / 10;
  uint16_t rpmd2 = ((rpm % 1000) - (rpmd1 * 10) - rpmd0) / 100;
  uint16_t rpmd3 = ((rpm % 10000) - (rpmd2 * 100) - (rpmd1 * 10) - rpmd0) / 1000;
  uint16_t rpmd4 = (rpm - (rpmd3 * 1000) - (rpmd2 * 100) - (rpmd1 * 10) - rpmd0) / 10000;

  //Put it together in an int array
  int totalrpm[5] = {rpmd4, rpmd3, rpmd2, rpmd1, rpmd0};

  bool trailingZeroes = true;

  int x = 230; // Starting point x
  int y = 50; // Y-level
  int xWidth = 47; // Width of one single digit
  int yWidth = 60; // Height of one single digit

  if(!advanced){
    x = 65; // Starting point x
    y = 215; // Y-level
    xWidth = 70; // Width of one single digit
    yWidth = 90; // Height of one single digit
  }

  for(int i = 0; i <= 4; i++){
    
    tft.setCursor(x, y);

    if(totalrpm[i] != 0){
      trailingZeroes = false;
    }

    if(totalrpm[i] != prevRPMDigits[i]){// Don't refresh the digit if it's the same as it was before

      tft.fillRect(x, y, xWidth, yWidth, Black);
      if(!trailingZeroes || i == 4){// Don't display trailing zeroes in front
        tft.print(totalrpm[i]);
      }
    }
    prevRPMDigits[i] = totalrpm[i]; // Refresh the previous value array

    x = x + xWidth; // Move to next digit
  }
}

int prevRpm = 0;
int16_t prevRpmXPos = 0;

void drawRpmGauge(uint16_t rpm, bool advanced){

  int gaugeWidth = 200;
  int gaugeHeight = 90;
  int startingX = 15;
  int startingY = 45;

  if(!advanced){
    gaugeWidth = 420;
    gaugeHeight = 130;
    startingX = 30;
    startingY = 55;
  }

  uint16_t rpmColor = Green;
  if(rpm > 10000){
    rpmColor = Red;
  } else if(rpm > 7000){
    rpmColor = Yellow;
  }

  uint16_t xPosition = startingX + ((rpm * gaugeWidth) / 14500);
  if(rpm > prevRpm){
    uint16_t xWidth = xPosition - prevRpmXPos;
    tft.fillRect(xPosition-xWidth, startingY, xWidth, gaugeHeight, rpmColor);
  } else if(rpm < prevRpm){
    uint16_t xWidth = prevRpmXPos - xPosition;
    tft.fillRect(xPosition, startingY, xWidth, gaugeHeight, Black);
  }

  prevRpm = rpm;
  prevRpmXPos = xPosition;
}

float floatMap(float input, float fromLow, float fromHigh, float toLow, float toHigh){
  if(input < fromLow) return fromLow;
  if(input > fromHigh) return fromHigh;

  float percentage = (input - fromLow) / (fromHigh - fromLow);

  return (toHigh - toLow) * percentage + toLow;
}

void egress(){
  for(int i = 0; i < 10; i++){
    tft.fillScreen(Red);
    delay(500);
    tft.setTextSize(12);
    tft.setCursor(27, 120);
    tft.print("EGRESS");
    delay(500);
  }
  reboot();
}


void reboot() {
  setup();
  loop();
}

/*
*  Digital dash display code designed take a Serial input from an ESP32 and display to a digital driver-interface TFT screen. 
*/

// Define TFT display             CS  DC  RST 
ILI9488_t3 tft = ILI9488_t3(&SPI, 10,  9,  8);

// Teensy default SPI pins (To be used with display):
// CS: 10
// MOSI: 11
// MISO: 12
// CLK:  13
//                      24MHz clock, MSB first, Mode 0
SPISettings tftSPISettings(24000000, MSBFIRST, SPI_MODE0);

void setup(){

  // Initialize 
  pinMode(TFT_CS, INPUT_PULLUP);
  pinMode(TFT_CLK, INPUT_PULLUP);
  pinMode(TFT_IN, INPUT_PULLUP);
  pinMode(TFT_OUT, INPUT_PULLUP);

  // Serial settings
  Serial.begin(500e3); // 500 kBits/s
  SPI.begin();//Initialize the SPI bus if not already initialized.
  SPI.beginTransaction(tftSPISettings);//Configure SPI interface for the display

  // Initialize TFT display
  tft.begin();
  tft.setRotation(1); // landscape mode
  tft.fillScreen(Black);
}

void loop(){

}
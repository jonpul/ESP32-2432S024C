// demonstrate basic touch handling on the device
// you need TFT_eSPI and BB_CapTouch installed and TFT_eSPI configured for this device
/*
  Pin definitions for BB_CapTouch:
  TOUCH_SDA 33
  TOUCH_SCL 32
  TOUCH_INT 21
  TOUCH_RST 25

  Pin definitions for TFT_eSPI (in the SetupXX_ILI9341_ESP32.h file):
  TFT_MISO 12
  TFT_MOSI 13
  TFT_SCLK 14
  TFT_CS 15
  TFT_DC 2
  TFT_RST -1
  TFT_BL 27
  **Make sure the "#define TOUCH_CS" line is commented out (otherwise TFT_eSPI will try to handle the touch, which you don't want).
*/

#include <bb_captouch.h>

#define BACKLIGHT_PIN 27  

#include <TFT_eSPI.h>     // display 
TFT_eSPI tft = TFT_eSPI(); 

bool backlightOn = true;
// These defines are for a low cost ESP32 LCD board with the GT911 touch controller
#define TOUCH_SDA 33
#define TOUCH_SCL 32
#define TOUCH_INT 21
#define TOUCH_RST 25
#define TADR 0x15

BBCapTouch bbct;
const char *szNames[] = {"Unknown", "FT6x36", "GT911", "CST820"};

void setup() 
{
  Serial.begin(115200);
  uint8_t wbf[16];
  bbct.init(TOUCH_SDA, TOUCH_SCL, TOUCH_RST, TOUCH_INT);
  int iType = bbct.sensorType();
  Serial.printf("Sensor type = %s\n", szNames[iType]);

  // turn backlight on
  pinMode(BACKLIGHT_PIN, OUTPUT);
  digitalWrite(BACKLIGHT_PIN, HIGH);

  tft.begin();
  tft.setRotation(3);  // landscape with USB port on the left and sd card slot on top
  tft.fillScreen(TFT_BLACK);
}

void loop() 
{
  tft.drawString("Hello", 10, 10,4);
  TOUCHINFO ti;
  if (bbct.getSamples(&ti)) 
  {
    for (int i=0; i<ti.count; i++)
    {
      if(i==0) Serial.println(""); else Serial.print("  ");
      Serial.print("Touch ");Serial.print(i+1);Serial.print(": ");;
      Serial.print("  x: ");Serial.print(ti.x[i]);
      Serial.print("  y: ");Serial.print(ti.y[i]);
      Serial.print("  size: ");
      Serial.print(ti.area[i]);
    }  // for each touch point
  }
  delay(10);
}


// load and display PNGs from SPIFFS 
// you must have uploaded PNGs via the data folder before running this sketch

#include <LittleFS.h>
#define FileSys LittleFS

#include <PNGdec.h>   // Include the PNG decoder library

PNG png;
#define MAX_IMAGE_WIDTH 200 // Adjust for your images

// Include the TFT library https://github.com/Bodmer/TFT_eSPI
#include <TFT_eSPI.h>              
TFT_eSPI tft = TFT_eSPI();         


void setup()
{
  Serial.begin(115200);
  Serial.println("\n\n Using the PNGdec library");
  //Backlight Pins
  pinMode(27, OUTPUT);
  digitalWrite(27, HIGH);

  tft.setRotation(1);

  // turn off the horrible RGB LED on front
  pinMode(17, OUTPUT); // blue
  pinMode(4, OUTPUT); //red
  pinMode(16, OUTPUT); //green
  analogWrite(17, 255);
  analogWrite(4,255);
  analogWrite(16,255);

  // Initialise FS
  if (!FileSys.begin()) {
    Serial.println("LittleFS initialisation failed!");
    while (1) yield(); // Stay here twiddling thumbs waiting
  }

  // Initialise the TFT
  tft.begin();

  Serial.println("\r\nInitialisation done.");
}

void loop()
{
  tft.fillScreen(TFT_BLACK);
  
  int16_t xpos = 10;
  int16_t ypos = 10;

  int16_t rc = png.open("/xmastree_100px.png", pngOpen, pngClose, pngRead, pngSeek, pngDraw);
      
  if (rc == PNG_SUCCESS) {
    tft.startWrite();
    Serial.printf("image specs: (%d x %d), %d bpp, pixel type: %d\n", png.getWidth(), png.getHeight(), png.getBpp(), png.getPixelType());
    if (png.getWidth() > MAX_IMAGE_WIDTH) {
      Serial.println("Image too wide for allocated line buffer size!");
    }
    else {
      rc = png.decode(NULL, 0);
      png.close();
    }
    tft.endWrite();
  }
  delay(3000);
  tft.fillScreen(TFT_BLACK);
  rc = png.open("/test.png", pngOpen, pngClose, pngRead, pngSeek, pngDraw);
      
  if (rc == PNG_SUCCESS) {
    tft.startWrite();
    Serial.printf("image specs: (%d x %d), %d bpp, pixel type: %d\n", png.getWidth(), png.getHeight(), png.getBpp(), png.getPixelType());
    if (png.getWidth() > MAX_IMAGE_WIDTH) {
      Serial.println("Image too wide for allocated line buffer size!");
    }
    else {
      rc = png.decode(NULL, 0);
      png.close();
    }
    tft.endWrite();
  }
    delay(3000);  
}


//=========================================v==========================================
//                                      pngDraw
//====================================================================================
// This next function will be called during decoding of the png file to
// render each image line to the TFT.  If you use a different TFT library
// you will need to adapt this function to suit.
// Callback function to draw pixels to the display
void pngDraw(PNGDRAW *pDraw) {
  uint16_t lineBuffer[MAX_IMAGE_WIDTH];
  png.getLineAsRGB565(pDraw, lineBuffer, PNG_RGB565_BIG_ENDIAN, 0xffffffff);
  tft.pushImage(xpos, ypos + pDraw->y, pDraw->iWidth, 1, lineBuffer);
}
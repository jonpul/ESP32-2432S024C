// make sure #define TOUCH_CS line is commented out in Setup42_ILI9341_ESP32.h (the "42" may change over time, but that file for ILI9341)
// This line needs to be uncommented for the Elecrow 2.4" resistive touch display. 

// ESP32 Dev Module


#include <LittleFS.h>
#define FileSys LittleFS

#include <bb_captouch.h>
#include <TFT_eSPI.h>     // display 
#include <WiFi.h>         
#include <HTTPClient.h>
#include <Arduino_JSON.h> // https://github.com/arduino-libraries/Arduino_JSON
#include <TimeLib.h> // https://github.com/PaulStoffregen/Time
#include <time.h>
#include <math.h>

// set up event reminder struct  --- actual events are populated in secrets.h
struct event {
                  char eventName[100];      // required
                  int month;                // required 
                  int day;                  // if day equals zero "1" is assumed
                  int startYear;            // if year is > zero it will show the number of the event "22nd birthday", "37th anniversary", etc
                  uint32_t textColor;       // TFT_eSPI color in 565 format. Can also be a TFT_eSPI constant like TFT_RED or whatever
                  char iconFilename[100];   // filename matching a 100px wide PNG icon file loaded in SPIFFS, including the leading slash ("/")
                };

// to be removed
int yearMarried = 1987;



#include "secrets.h"

int maxEventIndex = (sizeof(events)/sizeof(event))-1;
int lastEventID = -1;
int currentEventID = 0;

#include <PNGdec.h>   // PNGDecoder

PNG png;
#define MAX_IMAGE_WIDTH 201 // Adjust for your images
int16_t xpos = 0;
int16_t ypos = 0;

#define minimum(a,b)     (((a) < (b)) ? (a) : (b))

// custom fonts (generated from https://oleddisplay.squix.ch/)
#include "Roboto_22.h"
#include "Roboto_12.h"
#include "Roboto_32.h"

// built in font and GFXFF reference handle
#define GFXFF 1
// font aliases
#define ROBOTO22 &Roboto_22
#define ROBOTO12 &Roboto_12
#define ROBOTO32 &Roboto_32

// gesture thresholds
#define MILLIS_LONGPRESS 500      // how long makes a tap into a tap and hold?
#define SWIPE_PIXEL_VARIANCE 40   // how many pixels to ignore from the start for a swipe gesture 
#define SWIPE_MIN_LENGTH 30       // how many pixels required to make a swipe

enum touchGesture 
{
  NOTOUCH = 0,
  TAP = 1,
  LONGTAP = 2,
  SWIPERIGHT =3,
  SWIPELEFT = 4,
  SWIPEDOWN = 5,
  SWIPEUP = 6    
};

int startX, startY;
int curX, curY;
int tapX, tapY;
long startTouchMillis, endTouchMillis;
enum touchGesture gestureResult = NOTOUCH;

// sleep timer globals
int prevHour = -1;   // used to test if hour has changed
int displaySleepHour = 1;  // the backlight sleep only works if the OFF hour is less than the ON hour. 
int displayWakeHour = 7;
bool backlightOn = true;
#define BACKLIGHT_PIN 27  // ESP32-2432S024C screen
// These defines are for a low cost ESP32 LCD board with the GT911 touch controller
#define TOUCH_SDA 33
#define TOUCH_SCL 32
#define TOUCH_INT 21
#define TOUCH_RST 25
#define TADR 0x15
BBCapTouch bbct;
const char *szNames[] = {"Unknown", "FT6x36", "GT911", "CST820"};

// Pages
// originally an enum but it wasn't actually bringing much to the party so just doing it manually
//  THOUGHT/QUOTE = 0
//  DADJOKE = 1
//  REMINDERS = 2
//  EIGHTBALL = 3
const int numPages = 4;

int curPage = 0;
bool pageJustChanged = false;

int last8BallAnswerType = -1;
int last8BallAnswer = -1;

// timer values
#define PAGE_DISPLAY_MILLIS 30000         // how long to display a page
int lastPageDisplayMillis = millis();     // initialize page pause timer
#define THOUGHT_REFRESH_MILLIS 3600000    // how long to get a new thought  (3600000 = 1 hr)
long lastThoughtRefreshMillis = millis(); // how long since we last refreshed the thought
#define DADJOKE_REFRESH_MILLIS 1800000    // how long to get a new joke
long lastDadJokeRefreshMillis = millis(); // how long since we last refreshed the joke
#define DATE_REFRESH_MILLIS 3600000      // how long to refresh date (3600000 = 1 hr)
long lastDateRefreshMillis = millis();    // how long since we last refreshed the date
#define CHECK_SLEEP_OR_WAKE_MILLIS 600000 // how long between checking sleep/wake state (600000 = 10 mins)
long lastCheckSleepWakeMillis = millis(); // how long since we last refreshed the date
#define MILLIS_8BALL_LIMIT 500           // debounce 8ball tap requests a little bit
long millisLast8BallAsk = millis();          
#define EVENT_DISPLAY_MILLIS 5000         // how long to display an event
int lastDisplayMillis = millis();         // initialize rotating display pause timer     

// have a few stored quotes in case we have rate limit or connection problems
String cannedThoughts[] = {"The shoe that fits one person pinches another. There is no recipe for living that suits all cases.","Neither a borrower nor a lender be.", "The greatest glory in living lies not in never falling, but in rising every time we fall.", "The way to get started is to quit talking and begin doing.", "Your time is limited, so don't waste it living someone else's life.","If you set your goals ridiculously high and it's a failure, you will fail above everyone else's success."};
String cannedAuthors[] = {"Carl Jung","William Shakespeare", "Nelson Mandela", "Walt Disney", "Steve Jobs", "James Cameron"};
String thought = "";
String author = "";
int thoughtLines = 0;

String joke = "";

#define DEBUG
uint16_t touchX, touchY;

TFT_eSPI tft = TFT_eSPI(); 

void setup() {
  // put your setup code here, to run once:
  // prep the display
  Serial.begin(115200);


  for(int i=0; i<=maxEventIndex; i++)
  {
    Serial.println(events[i].eventName);
    Serial.printf("%d/%d\n", events[i].month, events[i].day);
    if(events[i].startYear > 0)
       Serial.printf("Starting in %d\n",events[i].startYear);
    Serial.println(events[i].iconFilename);
    Serial.printf("Color: %04x\n",events[i].textColor);
    Serial.println("---------------------");
  }



  // turn off this horrible RGB LED (bizarre but 255 is the low value for this)
  pinMode(4, OUTPUT); //red
  pinMode(16, OUTPUT); //green
  pinMode(17, OUTPUT); // blue
  analogWrite(4,255);
  analogWrite(16,255);
  analogWrite(17,255);
 
  // Initialise FS
  if (!FileSys.begin()) {
    Serial.println("LittleFS initialisation failed!");
    while (1) yield(); // Stay here twiddling thumbs waiting
  }

  // config touch
  uint8_t wbf[16];
  bbct.init(TOUCH_SDA, TOUCH_SCL, TOUCH_RST, TOUCH_INT);
  int iType = bbct.sensorType();
  Serial.printf("Sensor type = %s\n", szNames[iType]);

  //Backlight Pins
  pinMode(BACKLIGHT_PIN, OUTPUT);
  digitalWrite(BACKLIGHT_PIN, HIGH);
  //attempt connection with retries and exit if failed
  connectWifi(false);
  tft.begin();
  tft.setRotation(1);  // landscape with USB port on the left and sd card slot on top
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_WHITE);
  if(!WiFi.isConnected())
  {
    tft.fillScreen(TFT_RED);
    tft.drawString("No Wifi connection", 10, 10,4);
    delay(50000);
  }
  if(WiFi.isConnected()) // only makes sense to do this if we have a connection
  {
    tft.fillScreen(TFT_BLACK);
    tft.drawString("Connected...", 10,10,4);
    tft.fillScreen(TFT_BLACK);
    tft.drawString("Getting time...", 10,10,4);
    getTime(true);  // we need the time to get the date to calculate days until
    tft.fillScreen(TFT_BLACK);
    tft.drawString("Getting thought...", 10,10,4);
    getThought(true);
    while(thought.length()<1 || thought.length()>100)
    {
      Serial.println(F("Thought too long, getting another"));
      getThought(true);
    }
    thought = breakStringIntoLines(thought,true);
    #ifdef DEBUG
      Serial.println(thought);
    #endif
    //displayThought();  
    tft.fillScreen(TFT_BLACK);
    tft.drawString("Getting joke...", 10,10,4);
    getDadJoke(true);
    joke = breakStringIntoLines(joke,false);
    Serial.println(joke);
    pageJustChanged = true;
  }
}

void loop() {
  // get a new quote manually and reset the timer
  TOUCHINFO ti;
  if (bbct.getSamples(&ti)) 
  {
    touchX = ti.x[0];
    touchY = ti.y[0];
    if(startTouchMillis == 0)  // new touch
    {
      startX = touchX;
      startY = touchY;
      curX = startX;
      curY = startY;
      startTouchMillis = millis();
    }
    else  // continuing touch
    {
      curX = touchX;
      curY = touchY;
    }
  }
  else  // no touch
  {
    if(startTouchMillis !=0)   // a touch just ended
    {
      endTouchMillis = millis();   
      if(endTouchMillis - startTouchMillis >= MILLIS_LONGPRESS) // tap & hold
      {
        tapX = curX;
        tapY = curY;
        gestureResult = LONGTAP;
      }
      else if((curX > startX) &&
              (abs(curX-startX) > SWIPE_MIN_LENGTH) && 
              (abs(curY-startY)< SWIPE_PIXEL_VARIANCE))  // right swipe
      {
        gestureResult = SWIPERIGHT;
        Serial.println("Swipe right");
      }
      else if((curX < startX) &&
              (abs(curX-startX) > SWIPE_MIN_LENGTH) && 
              (abs(curY-startY)< SWIPE_PIXEL_VARIANCE))  // left swipe
      {
        gestureResult = SWIPELEFT;
        Serial.println("Swipe left");
      }
      else if((curY > startY) &&
              (abs(curY-startY) > SWIPE_MIN_LENGTH) && 
              (abs(curX-startX)< SWIPE_PIXEL_VARIANCE))  // down swipe
      {
        gestureResult = SWIPEUP;
        Serial.println("Swipe up");
      }
      else if((curY < startY) &&
              (abs(curY-startY) > SWIPE_MIN_LENGTH) && 
              (abs(curX-startX)< SWIPE_PIXEL_VARIANCE))  // up swipe
      {
        gestureResult = SWIPEDOWN;
        Serial.println("Swipe down");
      } 
      else // must be a regular tap if we've gotten here
      {
        tapX = curX;
        tapY = curY;
        gestureResult = TAP;
      }
      // reset the touch tracking
      startTouchMillis = 0;  
      startX = 0;
      startY = 0;
      curX = 0;
      curY = 0;
      endTouchMillis = 0;
    }
  } // end gesture handling


  if(gestureResult == TAP)
  {
    if(!backlightOn)
    {
      setDisplayAwake(true);
      gestureResult = NOTOUCH;
    }
    else
    {
      switch(curPage)
      {
        case 0: // nothing on thought
          break; 
        case 1: // nothing on joke
          break;
        case 2: // nothing on events page
          break;
        case 3: // nothing on 8 ball
          displayMagic8Ball(random(3),-1);
          pageJustChanged = true;
          lastPageDisplayMillis = millis(); // reset page display timer
          break;
      }
      gestureResult = NOTOUCH;
    } 
  }
  if (gestureResult == LONGTAP) 
  {
    if(!backlightOn)
    {
      setDisplayAwake(true);
      gestureResult = NOTOUCH;
    }
    else
    {
      switch(curPage)
      {
        case 0: // thought/quote, get a new one
          thought = "";
          while(thought.length()<1 || thought.length()>100)
          {
            #ifdef DEBUG
              Serial.println(F("Thought too long, getting another"));
            #endif
            getThought(false);
          }
          thought = breakStringIntoLines(thought,true);
          lastThoughtRefreshMillis = millis();           
          pageJustChanged = true;
          lastPageDisplayMillis = millis(); // reset page display timer
          break; 
        case 1: // dad joke, get a new one
          joke = "";
          while(joke.length()<1 || joke.length()>115)
          {
            #ifdef DEBUG
              Serial.println(F("Joke too long, getting another"));
            #endif
            getDadJoke(false);
          }
          joke = breakStringIntoLines(joke,false);
          lastDadJokeRefreshMillis = millis();   
          pageJustChanged = true;
          lastPageDisplayMillis = millis(); // reset page display timer
          break;
        case 2: // events
          break;
        case 3: // 8 ball
          break;
      }
      gestureResult = NOTOUCH;
    }
  }

  if(gestureResult == SWIPELEFT)
  {
    // what to do for SWIPELEFT on each page type
    switch (curPage)
    {
      case 0: // do nothing for thought
        break;
      case 1: // do nothing for joke
        break;
      case 2: // previous event
        currentEventID--;
        lastDisplayMillis = millis();
        pageJustChanged = true;
        break;
      case 3: // do nothing for 8 ball
        break;
    }
    gestureResult = NOTOUCH;
  }
  if(gestureResult == SWIPERIGHT)
  {
    switch (curPage)
    {
      // what to do for SWIPERIGHT on each page type
      case 0: // nothing for thought
        break;
      case 1: // do nothing for joke
        break;
      case 2: // next event
        currentEventID++;
        lastDisplayMillis = millis();
        pageJustChanged = true;
        break;
      case 3: // nothing for 8 ball
        break;
    }
    gestureResult = NOTOUCH;
  }
  if(gestureResult == SWIPEDOWN)
  {
    // move to next page type 
    curPage++;
    lastPageDisplayMillis = millis(); // reset page display timer
    if(curPage > (numPages-1))
    {
      curPage = 0;
    }
    gestureResult = NOTOUCH;
    pageJustChanged = true;
   }
  if(gestureResult == SWIPEUP)
  {
    // move to prev page type
    curPage--;
    lastPageDisplayMillis = millis(); // reset page display timer
    if(curPage < 0)
    {
      curPage = numPages-1;
    }
    gestureResult = NOTOUCH;
    pageJustChanged = true;
  }






  // display the page based on the type - only if backlight is on
  if(backlightOn)
  {
    switch(curPage)
    {
      case 0:  // thought/quote
        if(millis() > (THOUGHT_REFRESH_MILLIS + lastThoughtRefreshMillis))
        {
          getThought(false); 
          thought = breakStringIntoLines(thought, true);
          lastThoughtRefreshMillis = millis();
        }
        if(pageJustChanged)
        {
          displayThought();
        }
        last8BallAnswerType = -1;
        last8BallAnswer = -1;
        pageJustChanged = false;
        break;
      case 1:  // dad joke
        if(millis() > (DADJOKE_REFRESH_MILLIS + lastDadJokeRefreshMillis))
        {
            getDadJoke(false);
            joke = breakStringIntoLines(joke, false);
            lastDadJokeRefreshMillis = millis();
        }
        if(pageJustChanged)
        {
          displayDadJoke();
        }
        last8BallAnswerType = -1;
        last8BallAnswer = -1;
        pageJustChanged = false;
        break;
      case 2:  // days til anniversary
          if(pageJustChanged)
          {
            if(currentEventID > maxEventIndex)
            {
              currentEventID = 0;
            }
            else if (currentEventID < 0)
            {
              currentEventID = maxEventIndex;
            }
            if(lastEventID == -1)
            {
              lastEventID = currentEventID;
            }
            displayDaysToEvent(currentEventID);
            lastDisplayMillis = millis(); // keep the event from changing if we just got to this page
          }
          pageJustChanged = false;
          // this rotates through the events on this page, if it's time to change, bump the eventID and set pageJustChanged
          if(millis() > (EVENT_DISPLAY_MILLIS + lastDisplayMillis))
          {
            currentEventID++;  // next event
            lastDisplayMillis = millis();
            pageJustChanged = true; 
          }
          last8BallAnswerType = -1;
          last8BallAnswer = -1;
          break;
      case 3: // 8ball
        if(pageJustChanged)
        {
          displayMagic8Ball(last8BallAnswerType, last8BallAnswer);
        }
        pageJustChanged = false;
        break;

    }

    // check the page timer and increment the page if its time (unless stopwatch is running)
    if(millis() > (lastPageDisplayMillis + PAGE_DISPLAY_MILLIS))
    {
      curPage++;
      lastPageDisplayMillis = millis(); // reset page display timer
      if(curPage > (numPages-1))
      {
        curPage = 0;
      }
      //gestureResult = NOTOUCH;
      pageJustChanged = true;

      tft.fillScreen(TFT_BLACK);
    }
  }  
  // update the date periodically
  if(millis() > (lastDateRefreshMillis + DATE_REFRESH_MILLIS))
  {
      getTime(false);
      Serial.println(F("date refreshed by timer"));
      lastDateRefreshMillis = millis();
      // no need for pageJustChanged as we're not displaying the date anywhere
  }

  // check sleep/wake 
  if(millis() > (lastCheckSleepWakeMillis + CHECK_SLEEP_OR_WAKE_MILLIS))
  {
    checkSleepOrWake();
    lastCheckSleepWakeMillis = millis();
  }
}

// quietMode = true will try to connect but won't show any messages. 
bool connectWifi(bool quietMode)
{
  #ifdef DEBUG
    if(quietMode)
    {
      Serial.println(F("connecting wifi in quiet mode"));
    }
    else
    {
      Serial.println(F("connecting wifi in non-quiet mode"));
    }
  #endif
  const int numRetries =5;
  int retryCount = 0;
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(1); 
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE);
  // Check if the secrets have been set
  if (!quietMode && String(SSID) == "YOUR_SSID" || String(SSID_PASSWORD) == "YOUR_SSID_PASSWORD") {
    tft.drawString("Missing secrets!", tft.width()/2, 70,4);
    #ifdef DEBUG
      Serial.println(F("Please update the secrets.h file with your credentials before running the sketch."));
      Serial.println(F("You need to replace YOUR_SSID and YOUR_WIFI_PASSWORD with your WiFi credentials."));
    #endif
    delay(1000);
    return false;  // Stop further execution of the code
  }

  // Connect to Wi-Fi
  WiFi.begin(SSID, SSID_PASSWORD);
  while (!WiFi.isConnected() && (retryCount <= numRetries)) 
  {
    delay(1500);
    if(!quietMode)
    {
      tft.fillScreen(TFT_BLACK);
      tft.drawString("Connecting WiFi...", tft.width()/2, 70,4);
    }
    retryCount++;
  }
  if(WiFi.isConnected())
  {
    if(!quietMode)
    {
      tft.drawString("Connected!", tft.width()/2, 100,4);
      delay(500);
    }
    return(true);
  }
  else if (String(BACKUP_SSID)!="")  // try the backup if there is one
  {
    retryCount = 0; // reset retry count for backup SSID
    WiFi.disconnect();
    delay(500);
    WiFi.begin(BACKUP_SSID, BACKUP_SSID_PASSWORD);
    while (!WiFi.isConnected() && (retryCount <= numRetries)) 
    {
      delay(1500);
      if(!quietMode)
      {
        tft.fillScreen(TFT_BLACK);
        tft.drawString("Backup WiFi...", tft.width()/2, 70,4);
      }
      retryCount++;
    }
    if(WiFi.isConnected())
    {
      if(!quietMode)
      {
        tft.drawString("Connected!", tft.width()/2, 100,4);
        delay(500);
      }
      Serial.println("****Connected to backup wifi****");
      return(true);
    }
    else
    {
      return(false);
    }
  }
}

void getThought(bool firstRun)
{
  // don't bother if screen is asleep
  if(!backlightOn && !firstRun)
  {
    #ifdef DEBUG
      Serial.println(F("Skipping thought update. Display is asleep."));
    #endif
    return;
  }

  #ifdef DEBUG
    if(firstRun)
    {
      Serial.println(F("STARTUP: getting thought"));
    }
    else
    {
      Serial.println(F("NON-STARTUP: getting thought"));
    }
  #endif

  JSONVar jsonObj = null;

  if(WiFi.isConnected())
  {
    // Initialize the HTTPClient object
    HTTPClient http;
    tft.fillCircle(tft.width()-6,tft.height()-6,5,TFT_BLUE); // draw refresh indicator dot
    
    // Construct the URL using token from secrets.h  
    //this is weatherapi.com one day forecast request, which also returns location and current conditions
    // use zipcode if there is one, otherwise use public IP location 
    String url = "https://www.stands4.com/services/v2/quotes.php?uid="+(String)THOUGHT_USERID+"&tokenid="+(String)THOUGHT_TOKEN+"&format=json";
    
    // Make the HTTP GET request 
    http.begin(url);
    int httpCode = http.GET();

    String payload = "";
    // Check the return code
    if (httpCode == HTTP_CODE_OK) {
      // If the server responds with 200, return the payload
      payload = http.getString();
      #ifdef DEBUG
        Serial.println(payload);
      #endif
    } 
    else 
    {
      // For any other HTTP response code, print it
      #ifdef DEBUG
        Serial.println(F("Received unexpected HTTP response:"));
        Serial.println(httpCode);
      #endif
      http.end();
      // if there was an error just use a canned quote
      selectCannedThought();
      return;
    }
    // End the HTTP connection
    http.end();

    // if there was an error just use a canned quote
    if(payload.substring(2,7)=="error")
    {
      selectCannedThought();
      return; 
    }

    // Parse response
    jsonObj = JSON.parse(payload);
    // // Read values
    thought = (String)jsonObj["result"]["quote"];
    author = (String)jsonObj["result"]["author"];

    #ifdef DEBUG
      Serial.println(F("Thought refreshed"));
    #endif

  }
  else if(!firstRun)
  {
    connectWifi(true);
  }
  tft.fillCircle(tft.width()-6,tft.height()-6,5,TFT_BLACK); // erase refresh indicator dot
}

void displayThought()
{
  // final length check 
  while(thought.length()<1 || thought.length()>100)
  {
    #ifdef DEBUG
      Serial.println(F("Thought too long, getting another"));
    #endif
    getThought(false);
  }
  
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(1); 
  tft.setTextColor(TFT_WHITE);
  tft.setFreeFont(ROBOTO22);

  // center thought lines vertically on the screen
  int thoughtStartY = (tft.height()/2)-((tft.fontHeight(GFXFF)*thoughtLines)/2);  
  int authorY = thoughtStartY+(tft.fontHeight(GFXFF)*thoughtLines);

  tft.setViewport(5, thoughtStartY-17,314, (authorY-thoughtStartY)+20);
  tft.setCursor(0,20);
  tft.print(thought.c_str());
  tft.setFreeFont(ROBOTO12);
  String paddedAuthor = author+"   ";
  tft.setCursor(tft.width()-tft.textWidth(paddedAuthor.c_str()), (authorY-thoughtStartY)+9);  // need to set this position before changing smaller font
  tft.setTextDatum(MR_DATUM);
  tft.print(paddedAuthor.c_str());

  tft.resetViewport();

  // draw filigree
  // top
  tft.drawFastHLine(40, thoughtStartY-30, tft.width()-80, TFT_DARKGREY);
  tft.drawCircle(40, thoughtStartY-35, 3, TFT_DARKGREY);
  tft.drawCircle(tft.width()-40, thoughtStartY-35, 3, TFT_DARKGREY);
  tft.drawCircle(tft.width()/2, thoughtStartY-35, 3, TFT_DARKGREY);
  //bottom
  tft.drawFastHLine(40, authorY+20, tft.width()-80, TFT_DARKGREY);
  tft.drawCircle(40, authorY+25, 3, TFT_DARKGREY);
  tft.drawCircle(tft.width()-40, authorY+25, 3, TFT_DARKGREY);
  tft.drawCircle(tft.width()/2, authorY+25, 3, TFT_DARKGREY);
}

String breakStringIntoLines(String item, bool countThoughtLines)
{
  const int lineSize = 23;
  const int forwardWindow = 3; // how many chars to look ahead to find a space to replace with \n
  const int backWindow = 10; // how many chars to look back to find a space if forwardWindow doesn't have one

  String thoughtWithBreaks = "";
  int pos = 0;
  
  if(countThoughtLines)
    thoughtLines = 1;  // used to position the author name in display

  // handle the easy case - no breaks needed
  if(item.length()<=lineSize)
  {
    return item;
  }
  else
  {
    pos = 0;

    while((item.length()-pos) > lineSize)
    {
      bool splitMade = false;
      pos += lineSize;
      for(int i=0; i<forwardWindow; i++)
      {
        //Serial.println("Searching forward");
        // look ahead within the window to find a space
        if(isSpace(item.charAt(pos+i)))   // ******** TODO also look for a hyphen ("-") and leave it but put a line break AFTER that if you find it
        {
          item.setCharAt(pos+i,'\n');
          //Serial.println("Sub made");
          splitMade = true;
          pos= (pos + i) + 1; // reset position to where we made the replacement. That's now the start of the next line. 
          if (countThoughtLines)
            thoughtLines+=1;
          break;
        }
      }
      if(item.length()-pos > forwardWindow)
      {
        if(!splitMade)
        {
          for(int i=0; i<backWindow; i++)
          {
            //Serial.println("Searching backward");
            if(isSpace(item.charAt(pos-i)))  // ******** TODO also look for a hyphen ("-") and leave it but put a line break AFTER that if you find it
            {
              item.setCharAt(pos-i,'\n');
              //Serial.println("sub made");
              splitMade = true;
              pos = (pos-i) + 1; // reset position to where we made the replacement. That's now the start of the next line.
              if(countThoughtLines)
                thoughtLines+=1;
              break;
            }
          }
        }
      }
    }
  }
  return item;
}

void selectCannedThought()
{
  int randThought = random(sizeof(cannedThoughts)/sizeof(String));
  thought = cannedThoughts[randThought];
  author = cannedAuthors[randThought];
}

void getDadJoke(bool firstRun)
{
  // don't bother if screen is asleep
  if(!backlightOn && !firstRun)
  {
    #ifdef DEBUG
      Serial.println(F("Skipping joke update. Display is asleep."));
    #endif
    return;
  }

  #ifdef DEBUG
  if(firstRun)
  {
    Serial.println(F("STARTUP: getting dad joke"));
  }
  else
  {
    Serial.println(F("NON-STARTUP: getting dad joke"));
  }
  #endif

  JSONVar jsonObj = null;

  if(WiFi.isConnected())
  {
    // Initialize the HTTPClient object
    HTTPClient http;
    tft.fillCircle(tft.width()-6,tft.height()-6,5,TFT_BLUE); // draw refresh indicator dot
    
    // Construct the URL using token from secrets.h  
    //this is weatherapi.com one day forecast request, which also returns location and current conditions
    // use zipcode if there is one, otherwise use public IP location 
    String url = "https://icanhazdadjoke.com/";
    
    //Make the HTTP GET request 
    http.begin(url);
    http.addHeader("accept","application/json");
    int httpCode = http.GET();

    String payload = "";
    // Check the return code
    if (httpCode == HTTP_CODE_OK) {
      // If the server responds with 200, return the payload
      payload = http.getString();
      #ifdef DEBUG
        Serial.println(payload);
      #endif
    } 
    else 
    {
      // For any other HTTP response code, print it
      #ifdef DEBUG
        Serial.println(F("Received unexpected HTTP response:"));
        Serial.println(httpCode);
      #endif
      http.end();
      // if there was an error just use a placeholder
      joke = "My friend told me that pepper is the best seasoning for a roast, but I took it with a grain of salt.";
    }
    // End the HTTP connection
    http.end();

   // Parse response
    jsonObj = JSON.parse(payload);
    joke = (String)jsonObj["joke"];
    // replace select unicode chars (leave the backslash)
    // u2018 = '
    joke.replace("\u2018","\'");
    // u2019 = '
    joke.replace("\u2019","\'");
    // u201c = "
    joke.replace("\u201c","\"");
    // u201d = "
    joke.replace("\u201d","\"");
    // change \n to space
    joke.replace("\n"," ");
    // remove \r
    joke.replace("\r", "");

    #ifdef DEBUG
      Serial.println(F("joke refreshed"));
    #endif
  }
  else if(!firstRun)
  {
    connectWifi(true);
  }
  tft.fillCircle(tft.width()-6,tft.height()-6,5,TFT_BLACK); // erase refresh indicator dot
}

void displayDadJoke()
{
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(1); 
  tft.setTextColor(TFT_WHITE);
  tft.setFreeFont(ROBOTO22);

  int16_t rc = png.open("/tinydadjoke.png", pngOpen, pngClose, pngRead, pngSeek, pngDraw);
      
  if (rc == PNG_SUCCESS) 
  {
    tft.startWrite();
    //Serial.printf("image specs: (%d x %d), %d bpp, pixel type: %d\n", png.getWidth(), png.getHeight(), png.getBpp(), png.getPixelType());
    if (png.getWidth() > MAX_IMAGE_WIDTH) {
      Serial.println("Image too wide for allocated line buffer size!");
    }
    else 
    {
      xpos = 5;
      ypos = 5;
      rc = png.decode(NULL, 0);

      xpos =  tft.width()-55;
      ypos = 5;
      rc = png.decode(NULL, 0);
      png.close();
    }
    tft.endWrite();
  }

  int jokeStartY = 60;

  tft.setViewport(5, jokeStartY-17,314, 180);
  tft.setCursor(0,40);
  tft.print(joke.c_str());
  tft.resetViewport();
}

void displayDaysToEvent(int eventID)
{
  String yearOrdinal = getYearOrdinal(events[currentEventID].startYear, events[currentEventID].month, events[currentEventID].day);

  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(1); 
  tft.setTextColor(TFT_WHITE);
  String daysTo = (String)daysBetweenDateAndNow(year(), events[currentEventID].month, events[currentEventID].day);
  tft.drawString(daysTo, 190, 30,7);
  tft.setFreeFont(ROBOTO22);
  tft.drawString("days until",200, 80,GFXFF);


  tft.setTextColor(events[eventID].textColor);
  // if there is a year ordinal (because a start year was specified) draw it, otherwise don't
  if(yearOrdinal.length() > 0)
  {
    tft.drawString(yearOrdinal,200, 110, GFXFF);
    tft.drawString(events[eventID].eventName,200, 140, GFXFF);
  }
  else
  {
    tft.drawString(events[eventID].eventName,200, 110, GFXFF);
  }
  
  int16_t rc = png.open(events[eventID].iconFilename, pngOpen, pngClose, pngRead, pngSeek, pngDraw);
  // draw the PNG we opened 
  if (rc == PNG_SUCCESS) 
  {
    tft.startWrite();
    //Serial.printf("image specs: (%d x %d), %d bpp, pixel type: %d\n", png.getWidth(), png.getHeight(), png.getBpp(), png.getPixelType());
    xpos = 5;
    ypos = (tft.height()-png.getHeight())-5;
    if (png.getWidth() > MAX_IMAGE_WIDTH) {
      Serial.println("Image too wide for allocated line buffer size!");
    }
    else {
      rc = png.decode(NULL, 0);
      png.close();
    }
    tft.endWrite();        
  }
}

void getTime(bool firstRun)
{
  #ifdef DEBUG
    if(firstRun)
    {
      Serial.println(F("STARTUP: getting time"));
    }
    else
    {
      Serial.println(F("NON-STARTUP: getting time"));
    }
  #endif
  if(WiFi.isConnected())
  {
    JSONVar jsonObj = null;

    // Initialize the HTTPClient object
    HTTPClient http;
    
    // Construct the URL using token from secrets.h  
    //this is WorldTimeAPI.org time request for current public IP
    String url = F("https://worldtimeapi.org/api/ip");
    // Make the HTTP GET request 
    http.begin(url);
    int httpCode = http.GET();

    String payload = "";
    // Check the return code
    if (httpCode == HTTP_CODE_OK) {
      // If the server responds with 200, return the payload
      payload = http.getString();
    } else if (httpCode == HTTP_CODE_UNAUTHORIZED) {
      // If the server responds with 401, print an error message
      #ifdef DEBUG
        Serial.println(F("Time API Key error."));
        Serial.println(String(http.getString()));
      #endif
    } else {
      // For any other HTTP response code, print it
      #ifdef DEBUG
        Serial.println(F("Received unexpected HTTP response:"));
        Serial.println(httpCode);
      #endif
    }
    // End the HTTP connection
    http.end();

    // Parse response
    jsonObj = JSON.parse(payload);
    
    // get local time 
    const char* localTime = jsonObj["datetime"];

    #ifdef DEBUG
      Serial.println(payload);
      Serial.println(localTime);
    #endif
    parseTime(localTime);
    
    #ifdef DEBUG
      Serial.println(F("Time refreshed"));
    #endif
  }
  else if(!firstRun)
  {
    connectWifi(true);
  }
}

void parseTime(const char* localTime)
{
  // time string looks like "2024-07-07T09:40:04.944551-05:00",
  // consts to avoid magic numbers
  const int startYr = 0;
  const int lenYr = 4;
  const int startMon = 5;
  const int lenMon = 2;
  const int startDay = 8;
  const int lenDay =  2;
  const int startHr = 11;
  const int lenHr = 2;
  const int startMin = 14;
  const int lenMin = 2;
  const int startSec = 17;
  const int lenSec = 2;
  
  // extract the year
  char cYr[lenYr+1];
  for (int i=startYr; i<lenYr; i++)
  {
    cYr[i]=localTime[i];
  }
  cYr[lenYr]='\0';
  int tYr;
  sscanf(cYr,"%d", &tYr);
  //year = tYr;

  // extract the month
  char cMon[lenMon+1];
  for (int i=0; i<lenMon; i++)
  {
    cMon[i]=localTime[i+startMon];
  }
  cMon[lenMon]='\0';
  int tMon;
  sscanf(cMon,"%d", &tMon);
  //month = tMon;
  // extract the day
  char cDay[lenDay+1];
  for (int i=0; i<lenDay; i++)
  {
    cDay[i]=localTime[i+startDay];
  }
  cDay[2]='\0';
  int tDay;
  sscanf(cDay,"%d", &tDay);
  //day = tDay;

  // extract the hour
  char cHr[lenHr+1];
  for (int i=0; i<lenHr; i++)
  {
    cHr[i]=localTime[i+startHr];
  }
  cHr[lenHr]='\0';
  int tHr;
  sscanf(cHr,"%d", &tHr);

  // extract the minute
  char cMin[lenMin+1];
  for (int i=0; i<lenMin; i++)
  {
    cMin[i]=localTime[i+startMin];
  }
  cMin[lenMin]='\0';
  int tMin;
  sscanf(cMin,"%d", &tMin);

  // extract the minute
  char cSec[lenMin+1];
  for (int i=0; i<lenSec; i++)
  {
    cSec[i]=localTime[i+startSec];
  }
  cSec[lenSec]='\0';
  int tSec;
  sscanf(cSec,"%d", &tSec); 

  setTime(tHr,tMin,tSec,tDay,tMon,tYr);  
  Serial.printf("Parsed and set time......  %d:%d:%d %d/%d/%d\n",hour(), minute(), second(), month(), day(), year());
}

String getYearOrdinal(int eventStartYear, int eventMonth, int eventDay)
{
  int nbrYears = 0;

  if(year() < eventStartYear || eventStartYear <= 0)  // event doesn't have a start year or hasn't happened yet, return empty string
  {
    return "";
  }
  else if((month() < eventMonth) || (month() == eventMonth && day() <= eventDay))  // simple case, still in this year but in the future or today
  {  
    nbrYears = year()-eventStartYear;
  }
  else  // event is passed for this year, so add one to year before doing the math
  {
    nbrYears = (year()+1)-eventStartYear;
  }

  // add the st, nd, rd, th
  if ((nbrYears % 10 == 1) && (nbrYears % 100 != 11))
  {
      return (String)nbrYears+"st";
  }
  else if ((nbrYears % 10 == 2) && (nbrYears % 100 != 12))
  {
      return (String)nbrYears+"nd";
  }
  else if ((nbrYears % 10 == 3) && (nbrYears % 100 != 13))
  {
    return (String)nbrYears+"rd";
  }
  else
  {
    return (String)nbrYears+"th";
  }
}

int daysBetweenDateAndNow (int tYear, int tMonth, int tDay)
{
    struct tm tm1 = { 0 };
    struct tm tm2 = { 0 };

    /* date 1: today's date in year/month/day globals */
    tm1.tm_year = year() - 1900;
    tm1.tm_mon = month() - 1;
    tm1.tm_mday = day();
    tm1.tm_hour = tm1.tm_min = tm1.tm_sec = 0;
    tm1.tm_isdst = -1;

    /* date 2: the target date passed in  */
    tm2.tm_year = tYear - 1900;
    tm2.tm_mon = tMonth - 1;
    tm2.tm_mday = tDay;
    tm2.tm_hour = tm2.tm_min = tm2.tm_sec = 0;
    tm2.tm_isdst = -1;

    time_t t1 = mktime(&tm1);
    time_t t2 = mktime(&tm2);

    double dt = difftime(t2, t1);
    int days = round(dt / 86400);
  
    if(days<0)
    {
      // this means we are already past the date for this year and need to add one to the year and calc again. 
      tm2.tm_year = (tYear+1) - 1900;
      t2 = mktime(&tm2);
      double dt = difftime(t2, t1);
      days = (round(dt / 86400));
    }
    return days;
}

void setDisplayAwake(bool setBacklightOn)
{
  backlightOn = setBacklightOn;
  if(setBacklightOn)
  {
    digitalWrite(BACKLIGHT_PIN, HIGH);
    #ifdef DEBUG
      Serial.printf("Backlight on at %d:%d:%d\n",hour(),minute(),second());
    #endif
    // refresh things we ignored when the screen was off
    getThought(false);
    lastThoughtRefreshMillis = millis();
    getDadJoke(false);
    lastDadJokeRefreshMillis = millis();
    pageJustChanged = true;
  }
  else
  {
    digitalWrite(BACKLIGHT_PIN, LOW);
    #ifdef DEBUG
      Serial.printf("Backlight off at %d:%d:%d\n",hour(),minute(),second());
    #endif
  }
}
void checkSleepOrWake()
{
 // when the hour changes check if we should sleep or wake up the display (which only happens on the hour
  if(prevHour != hour())
  {
    prevHour = hour();
    Serial.printf("prevhour=%d displaySleepHour=%d\n",prevHour, displaySleepHour);

    if(hour() >= displaySleepHour && hour() < displayWakeHour)
    {
      // time for to sleep the display
      if(backlightOn)
      {
        setDisplayAwake(false);
      }
    }
    else if(hour() >= displayWakeHour)
    {
      // time to wake up the display
      if(!backlightOn)
      {
        setDisplayAwake(true);
      }
    }
  }
  //reset checkSleepOrWakeTimer
}

void displayMagic8Ball(int answerType, int answer)
{
  if(millis() < millisLast8BallAsk + MILLIS_8BALL_LIMIT)
  {
    return;
  }
  // only draw the heading if we aren't showing a result
  tft.setTextDatum(MC_DATUM);
  if(answerType == -1)
  {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    tft.setFreeFont(ROBOTO32);
    tft.drawString("Ask the Magic 8 Ball", tft.width()/2, 20, GFXFF); 
  } 

  // on tap of the 8ball page, get a random 0-3 and pass it in here

  char * affirmative[] = {  "It is certain",
                          "It is decidedly so",
                          "Without a doubt",
                          "Yes definitely",
                          "You may rely on it",
                          "As I see it, yes",
                          "Most likely",
                          "Outlook good",
                          "Yes",
                          "Signs point to yes"
                        };
  int nbrAffirm = (sizeof(affirmative) / sizeof(affirmative[0]));
  char * imgAffirmative[] = { "/8ball_yes_1.png",
                              "/8ball_yes_2.png",
                              "/8ball_yes_3.png"
                            };
  int nbrImgAffirmative = (sizeof(imgAffirmative) / sizeof(imgAffirmative[0]));

  char * unclear[] =    { "Reply hazy, try again",
                          "Ask again later",
                          "Better not tell you now",
                          "Cannot predict now",
                          "Concentrate and ask again"
                        };
  int nbrUnclear = (sizeof(unclear) / sizeof(unclear[0]));
  char * imgUnclear[] =     { "/8ball_unclear_1.png",
                              "/8ball_unclear_2.png",
                              "/8ball_unclear_3.png"
                            };
  int nbrImgUnclear = (sizeof(imgUnclear) / sizeof(imgUnclear[0]));

  char * negative[] =   { "Don't count on it",
                          "My reply is no",
                          "My sources say no",
                          "Outlook not so good",
                          "Very doubtful"
                        };
  int nbrNegative = (sizeof(negative) / sizeof(negative[0]));
  char * imgNegative[] =     { "/8ball_no_1.png",
                              "/8ball_no_2.png",
                              "/8ball_no_3.png"
                            };
  int nbrImgNegative = (sizeof(imgNegative) / sizeof(imgNegative[0]));

  if(answerType>-1)
  {
    // erase the old text
    tft.fillRect(0,55,tft.width(),185,TFT_BLACK);
    xpos = 110;
    ypos = 100;
    int16_t rc;

    if(answerType == 0)
    {
      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(TFT_RED);
      tft.setFreeFont(ROBOTO22);
      if(answer==-1)
      {
        answer = random(nbrAffirm);
      }
      tft.drawString(affirmative[answer], tft.width()/2, 70, GFXFF);
      rc = png.open(imgAffirmative[random(nbrImgAffirmative)], pngOpen, pngClose, pngRead, pngSeek, pngDraw);

      lastPageDisplayMillis = millis(); // reset page display timer
    }
    else if (answerType == 1)
    {
      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(TFT_RED);
      tft.setFreeFont(ROBOTO22);
      if(answer==-1)
      {
        answer = random(nbrUnclear);
      }
      tft.drawString(unclear[answer], tft.width()/2, 70, GFXFF);
      rc = png.open(imgUnclear[random(nbrImgUnclear)], pngOpen, pngClose, pngRead, pngSeek, pngDraw);

      lastPageDisplayMillis = millis(); // reset page display timer
    }
    else if (answerType == 2)
    {
      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(TFT_RED);
      tft.setFreeFont(ROBOTO22);
      if(answer == -1)
      {
        answer = random(nbrNegative);
      }
      tft.drawString(negative[answer], tft.width()/2, 70, GFXFF);
      rc = png.open(imgNegative[random(nbrImgNegative)], pngOpen, pngClose, pngRead, pngSeek, pngDraw);

      lastPageDisplayMillis = millis(); // reset page display timer
    }
    // draw the PNG we opened in the switch above
    if (rc == PNG_SUCCESS) 
    {
      tft.startWrite();
      //Serial.printf("image specs: (%d x %d), %d bpp, pixel type: %d\n", png.getWidth(), png.getHeight(), png.getBpp(), png.getPixelType());
      if (png.getWidth() > MAX_IMAGE_WIDTH) {
        Serial.println("Image too wide for allocated line buffer size!");
      }
      else {
        rc = png.decode(NULL, 0);
        png.close();
      }
      tft.endWrite();        
    }
    millisLast8BallAsk = millis();
  }

  if (answerType==-1 && answer == -1)
  {
    // if we get here, we aren't displaying an answer just telling them to ask
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_SILVER);
    tft.setFreeFont(ROBOTO22);
    tft.drawString("Think of your question",tft.width()/2, 80,GFXFF);
    tft.drawString("& tap on the screen!",tft.width()/2,100,GFXFF);
    lastPageDisplayMillis = millis(); // reset page display timer
  }
  last8BallAnswerType = answerType;
  last8BallAnswer = answer;
}

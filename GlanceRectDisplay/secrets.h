// Replace with your network details
const char* SSID = "<SSID>";
const char* SSID_PASSWORD = "<WIFI PWD>";
const char* BACKUP_SSID = "<BACKUP SSID>";
const char* BACKUP_SSID_PASSWORD = "<BACKUP WIFI PWD>";
const char* THOUGHT_USERID = "<USER ID>";     // https://www.stands4.com/services/v2/quotes.php?uid=<user ID>&tokenid=<token>>&format=json  << you will need a free account on www.stands4.com to call this endpoint
const char* THOUGHT_TOKEN = "<TOKEN>";   

/*
  event icon image files for SPIFFS:
  celebration_100px.png
  couple_dancing_100px.png
  fall_100px.png
  fireworks_100px.png
  halloween_100px.png
  hearts_100px.png
  menora_100px.png
  spring_100px.png
  summer_100px.png
  us_flag_100px.png
  winter_100px.png
  xmastree_100px.png
*/

event events[] = {
                    {
                      .eventName = "Halloween!!!",
                      .month = 10,
                      .day = 31,
                      .startYear = 0,
                      .textColor = TFT_ORANGE,
                      .iconFilename = "/halloween_100px.png"
                    },
                    {
                      .eventName = "Christmas!!!",
                      .month = 12,
                      .day = 25,
                      .startYear = 0,
                      .textColor = TFT_GREEN,
                      .iconFilename = "/xmastree_100px.png"
                    },
                    {
                      .eventName = "My Birthday",
                      .month = 7,
                      .day = 21,
                      .startYear = 1968,
                      .textColor = TFT_YELLOW,
                      .iconFilename = "/celebration_100px.png"
                    },
                    {
                      .eventName = "First Day of Fall",
                      .month = 9,
                      .day = 22,
                      .startYear = 0,
                      .textColor = TFT_ORANGE,
                      .iconFilename = "/fall_100px.png"
                    },
                    {
                      .eventName = "First Day of Winter",
                      .month = 12,
                      .day = 22,
                      .startYear = 0,
                      .textColor = TFT_SKYBLUE,
                      .iconFilename = "/winter_100px.png"
                    }
                  };


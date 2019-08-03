#include <U8glib.h>
#include <ThreeWire.h>
#include <RtcDS1302.h>
#include <Servo.h>

U8GLIB_SSD1306_128X64 u8g(U8G_I2C_OPT_NONE);
ThreeWire myWire(12,13,11); // IO, SCLK, CE
RtcDS1302<ThreeWire> Rtc(myWire);
Servo myservo;

int blinkCounter = 0;
int oldDay = 0;

#define RESOLVE_COUNT 4
#define RESOLVE_ITER 2
#define RESOLVE_TIMEOUT 5
const int reserveTimes[RESOLVE_COUNT] = {8, 12, 16, 20}; // 9am, 9pm
bool feeded[RESOLVE_COUNT][RESOLVE_ITER];

void setTime() {
  RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
  if (!Rtc.IsDateTimeValid())
  {
      // Common Causes:
      //    1) first time you ran and the device wasn't running yet
      //    2) the battery on the device is low or even missing

      Serial.println("RTC lost confidence in the DateTime!");
      Rtc.SetDateTime(compiled);
  }

  if (Rtc.GetIsWriteProtected())
  {
      Serial.println("RTC was write protected, enabling writing now");
      Rtc.SetIsWriteProtected(false);
  }

  if (!Rtc.GetIsRunning())
  {
      Serial.println("RTC was not actively running, starting now");
      Rtc.SetIsRunning(true);
  }

  RtcDateTime now = Rtc.GetDateTime();
  if (now < compiled)
  {
      Serial.println("RTC is older than compile time!  (Updating DateTime)");
      Rtc.SetDateTime(compiled);
  }
}

void initFeed(bool isFirst) {
  RtcDateTime toCheck = Rtc.GetDateTime();
  for (int i = 0; i < RESOLVE_COUNT; i++) {
    for (int j = 0; j < RESOLVE_ITER; j++) {
      // if it's first time, need to check time is passed or not
      feeded[i][j] = isFirst && (reserveTimes[i] <= toCheck.Hour() && (RESOLVE_TIMEOUT * j) <= toCheck.Minute());
    }
  }
}

void setup()
{
  Serial.begin(115200);
  Serial.print("compiled: ");
  Serial.print(__DATE__);
  Serial.println(__TIME__);

  myservo.attach(9);
  myservo.write(180);

  Rtc.Begin();
  oldDay = Rtc.GetDateTime().Day();
  setTime();
  initFeed(true);
}

void printLCD(const RtcDateTime& dt)
{
    char datestring[20];
    char timestring[20];
    char reservestring[20];
    sprintf(datestring, "%04u/%02u/%02u",
            dt.Year(),
            dt.Month(),
            dt.Day());
    sprintf(timestring, "%02u:%02u",
            dt.Hour(),
            dt.Minute());

    u8g.firstPage();
    do {
      // title
      u8g.setFont(u8g_font_8x13B);
      u8g.setPrintPos(88,10);
      u8g.print("Feedy");

      // date
      u8g.setFont(u8g_font_unifont);
      u8g.setPrintPos(0,10);
      u8g.print(datestring);
      u8g.drawLine(0,13,128,13);

      // time
      u8g.setFont(u8g_font_helvB14);
      u8g.setPrintPos(0,30);
      u8g.print(timestring);
      u8g.drawLine(0,33,128,33);

      // remain time for next feed
      u8g.setFont(u8g_font_6x12);
      u8g.setPrintPos(57,22);
      u8g.print("Next feed in");

      // iter times
      for (int i = 0; i < RESOLVE_ITER; i++) {
        u8g.setPrintPos(0, 43 + (10 * (i + 1)));
        sprintf(reservestring, "%02um", RESOLVE_TIMEOUT * i);
        u8g.print(reservestring);
      }

      // resolve times
      bool nextFeed = true;
      for (int i = 0; i < RESOLVE_COUNT; i++) {
        u8g.setPrintPos(30 + (25 * i), 43);
        sprintf(reservestring, "%02uh", reserveTimes[i]);
        u8g.print(reservestring);

        // feed status
        for (int j = 0; j < RESOLVE_ITER; j++) {
          int xPos = 39 + (25 * i);
          int yPos = 39 + (10 * (j + 1));
          if (feeded[i][j]) {
            u8g.drawFilledEllipse(xPos, yPos, 4, 4, U8G_DRAW_ALL);
          } else {
            if (nextFeed) {
              // update remain time
              RtcDateTime newDt(dt.Year(), dt.Month(), dt.Day(), reserveTimes[i], RESOLVE_TIMEOUT * j, 0);
              newDt -= dt;
              u8g.setPrintPos(81,31);
              sprintf(reservestring, "%02u:%02u:%02u", newDt.Hour(), newDt.Minute(), newDt.Second());
              u8g.print(reservestring);

              if (blinkCounter > 4)
                u8g.drawCircle(xPos, yPos, 4);
            } else {
              u8g.drawCircle(xPos, yPos, 4);
            }
            nextFeed = false;
          }
        }
      }

    } while(u8g.nextPage());

    blinkCounter++;
    if (blinkCounter == 9)
      blinkCounter = 0;
}

void feed() {
  myservo.write(0);
  delay(1000);
  myservo.write(180);
  delay(1000);
}

void loop()
{
  RtcDateTime now = Rtc.GetDateTime();

  // reset feed when day changed
  if (oldDay != now.Day()) {
    initFeed(false);
    oldDay = now.Day();
  }

  // print to LCD
  printLCD(now);

  // check feed time and do
  for (int i = 0; i < RESOLVE_COUNT; i++) {
    if (now.Hour() == reserveTimes[i]) {
      for (int j = 0; j < RESOLVE_ITER; j++) {
        if (!feeded[i][j] && (now.Minute() >= (RESOLVE_TIMEOUT * j))) {
          feeded[i][j] = true;
          feed();
        }
      }
    }
  }
}

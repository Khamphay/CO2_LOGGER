#include <Arduino.h>
// #include <Adafruit_SCD30.h>
#include <Wire.h>
#include <TimeLib.h>
#include "SparkFun_SCD30_Arduino_Library.h"
#include <FS.h>
#include <SD_MMC.h>
#define XPOWERS_CHIP_AXP2101
#include "XPowersLib.h"
// #include "utilities.h"
#include <time.h>

#define TINY_GSM_RX_BUFFER 1024
#define TINY_GSM_MODEM_SIM7080
#include <TinyGsmClient.h>

#define SDMMC_CMD (39)
#define SDMMC_CLK (38)
#define SDMMC_DATA (40)

SCD30 scd;
File file, fileConfig;
XPowersPMU PMU;
TinyGsm modem(Serial1);

const char *config = "/config.txt";
const char *fileName = "/data_logger.txt";
const char *header = "Date,Time,CO2,AirHumidity,Temperature";

unsigned char val[6];
unsigned long lastTime = 0;
unsigned long interval = 60000; // save data every 1 minute or 60 second (defualt)
unsigned long lastUpdateDonf = 0;
unsigned long updateConfInterval = 300000; // update config date every 5 minute
unsigned char stt;

bool DEBUG = true;

int year2 = 2024;
int month2 = 04;
int day2 = 03;
int hour2 = 00;
int min2 = 00;
int sec2 = 00;

bool gps = false;

unsigned char conv2digit()
{
  /**
   * Convert the ASCII Code of serial monitor to numberic
   *Example: If user enter 56 to the serial:
   * The ASCII code of 5 is 53.
   * The ASCII Code of 6 is 54.
   * Convert ASCII code of 5 and 6: (53-48)*10 + (54-48) = 50+6 = 56
   */
  return (Serial.read() - 48) * 10 + Serial.read() - 48;
}

void initSDCard()
{
  if (SD_MMC.begin("/sdcard", true))
    SD_MMC.end();

  SD_MMC.setPins(SDMMC_CLK, SDMMC_CMD, SDMMC_DATA); // set sdcard pin use 1-bit mode
  while (!SD_MMC.begin("/sdcard", true))
  {
    if (DEBUG)
      Serial.println("Card Mount Failed...");

    delay(1000);
  }
  uint8_t cardType = SD_MMC.cardType();
  if (cardType == CARD_NONE)
  {
    if (DEBUG)
      Serial.println("No SD_MMC card attached");

    while (1)
    {
      delay(1000);
    }
  };
}

void setConfig()
{
  fileConfig = SD_MMC.open(config, FILE_WRITE, true);
  if (fileConfig)
  {
    fileConfig.println(String(year()) + "\n" + String(month()) + "\n" + String(day()));
    fileConfig.println(String(hour()) + "\n" + String(minute()) + "\n" + String(second()));
  }
  fileConfig.close();
}

void readConfig()
{
  fileConfig = SD_MMC.open(config, FILE_READ, false);
  if (fileConfig)
  {
    int line = 0;
    char curr;
    String val = "";
    while (line < 6 && fileConfig.available())
    {
      curr = fileConfig.read();
      if (curr == '\n')
      {
        if (line == 0)
          year2 = atoi(val.c_str());
        else if (line == 1)
          month2 = atoi(val.c_str());
        else if (line == 2)
          day2 = atoi(val.c_str());
        else if (line == 3)
          hour2 = atoi(val.c_str());
        else if (line == 4)
          min2 = atoi(val.c_str());
        else if (line == 5)
          sec2 = atoi(val.c_str());

        line++;
        val = "";
      }
      else
      {
        val += curr;
      }
    }
  }
  fileConfig.close();
}

void writeHaeder()
{
  int check = -1;
  file = SD_MMC.open(fileName, FILE_READ);
  if (file)
  {
    char val[4];
    int index = 0;
    while (index < 4 && file.available())
    {
      val[index] = file.read();
      index++;
    }
    check = strcmp(val, "Date");
  }
  file.close();
  if (check == 0)
    return;

  delay(500);
  file = SD_MMC.open(fileName, FILE_WRITE, true);
  if (file)
  {
    file.println(header);
  }
  else if (DEBUG)
  {
    Serial.println("Can't write data to SDCard.");
  }

  file.close();
}

void writeData(String data)
{
  file = SD_MMC.open(fileName, FILE_APPEND, false);
  if (file)
  {
    file.println(data);
  }
  else
  {
    file.close();
    if (DEBUG)
      Serial.println("Can't write data to SDCard.");

    // Re-initial SD Card
    initSDCard();
    delay(200);

    // Re-write data
    file = SD_MMC.open(fileName, FILE_APPEND, false);
    if (file)
    {
      file.println(data);
    }
    file.close();
  }
}

void readData()
{
  file = SD_MMC.open(fileName, FILE_READ);
  if (file)
  {
    while (file.available())
    {
      Serial.write(file.read());
    }
  }
  file.close();
}

void setting()
{
  if (Serial.available())
  {
    stt = Serial.read();
    switch (stt)
    {
    case 'b':
      DEBUG = true;
      Serial.printf("Debugging is: %s\n", DEBUG ? "ON" : "OFF");
      break;
    case 'c':
      // Serial.print("Type cY for sure? or cN for not?");
      if (Serial.read() == 'Y')
      {
        file = SD_MMC.open(fileName, FILE_WRITE, true);
        if (file)
        {
          file.println(header);
        }
        file.close();
        Serial.println("Clear all done!.");
      }

      break;
    case 'd':
      Serial.printf("%d-%d-%d %d:%d:%d \n", day(), month(), year(), hour(), minute(), second());
      break;
    case 't':
      // TODO: Convert the ASCII value of the Serial to numeric (reading two digits of each loop)
      for (unsigned char i = 0; i < 6; i++)
      {
        val[i] = conv2digit();
      }
      // setTime(hh, mm, ss, DD, MM, YY);
      setTime(val[3], val[4], val[5], val[0], val[1], val[2]);
      delay(500);
      setConfig();
      Serial.printf("%d-%d-%d %d:%d:%d \n", day(), month(), year(), hour(), minute(), second());
      break;
    case 'i':
      switch (Serial.read())
      {
      case 'm':
        interval = conv2digit() * 60000;
        Serial.printf("Set interval as %d mn\n", (interval / 60000));
        break;
      case 'h':
        interval = conv2digit() * 60 * 60000;
        Serial.printf("Set interval as %d h\n", interval / 60000);
        break;
      default:
        break;
      }
      break;
    case 'p':
      Serial.println("_______Menu______");
      Serial.println("*b Debugging");
      Serial.println("*r For read data");
      Serial.println("*cx For clear all data, Type cY for sure? or cN for not?");
      Serial.println("*d Show the date and time");
      Serial.println("*tddMMyyHHmmss Set the date and time");
      Serial.println("*imxx Set the interval for measures as minutes");
      Serial.println("*ihxx Set the interval for measures as hours");
      Serial.println("*p Display for menu");
      break;
    case 'r':
      readData();
      break;
    default:
      Serial.println("Enter p display for menu.");
      break;
    }
  }
}

void setup()
{
  Serial.begin(9600);

  /*********************************
   * step 1 : Start scd30
   ***********************************/
  Wire.begin(43, 44, 10000U);
  while (!scd.begin(Wire))
  {
    if (DEBUG)
      Serial.println("Air sensor not detected. Please check wiring. Freezing...");

    delay(3000);
  }
  Serial.println("SCD30 initailized!");

  // scd.setMeasurementInterval(30); // set interval for delay the measurement (30 second)
  scd.setTemperatureOffset(2.5); // set offset 2.5°C
  delay(300);

  /********************************
   * step 2 : Start sd card
   ***********************************/

  // Set the working voltage of the sdcard, please do not modify the parameters
  PMU.setALDO3Voltage(3300); // SD Card VDD 3300
  PMU.enableALDO3();
  // TS Pin detection must be disable, otherwise it cannot be charged
  PMU.disableTSPinMeasure();

  initSDCard();

  if (DEBUG)
    Serial.println("Initialed Card!");

  /********************************
   * step 3 : Set datetime and header file
   ***********************************/
  readConfig();
  delay(500);
  setTime(hour2, min2, sec2, day2, month2, year2); // set defualt date time

  writeHaeder();

  DEBUG = false;
  delay(3000);
}

void loop()
{

  // Check data time and update
  // setDateTime();
  unsigned long now = millis();
  // Delay
  if (now - lastTime >= interval)
  {
    lastTime = now;
    if (scd.dataAvailable())
    {
      String data = String(day()) + "-" + String(month()) + "-" + String(year()) + ",";
      data += String(hour()) + ":" + String(minute()) + ":" + String(second()) + " " + (isAM() ? "AM," : "PM,");
      data += String(scd.getCO2()) + "," + String(scd.getHumidity()) + "," + String(scd.getTemperature());
      if (DEBUG)
        Serial.println("Writing data...");

      writeData(data);
      if (DEBUG)
        Serial.println("Writ completed.");

      data.clear();
    }
  }

  // Update config datetime every 5 minutes
  if (now - lastUpdateDonf >= updateConfInterval)
  {
    setConfig();
    lastUpdateDonf = now;
  }

  setting();
}

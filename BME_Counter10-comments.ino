/*This is the program for the Arduino mega. This program is connected to the bme280,
 *  Ultimate gps logger shield and the Counter shield
 *  This program logs 
 * 
 * 
 * 
 */
 #include <SoftwareSerial.h>vv
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <TimeLib.h>
#include <Adafruit_GPS.h> // tiny gps
#include <Adafruit_BME280.h>

#define FILENAME "wwhs%02d%02d%04d.txt"  //file name with a formatted string %d will be repaced with integers

//SoftwareSerial GPSSerial(8, 7); // RX, TX

#define GPSSerial Serial1  //GPS  object initalized
Adafruit_GPS GPS(&GPSSerial);//Adafruit_GPS  object initalized  

// Sensors
Adafruit_BME280 bme; // I2C for barometric sensor    Initialization of BME sensor

// SD card
const int SD_CS = 10;

File myFile;  //file object created for storing files in sd card
char filename[17];  //filename string created with 17 characters limit
int log_time = 60; // default is 1 minute         //logtime initialised

// General Variables
signed long count = 0;       //counter

boolean done = false;
unsigned long last_time = 0;       ///last logtime
unsigned long timeout = 0;

int counterPin = 9;

unsigned long lastlog = 0;


void initCounter() {
  pinMode(counterPin, OUTPUT);  //counter pin inistialised as output pin
  digitalWrite(counterPin, HIGH);//// logic 1 send to counter pin
  SPI.begin(); //////Arduino SPI initialised
  digitalWrite(counterPin, LOW); //bring ss low to select and begin SPI conversation
  SPI.transfer(0x00); //write serial data to the MDR0 register
  SPI.transfer(0x00); //write serial data to the MDR1 register
  digitalWrite(counterPin, HIGH);
  digitalWrite(counterPin, LOW);//////LOW to send another command
  SPI.transfer(0x20); //clears the CNTR
  digitalWrite(counterPin, HIGH); //bring high to end conversation
}

long readCounter() {
  unsigned int count_1, count_2, count_3, count_4;       //counts variables initialised
  long count_value = 0;
  digitalWrite(counterPin, LOW); // Begin SPI conversation
  SPI.transfer(0x60); // Request count
  count_1 = SPI.transfer(0x00); // Read highest order byte
  count_2 = SPI.transfer(0x00);
  count_3 = SPI.transfer(0x00);
  count_4 = SPI.transfer(0x00); // Read lowest order byte

  digitalWrite(counterPin, HIGH); // Terminate SPI conversation
  count_value = (count_1 << 8) + count_2;  //// left shifting count 1 by 8 bits and adding count_2 in it( it is same as doing x*2^y)
  count_value = (count_value << 8) + count_3; // left shifting count_value by 8 bits and adding count_3 in it
  count_value = (count_value << 8) + count_4; // left shifting count_value by 8 bits and adding count_4 in it
  return count_value;
}



void setup() {
  Serial.begin(115200);
  initCounter();  //initializing counter by calling initCounter Function

  // Initialize the GPS first to get time
  initGPS(); //initializing GPS by calling initGPS function

  //  boolean ok = false;
  //  do {
  //    ok = getLogTime();
  //    delay(2000);
  //  } while (!ok);

  // Check for a fix (do not block)
  pinMode(SD_CS, OUTPUT); //Adafruit SD shields and modules: pin 10

  /* check on all components */
  if (!SD.begin(SD_CS)) {  //check if no SD card or SDCard sheild found then do nothing
    Serial.println("SD!");
    //while (1);
  }

  if (!bme.begin()) {//check if no BME  found then do nothing
    Serial.println("BME!");
    //while (1);
  }

  last_time = now();   ///save now time to last_time variable
  Serial.println("OK");
}

void initGPS() {
  GPS.begin(9600);////Start serial communication with GPS @ baud rate 9600

  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);////////GPS initailzed to get only RMCGGA commands
  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCONLY);

  // Set the update rate
  GPS.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ); // 1 Hz update rate
  GPS.sendCommand(PGCMD_ANTENNA);  ////Intializing GPS internal and External antenna
}

void updateTime() {
  setTime(GPS.hour, GPS.minute, GPS.seconds, GPS.day, GPS.month, GPS.year);/////Setting time using GPS time
  //adjustTime(-5 * 3600); // UTC -5
}

void checkGPS() {
  if (GPSSerial.available()) {///If GPS serial device is connected and in working state the proceed further

    char c = GPS.read();////   startReading GPS data character by character
    String lastNMEA = "";

    if (GPS.newNMEAreceived()) {

      lastNMEA = GPS.lastNMEA(); //////store recieved data in lastNMEA string

      if (!GPS.parse(GPS.lastNMEA()))
        return;

      if (!lastNMEA.startsWith("\n$GPRMC"))  ////checking for raw data if data don't contain GPRMC it just discard the raw thing
        lastNMEA = "";
      else
        lastNMEA.trim();//removes "&" signs from GPS data
    }

    // if a fix was detected then send the NMEA sentence
    if (GPS.fix) {
      updateTime();///update the time

      if (millis() - lastlog > log_time || lastlog == 0) {/////time calculation for logs
        lastlog = millis();
        if (lastNMEA != "") {
          Serial.print("#NMEA="); Serial.println(lastNMEA);
        }
      }

    }
  }
}

void loop() {

  checkGPS();//calling checkGPS() function

  if (timeStatus() != timeNotSet) {
    if (isAM()) {///////call isAM function to check whether it is AM or PM, if it is AM then proceed to next statement
      if (hour() == 12 && !done) {
        myFile.close();
        sprintf(filename, FILENAME, day(), month(), year());//// in formatted filename string add day,month and year.
        myFile = SD.open(filename, FILE_WRITE);///open that file for writing data
        Log2Card();/////////wrtie log to that file
        done = true;///mark the done flag
      }

      else {
        done = false; ///mark done flag as false if Arduino wasn't able to communicate with SD Card.
      }
    }

    if (now() - last_time >= log_time) {//if statement for checking whether time has chenged after last logged data
      last_time = now();
      sprintf(filename, FILENAME, day(), month(), year());////////////if some time has passed, again insert day, month and year in the formatted string
      myFile = SD.open(filename, FILE_WRITE);
      Log2Card();///////again log the data to sd card
    }
  }
  update_log_time();////////update log time for future use

}

// write to file
void Log2Card() {//////a function to Log data to SD Card *** change to year month day
  String data = "{\"log\":\"";
  // Add date
  data += String(year());
  data += "";
  data += String(month());
  data += "";
  data += String(day());
  data += ",";
  //

  // Add time ***take out :
  data += String(hour());
  data += "";
  data += String(minute());
  data += "";
  data += String(second());
  data += ",";

  // Add counter
  count = readCounter(); //calls subroutine to reac the counter
  data += count;
  data += ",";

  // Add temperature
  data += String(bme.readTemperature());///////get temperature from bme
  data += ",";

  // Add pressure
  data += String(bme.readPressure() / 100.0F);
  data += ",";

  // Add humidity
  data += String(bme.readHumidity());

  data += "\"}";

  Serial.println(data); // Send data to Raspberry Pi
  myFile.println(data); // Log to file

  myFile.flush();
  //write date time, year, temperature, pressure and humidity to sd card.
  myFile.close();
  digitalWrite(counterPin, LOW);///Low to make it ready for another command
SPI.transfer(0x20);///clears the counter
digitalWrite(counterPin, HIGH);///ends the communication
}

boolean update_log_time() {///updates the log time and print it in serial monitor
  boolean result = false;
  if (Serial.available()) {
    char c = Serial.read();
    if (c == '$') {///////check if first charcter from GPS is "&", this ensures the data is valid and it's not a junk string or character
      int val = Serial.parseInt();//////// Get from GPS and converts it to int
      if (val > 0) {
        if (val != log_time) {
          log_time = val;
          Serial.println(log_time);
          Serial.println("{\"log_time\":\"done\"}");
        }
        result = true;
      }
    }
  }
  return result;
}

boolean getLogTime() {/////////calculates the log time with a difference of 2 seconds(2000ms) between each process
  unsigned long timeout = millis();
  boolean ret = false;
  Serial.println("#whatLogTime");
  while (!Serial.available()) {
    if (millis() - timeout > 2000) {////////differnce with 2 seconds 
      return ret;
    }
  }
  ret = update_log_time();////calls udate function for log time
  return ret;
}

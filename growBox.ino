#include <ESP8266WiFi.h>
#include <TimeLib.h>
#include <TimeAlarms.h>
#include <WiFiUdp.h>

const char* ssid = "SSID HERE";
const char* password = "PASSWORD HERE";

int lightCycleHours;
int waterDelay;
int waterTime;
int startTimeHour;
int startTimeMinute;
int endTimeHour;
int endTimeMinute;
int lightOnID;
int lightOffID;
int waterCycleID;

bool dst = false; //false = no, true = yes

WiFiServer server(80);

unsigned int localPort = 2390;      // local port to listen for UDP packets

/* Don't hardwire the IP address or we won't get the benefits of the pool.
 *  Lookup the IP address for the host name instead */
//IPAddress timeServer(129, 6, 15, 28); // time.nist.gov NTP server
IPAddress timeServerIP; // time.nist.gov NTP server address
const char* ntpServerName = "time.nist.gov";

const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message

byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets

// A UDP instance to let us send and receive packets over UDP
WiFiUDP udp;

AlarmId id;

void setup() {
  Serial.begin(115200);
  delay(10);

  // Connect to WiFi network
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  // Start the server
  server.begin();
  Serial.println("Server started");

  // Print the IP address
  Serial.print("Use this URL to connect: ");
  Serial.print("http://");
  Serial.print(WiFi.localIP());
  Serial.println("/");

  // Start UDP
  Serial.println("Starting UDP");
  udp.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(udp.localPort());

  //Request Time from NTP
  NtpRequest();

}

void loop() {

  Alarm.delay(1000);
  Serial.print(".");
  //Serial.println(hour());
  //Serial.println(minute());

  // Check if a client has connected
  WiFiClient client = server.available();
  if (!client) {
    return;
  }

  // Wait until the client sends some data
  Serial.println("*");
  Serial.println("Wake Up! We have Data incoming!!");
  while(!client.available()){
    Alarm.delay(1000);
  }
  digitalClockDisplay();
  Serial.println("dst value: " + String(dst));
  // Read the first line of the request
  String request = client.readStringUntil('\r');
  //fix string (remove GET text)
  request.remove(0,5);
  Serial.println(request);
  client.flush();

  // Match the request
  readRequest(request);

  // set end times
  calcEndTime();

  // Sync Time Again with NTP
  NtpRequest();

  // clear any current alarms and set  new alarms
  ClearAlarms();

  // Make HTML Page for next request:
  // Return the response
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println(""); //  do not forget this one
  client.println("<!DOCTYPE HTML>");
  client.println("<html>");
  client.println("<h1>Growbox Project</h1>");

  // Output Current Settings
  client.println("<b>Current Settings</b><br/>");
  if (startTimeHour){
    client.print("Timer Start Time: " + String(startTimeHour));
    client.print(":");
    if(startTimeMinute < 10){
      client.print('0');
    }
    client.print(String(startTimeMinute) + " <br/>");
  }
  if (lightCycleHours){ client.println("Light Cycle: " + String(lightCycleHours) + " hours<br/>");}
  if (waterDelay){ client.println("Watering Interval: " + String(waterDelay) +" hours<br/>");}
  if (waterTime){ client.println("Watering Time " + String(waterTime) + " minutes<br/>");}
  if (dst){client.println("Daylight Saving is in effect.");}

  // HTML Form
  client.println("<form  method='get' action=''>");
  client.println("<label for='t1'>Turn On Lights At (##:##) </label>");
  client.println("<input id='t1' name='t1' type='text' maxlength='10' value=''/> ");
  client.println("<br/>");
  client.println("<label for='e1'>Length of Light On Cycle (Hours) </label>");
  client.println("<input id='e1' name='e1' type='text' maxlength='4' value=''/> ");
  client.println("<br/>");
  client.println("<label for='e2'>Watering Time </label>");
  client.println("<input id='e2' name='e2' type='text' maxlength='4' value=''/> ");
  client.println("<br/>");
  client.println("<label for='e3'>Delay Between Water Cycles </label>");
  client.println("<input id='e3' name='e3' type='text' maxlength='4' value=''/> ");
  client.println("<br/>");
  client.println("<label for='e4'>Daylight Savings Time </label>");
  client.println("<input id='e4' name='e4' type='checkbox' value='true'/> ");
  client.println("<br/>");
  client.println("<input type='submit' value='Submit' />  ");
  client.println("</form>");

  Alarm.delay(1);
  //Serial.println("Client disconnected");
  Serial.println("");

}

/*
-----------------------------
-- Calc Functions --
-----------------------------
*/

void calcEndTime(){

  Serial.println("----END TIME --");
  //set the end hour based on the start hour and length
  endTimeHour = startTimeHour + lightCycleHours;

  // if hour is greater than 23 reduce by 24
  if (endTimeHour > 23){
    endTimeHour -= 24;
  }

  //set the end minute to the same as the start minute
  endTimeMinute = startTimeMinute;

}

/*
-----------------------------
-- Alarm Setting Functions --
-----------------------------
*/

void SetAlarms(){

  if ((startTimeHour != NULL) && (waterDelay != NULL) && (waterTime != NULL)){
    // Set Light On Alarm
    Alarm.alarmRepeat(startTimeHour,startTimeMinute,0, lightOn);

    // Set Light Off Alarm
    Alarm.alarmRepeat(endTimeHour,endTimeMinute,0, lightOff);

    // To Do -- If light is off but time is between on and off time then turn lights on
    if (startTimeHour < endTimeHour){
      if ((hour() >= startTimeHour) && (minute() >= startTimeMinute) &&
        (hour() <= endTimeHour) && (minute() < endTimeMinute)){
        Serial.println(" In range, turning lights on --");
        // -- TURN ON LIGHT RELAY
      }
    } else if (startTimeHour > endTimeHour) {
      if ( ((hour() >= startTimeHour) && (minute() >= startTimeMinute)) ||
        ((hour() <= endTimeHour) && (minute() < endTimeMinute)) ){
        Serial.println("In range, turning the lights on --");
        // -- TURN ON LIGHT RELAY
      }
    }

    // Set Water Cycle Alarm and start immediately
    Alarm.timerOnce(1, waterCycle);
    Alarm.timerRepeat(waterDelay * 3600, waterCycle); //seconds
    Serial.print("Alarms have been set -- ");
    digitalClockDisplay();

    //Output Alarms to Serial
    Serial.println("Light cycle length in hours: " + String(lightCycleHours));
    Serial.print("Start Time: ");
    printDisplayTime(startTimeHour,startTimeMinute);
    Serial.print("End Time: ");
    printDisplayTime(endTimeHour,endTimeMinute);
    Serial.println("Delay Between Water Cycles: " + String(waterDelay) + " hours");
    Serial.println("Watering Time: " + String(waterTime) + " minutes");

    //Output Alarms info
    Serial.print("Turn on lights at ");
    printDisplayTime(startTimeHour,startTimeMinute);
    printDisplayTime(endTimeHour,endTimeMinute);

    Serial.println("Length of cycle: " + String(lightCycleHours));
    Serial.println("Water cycle length: " + String(waterDelay) + " hours");
    Serial.println("Run water for " + String(waterTime) + " minutes");
  } else{
    Serial.println("Not enough info to create alarms.");
  }
}

void ClearAlarms(){
  if (lightOnID != NULL) {Alarm.free(lightOnID);}
  if (lightOffID != NULL) {Alarm.free(lightOffID);}
  if (waterCycleID != NULL) {Alarm.free(waterCycleID);}
  Serial.print("Clearing all Alarm IDs -- ");

  //Print Current Time
  digitalClockDisplay();

  // Reset new alarms
  SetAlarms();
}

void lightOn(){
  Serial.print("Turning on lights for " + String(lightCycleHours) + " hours. -- ");
  // -- TURN ON LIGHT RELAY

  //set ID to variable
  lightOnID = Alarm.getTriggeredAlarmId();

  //Print Current Time
  digitalClockDisplay();
}

void lightOff(){
  Serial.print("Turning off lights for " + String(24 - lightCycleHours) + " hours. -- ");
  // -- TURN OFF LIGHT RELAY

  //set ID to variable
  lightOffID = Alarm.getTriggeredAlarmId();

  //Print Current Time
  digitalClockDisplay();
}

void waterCycle(){
  //turn water on
  Serial.print("Turn on Water for " + String(waterTime) + " minutes and repeat every " + String(waterDelay) + " hours. -- ");
  // -- TURN ON WATER RELAY

  //set ID to variable
  waterCycleID = Alarm.getTriggeredAlarmId();

  // Set Timer to Turn off pump
  Alarm.timerOnce(waterTime * 60, waterOff);  //number of seconds

  //Print Current Time
  digitalClockDisplay();
}

void waterOff(){
  //turn water off
  Serial.println("Turning the water off -- ");

  //Print Current Time
  digitalClockDisplay();

  // use Alarm.free() to disable a timer and recycle its memory.
  Alarm.free(id);
  // optional, but safest to "forget" the ID after memory recycled
  id = dtINVALID_ALARM_ID;

}

/*
-----------------------------
-- Display Functions --
-----------------------------
*/

void digitalClockDisplay()
{
  // digital clock display of the time
  Serial.print(hour());
  printDigits(minute());
  printDigits(second());
  Serial.println();
}

void printDigits(int digits)
{
  Serial.print(":");
  if(digits < 10)
    Serial.print('0');
  Serial.print(digits);
}

void printDisplayTime(int hours, int minutes){
  Serial.print(hours);
  Serial.print(":");
  if(minutes < 10)
    Serial.print('0');
  Serial.print(minutes);
  Serial.println();
}

/*
--------------------------------
-- Read Web Request Functions --
--------------------------------
*/

void readRequest(String message){

  String variableName = "";
  String variableValue = "";
  bool readingName = false;

  for(int i = 0; i < message.length()-9;i++){
    // do somthing with each chr
    char currentCharacter = message[i];
    if (i == 0){
      // the first letter will always be the name
      readingName = true;
      //variableName += currentCharacter;
    } else if(currentCharacter == '&'){
      // starting the next variable
      readingName = true;

      // if there is a name recorded
      if (variableName != ""){
        // Write variables
        recordVariablesFromWeb(variableName, variableValue);
        // Reset variables for possible next in string 'message'
        variableName = "";
        variableValue = "";
      }
    } else if (currentCharacter == '='){
      // now reading the value
      readingName = false;
    } else {
      if(readingName == true){
        // add the current letter to the name
        variableName += currentCharacter;
      } else{
        // add the current letter to the value
        variableValue += currentCharacter;
      }
    }

  }
  //record variable
  recordVariablesFromWeb(variableName, variableValue);
}

void recordVariablesFromWeb(String variableName, String variableValue){
  // set values to their particular variables in this section
  // parse Variables to the Proper Variable
  //--//Serial.println(variableName);
  //--//Serial.println(variableValue);

  // variable t1
  if(variableName == "t1"){
    String hourString = "";
    String minuteString = "";
    variableValue.replace("%3A",":");

    int timeSection = 0; // 0 = hour, 1 = minute
    for (int i = 0; i < variableValue.length(); i++){
      char currentCharacter = variableValue[i];
      if (currentCharacter == ':'){
        timeSection++;
      } else{
        if (timeSection == 0){
          hourString += currentCharacter;
        } else if (timeSection == 1){
          minuteString += currentCharacter;
        }
      }
    }
    // set start times
    startTimeHour = hourString.toInt();
    startTimeMinute = minuteString.toInt();




  }
  // variable e1
  if(variableName == "e1"){
    lightCycleHours = variableValue.toInt();
  }
  // variable e2
  else if(variableName == "e2"){
    waterTime = variableValue.toInt();
  }
  // variable e3
  else if(variableName == "e3"){
    waterDelay = variableValue.toInt();
  }
  // variable e4
  else if(variableName == "e4"){
    Serial.println(variableValue);
    if(variableValue == "true"){
      dst = true;
      Serial.println("Setting daylight saving to true");
      }
  }
  // Add more else if's if needed,else...dont
}

void NtpRequest()
{
  //get a random server from the pool
  WiFi.hostByName(ntpServerName, timeServerIP);

  sendNTPpacket(timeServerIP); // send an NTP packet to a time server
  // wait to see if a reply is available
  delay(1000);

  int cb = udp.parsePacket();
  if (!cb) {
    Serial.println("no packet yet");
  }
  else {
    //--//Serial.print("packet received, length=");
    //--//Serial.println(cb);
    // We've received a packet, read the data from it
    udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer

    //the timestamp starts at byte 40 of the received packet and is four bytes,
    // or two words, long. First, esxtract the two words:

    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
    // combine the four bytes (two words) into a long integer
    // this is NTP time (seconds since Jan 1 1900):
    unsigned long secsSince1900 = highWord << 16 | lowWord;
    //--//Serial.print("Seconds since Jan 1 1900 = " );
    //--//Serial.println(secsSince1900);

    // now convert NTP time into everyday time:
    //--//Serial.print("Unix time = ");
    // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
    const unsigned long seventyYears = 2208988800UL;
    // subtract seventy years:
    unsigned long epoch = secsSince1900 - seventyYears;
    // print Unix time:
    //--//Serial.println(epoch);

    // print the hour, minute and second:
    Serial.print("The UTC time is ");       // UTC is the time at Greenwich Meridian (GMT)
    Serial.print((epoch  % 86400L) / 3600); // print the hour (86400 equals secs per day)
    Serial.print(':');
    if ( ((epoch % 3600) / 60) < 10 ) {
      // In the first 10 minutes of each hour, we'll want a leading '0'
      Serial.print('0');
    }
    Serial.print((epoch  % 3600) / 60); // print the minute (3600 equals secs per minute)
    Serial.print(':');
    if ( (epoch % 60) < 10 ) {
      // In the first 10 seconds of each minute, we'll want a leading '0'
      Serial.print('0');
    }
    Serial.println(epoch % 60); // print the second

    // set system time and adjust for time zone
    if (dst){
      setTime(epoch - 25200); //7 Hour Delay DST
    } else{
      setTime(epoch - 28800); //8 Hour Delay DST
    }

    Serial.print("The local time is: ");
    digitalClockDisplay();
  }

  // wait ten seconds before asking for the time again
  //delay(10000);
}

// send an NTP request to the time server at the given address
unsigned long sendNTPpacket(IPAddress& address)
{
  Serial.println("sending NTP packet...");
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  udp.beginPacket(address, 123); //NTP requests are to port 123
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
}
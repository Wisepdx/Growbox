#include <ESP8266WiFi.h>
#include <TimeLib.h>
#include <TimeAlarms.h>
#include <WiFiUdp.h>
#include <DHT.h>

const char* ssid = "NETWORK";
const char* password = "PASSWORD";

// variables
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
int checkTempID;
int fanTriggerTemp = 90;
float temp;
float humidity;
String logOutput[5];

// defining PINS
#define DHTPIN 5     // pin D1
const int RELAY1 = 16; // pin D0 -- Fan
const int RELAY2 = 4; // pin D2 -- Light
const int RELAY3 = 0; // pin D3 -- Water Pump
const int RELAY4 = 2; // pin D4


// DHT temp/ humidity sensor setup
#define DHTTYPE DHT11   // DHT 11
DHT dht(DHTPIN, DHTTYPE);

bool dst = true; // apply day light savings  | false = no, true = yes

WiFiServer server(80);

unsigned int localPort = 2390; // local port to listen for UDP packets

// NTP
IPAddress timeServerIP; // time.nist.gov NTP server address
const char* ntpServerName = "time.nist.gov";

const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message

byte packetBuffer[ NTP_PACKET_SIZE]; // buffer to hold incoming and outgoing packets

// a UDP instance to let us send and receive packets over UDP
WiFiUDP udp;

AlarmId id;

void setup() {
  Serial.begin(115200);

  // setup relays and default to off position
  pinMode(RELAY1, OUTPUT);
  pinMode(RELAY2, OUTPUT);
  pinMode(RELAY3, OUTPUT);
  pinMode(RELAY4, OUTPUT);
  digitalWrite(RELAY1, HIGH);
  digitalWrite(RELAY2, HIGH);
  digitalWrite(RELAY3, HIGH);
  digitalWrite(RELAY4, HIGH);

  // initialize dht sensor
  dht.begin();

  // some delay...
  delay(10);

  // Connect to WiFi network
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

  // Check if a client has connected
  WiFiClient client = server.available();
  if (!client) {
    Alarm.delay(1000);
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

  // Make HTML Page Response:
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println(""); //  do not forget this one
  client.println("<!DOCTYPE HTML>");
  client.println("<html>");
  client.println("<head><link rel='stylesheet' href='https://maxcdn.bootstrapcdn.com/bootstrap/4.0.0-alpha.6/css/bootstrap.min.css' integrity='sha384-rwoIResjU2yc3z8GV/NPeZWAv56rSmLldC3R/AZzGRnGxQQKnKkoFVhFQhNUwEyJ' crossorigin='anonymous'><script src='https://code.jquery.com/jquery-3.1.1.slim.min.js' integrity='sha384-A7FZj7v+d/sdmMqp/nOQwliLvUsJfDHW+k9Omg/a/EheAdgtzNs3hpfag6Ed950n' crossorigin='anonymous'></script><script src='https://cdnjs.cloudflare.com/ajax/libs/tether/1.4.0/js/tether.min.js' integrity='sha384-DztdAPBWPRXSA/3eYEEUWrWCy7G5KFbe8fFjk5JAIxUYHKkDx6Qin1DkWx51bBrb' crossorigin='anonymous'></script><script src='https://maxcdn.bootstrapcdn.com/bootstrap/4.0.0-alpha.6/js/bootstrap.min.js' integrity='sha384-vBWWzlZJ8ea9aCX4pEW3rVHjgjt7zpkNpZk+02D9phzyeVkE+jo0ieGizqPLForn' crossorigin='anonymous'></script></head>");
  client.println("<body><div class='container'><h1>Growbox Project</h1><ul class='nav nav-tabs' role='tablist'><li class='nav-item'><a class='nav-link active' data-toggle='tab' href='#Current' role='tab'>Current Settings</a></li><li class='nav-item'>");
  client.println("<a class='nav-link' data-toggle='tab' href='#Change' role='tab'>Change Settings</a></li></ul><br/><div class='tab-content'><div class='tab-pane active' id='Current' role='tabpanel'><div class='container'><h3>Current Settings</h3><br/><div class='row'>");
  client.print("<div class='col-12'><p><strong>Page Last Updated:</strong> ");
  client.print(String(hour()) + ":");//##:##
  if (minute() <10){client.print("0");}
  client.print(String(minute()));
  client.print("</p>");
  if (startTimeHour){
    client.print("<p><strong>Timer Start Time:</strong> " + String(startTimeHour));
    client.print(":");
    if(startTimeMinute < 10){
      client.print('0');
    }
    client.print(String(startTimeMinute) + " </p>");
  }
  if (lightCycleHours){ client.println("<p><strong>Length of Light Cycle:</strong> " + String(lightCycleHours) + " hours</p>");}
  if (waterDelay){ client.println("<p><strong>Time Between Waterings:</strong> " + String(waterDelay) +" hours</p>");}
  if (waterTime){ client.println("<p><strong>Watering Length:</strong> " + String(waterTime) + " minutes</p>");}
  if (dst){client.println("<div class='alert alert-info' role='alert'>daylights savings time is currently: active</div>");}
  if (!startTimeHour){client.println("<div class='alert alert-info' role='alert'>There are currently no information available</div>");}
  client.print("</div></div>");
  client.println("</div></div><div class='tab-pane' id='Change' role='tabpanel'><div class='container'><div class='row'><div class='col-12'>");
  client.println("<form method='get' action=''>");
  client.println("<div class='form-group row'><h3> Submit New Settings </h3></div>");
  client.println("<div class='form-group row'><label class='' for='t1'>Turn On Lights At:</label><input class='form-control' id='t1' name='t1' type='text' maxlength='10' value='' placeholder='00:00' /></div>");
  client.println("<div class='form-group row'><label for='e1'>Length of Light On Cycle (Hours) </label><input class='form-control' id='e1' name='e1' type='text' maxlength='4' value='' placeholder='0' /></div>");
  client.println("<div class='form-group row'><label for='e2'>Watering Time </label><input class='form-control' id='e2' name='e2' type='text' maxlength='4' value='' placeholder='0' /></div>");
  client.println("<div class='form-group row'><label for='e3'>Delay Between Water Cycles </label><input class='form-control' id='e3' name='e3' type='text' maxlength='4' value='' placeholder='0' /></div>");
  client.println("<div class='form-group row'><label for='e4'>Turn on Daylight Savings Time </label><input class='form-control' id='e4' name='e4' type='checkbox' value='true' /></div>");
  client.println("<div class='form-group row'><button type='submit' class='btn btn-primary' value='Submit'>Submit</button></div>");
  client.println("</form>");
  client.println("</div></div></div></div></div></div></body>");

  Alarm.delay(1);
  //Serial.println("Client disconnected");
  Serial.println("");

}


/*
-----------------------------
------- DHT Functions -------
-----------------------------
*/

void checkTemp(){
  
  // Checking sensor can be slow
  humidity = dht.readHumidity();
  // Read temperature as Fahrenheit (isFahrenheit = true)
  temp = dht.readTemperature(true);

  // Check if any reads failed and exit early (to try again).
  if (isnan(humidity) || isnan(temp)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  // turn the fan on if needed
  if (temp > fanTriggerTemp){
    // -- FAN RELAY ON
    digitalWrite(RELAY1, LOW);
  } else{
    // -- FAN RELAY OFF
    digitalWrite(RELAY1, HIGH);
  }

  checkTempID = Alarm.getTriggeredAlarmId();
  
  // uncomment if you want constant output
  // outputDebug("Humidity:" + humidity + " %");
  // outputDebug("Temperature (F):" + temp + " *F");
}



/*
-----------------------------
------ Calc Functions -------
-----------------------------
*/

void calcEndTime(){
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
    //Alarm.alarmRepeat(20,10,0, lightOn);

    // Set Light Off Alarm
    Alarm.alarmRepeat(endTimeHour,endTimeMinute,0, lightOff);

    // To Do -- If light is off but time is between on and off time then turn lights on
    if (startTimeHour < endTimeHour){
      if ((hour() >= startTimeHour) && (minute() >= startTimeMinute) &&
        (hour() <= endTimeHour) && (minute() < endTimeMinute)){
        outputDebug("In range, turning lights on");
        // -- TURN ON LIGHT RELAY
        digitalWrite(RELAY2, LOW);
      }
    } else if (startTimeHour > endTimeHour) {
      if ( ((hour() >= startTimeHour) && (minute() >= startTimeMinute)) ||
        ((hour() <= endTimeHour) && (minute() < endTimeMinute)) ){
        outputDebug("In range, turning lights on");
        // -- TURN ON LIGHT RELAY
        digitalWrite(RELAY2, LOW);
      }
    }

    // set water cycle alarm and start immediately
    Alarm.timerOnce(1, waterCycle);
    Alarm.timerRepeat(waterDelay * 3600, waterCycle); //seconds

    // start the temp read and fan check function
    Alarm.timerOnce(1, checkTemp);
    Alarm.timerRepeat(5000, checkTemp);

    // log event to serial
    outputDebug("Alarms have been set");

    // log variables to serial
    Serial.println("Light cycle length in hours: " + String(lightCycleHours));
    Serial.print("Start Time: ");
    printDisplayTime(startTimeHour,startTimeMinute);
    Serial.print("End Time: ");
    printDisplayTime(endTimeHour,endTimeMinute);
    Serial.println("Delay Between Water Cycles: " + String(waterDelay) + " hours");
    Serial.println("Watering Time: " + String(waterTime) + " minutes");

    // log alarms information
    Serial.print("Turn on lights at ");
    printDisplayTime(startTimeHour,startTimeMinute);
    Serial.print("Turn off lights at ");
    printDisplayTime(endTimeHour,endTimeMinute);

    Serial.println("Length of cycle: " + String(lightCycleHours) + " hours");
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
  if (checkTempID != NULL) {Alarm.free(checkTempID);}
  outputDebug("Clearing all Alarm IDs");

  // reset new alarms
  SetAlarms();
}

void lightOn(){
  outputDebug("Turning on lights for " + String(lightCycleHours) + " hours");

  // -- TURN ON LIGHT RELAY
  digitalWrite(RELAY2, LOW);

  // set ID to variable
  lightOnID = Alarm.getTriggeredAlarmId();
}

void lightOff(){
  outputDebug("Turning off lights for " + String(24 - lightCycleHours) + " hours");

  // -- TURN OFF LIGHT RELAY
  digitalWrite(RELAY2, HIGH);

  // set ID to variable
  lightOffID = Alarm.getTriggeredAlarmId();
}

void waterCycle(){
  outputDebug("Turn on Water for " + String(waterTime) + " minutes and repeating every " + String(waterDelay) + " hours");

  // -- TURN ON WATER RELAY
  digitalWrite(RELAY3, LOW);

  // set ID to variable
  waterCycleID = Alarm.getTriggeredAlarmId();

  // Set Timer to Turn off pump
  Alarm.timerOnce(waterTime * 60, waterOff);  //number of seconds
}

void waterOff(){
  // output debug line
  outputDebug("Turning the water off");

  // -- TURN OFF WATER RELAY
  digitalWrite(RELAY3, HIGH);

  // use Alarm.free() to disable a timer and recycle its memory.
  Alarm.free(id);

  // optional, but safest to "forget" the ID after memory recycled
  id = dtINVALID_ALARM_ID;

}

/*
-----------------------------
----- Display Functions -----
-----------------------------
*/

void outputDebug(String debugLine){
  Serial.print(debugLine);
  Serial.print(" at ");
  // current time
  digitalClockDisplay();
}

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
----- Log Output Functions -----
--------------------------------
*/

// write log to array
void writeLogArray(String log){
  
  // if no log slot open then move all logs up one slot and fill last slot
  if (logOutput[4] != NULL){
    for(int i=1;i<5;i++){
      logOutput[i-1] = logOutput[i];
    }
    //fill last slot
    logOutput[4] = log;
  } else{
    //check for an open log slot and print
    for(int i=0;i<5;i++){
      if (logOutput[i] == NULL){
        logOutput[i] = log;
        break;
      }
    }
  }

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
      // variableName += currentCharacter;
    } else if(currentCharacter == '&'){
      // starting the next variable
      readingName = true;

      // if there is a name recorded
      if (variableName != ""){
        // write variables
        recordVariablesFromWeb(variableName, variableValue);
        // reset variables for possible next in string 'message'
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
  // add more else if's if needed, else... don't.
}

void NtpRequest()
{
  // get a random server from the pool
  WiFi.hostByName(ntpServerName, timeServerIP);
  // send an NTP packet to a time server
  sendNTPpacket(timeServerIP);

  // wait to see if a reply is available
  delay(1000);

  int cb = udp.parsePacket();
  if (!cb) {
    Serial.println("no packet yet");
  }
  else {
    // We've received a packet, read the data from it
    udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer
    // the timestamp starts at byte 40 of the received packet and is four bytes,
    // or two words, long. First, esxtract the two words:
    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
    // combine the four bytes (two words) into a long integer
    // this is NTP time (seconds since Jan 1 1900):
    unsigned long secsSince1900 = highWord << 16 | lowWord;
    // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
    const unsigned long seventyYears = 2208988800UL;
    // subtract seventy years:
    unsigned long epoch = secsSince1900 - seventyYears;

    // print the hour, minute and second for UTC:
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
    // print the hour, minute for local time:
    Serial.print("The local time is: ");
    digitalClockDisplay();
  }
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

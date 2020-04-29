/*********
 * Sound Reactive Candle - Version 2.0
 * By James N.
 * added in this version is Wifi capable  
 * you can change the light through the web interface
 * Make sure to enter you SSID and Password of you Home Network
 * before upload sketch to the MCU
 * To access the web interface, from your browser enter the IP of the MCU 
*********/
#include <ESP8266WiFi.h>  // Wifi Lib
#include <Adafruit_NeoPixel.h> //Smart LED Lib

const char* ssid     = "YOUR SSID";  // Enter Your Network ID here
const char* password = "YOUR PASSWORD"; // Enter Your Network password here
int Wifi_HB_Timeout = 60000; // Wifi Connection Timeout in milliseconds
long Wifi_HB_Tmr; // variable to hold current start time
WiFiServer server(80);  // Web Port ID

#define N_PIXELS  6  // Number of pixels you are using
#define MIC_PIN   A0  // Microphone Pin
#define LED_PIN    D7  // NeoPixel LED Pin
#define DC_OFFSET  0  // DC offset in mic signal - if unusure, leave 0
#define NOISE     80  // Noise/hum/interference in mic signal
#define SAMPLES   50  // Length of buffer for dynamic level adjustment
#define TOP       (N_PIXELS +1) // Allow dot to go slightly off scale

int LightMode = 0;  // store light scheme 
int modechangetmr;  // use for Auto Lightmode cycle
int cycleTime = 1000; // mode change every 1 seconds.  set desire time
bool automodechange = false;  // when set to true, light mode will cycle 
int brightness;   // LED Brightness, max = 255
char* ModeToString[]={"Candle Fire","Multi Color","Lite Green","Purple","Lite Orange","Blue","Bright Pink","Greenish","White","Auto Rotation",};
int RGBValue[]={130,60,15,   // Fire - start idx = 0
                0,0,0,       // Multi color - start idx = 3
                80,250,150,  // Lite Green - start idx = 6
                200,50,225,  // Purple - start idx = 9
                250,160,60,   // Orange - start idx = 12
                10,25,230,   // Blue - start idx = 15
                175,20,75,   // Bright Pink - start idx = 18
                170,170,50,  // Green/Yellow - start idx = 21
                255,255,255}; // White - start idx = 24
int RGBIndex = 0; // variable to store the color index in array

byte
  peak      = 0,      // Used for falling dot
  volCount  = 0;      // Frame counter for storing past volume data
  
int
  vol[SAMPLES],       // Collection of prior volume samples
  lvl       = 10,     // Current "dampened" audio level
  minLvlAvg = 0,      // For dynamic adjustment of graph low & high
  maxLvlAvg = 512;

// create LED object
Adafruit_NeoPixel  strip = Adafruit_NeoPixel(N_PIXELS, LED_PIN, NEO_RGB + NEO_KHZ800);

String header;  // Variable to store the HTTP request
unsigned long currentTime = millis(); // current time use in network
unsigned long previousTime = 0;   // previous time
const long timeoutTime = 1000;  // duration for keeping client connection

// function to initiate Wifi connection
void Connect_WiFi()
{
  // Init Wifi to connect to Home AP
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.setOutputPower(100);        // 10dBm == 10mW, 14dBm = 25mW, 17dBm = 50mW, 20dBm = 100mW
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  Serial.print(".");
  }
  Serial.println("");
  Serial.print("WiFi connected and ");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  server.begin();
  Wifi_HB_Tmr = millis(); // start the wifi hearbeat timer
}

void setup() {
  Serial.begin(115200); // set serial comm for debugging
  memset(vol, 0, sizeof(vol));  // define array for mic signal
  strip.begin();  // init LED
  Connect_WiFi(); // init wifi connection
}

// main function
void loop(){
  uint8_t  i;
  uint16_t minLvl, maxLvl;
  int      n, height;
  String req, prevreq;

  // Check for WiFi Status every 60 seconds and initiate connection if needed
  if (millis() - Wifi_HB_Tmr >= Wifi_HB_Timeout){
    if (WiFi.status() != WL_CONNECTED){
      Connect_WiFi();
    }
    else{
      Wifi_HB_Tmr = millis();
//      Serial.println("WiFi Still connected");
    }
  }
  WiFiClient client = server.available();   // Listen for incoming clients
  if (client) {                             // If a new client connects,
    Serial.println("New Client.");          // print a message out in the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    currentTime = millis();
    previousTime = currentTime;
    while (client.connected() && currentTime - previousTime <= timeoutTime) { // loop while the client's connected
      currentTime = millis();         
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        header += c;
        if (c == '\n') {                    // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();

            if (header.indexOf("GET /lightmode") >= 0){
                LightMode += 1;           // Increment color set
              if (LightMode == 9){   // Auto Color Rotation
                strip.setBrightness(250);               
                automodechange = true;    // Enable Auto Color Rotation
                modechangetmr = millis(); // reset the duratio
              } else {
                automodechange = false;   // Disable Auto Color Rotation
                RGBIndex = LightMode * 3; // Starting RGB color index in color array
              }
            }
            else if (header.indexOf("GET /CandleLight") >= 0){  // Candle color
              automodechange = false; 
              LightMode = 0;
              RGBIndex = LightMode * 3; // Starting RGB color index in color array
            }
            // Display the HTML web page
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");
            // style setting for Lightmode buttons 
            client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
            client.println(".button { background-color: #963C14; border: none; color: white; padding: 16px 40px;");
            client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
            client.println(".button2 {background-color: #963C14;}</style></head>"); //77878A
            client.println("<body><h1>Reactive E-Candle</h1>");  // Title of web page
            // Display the 2 button and status of the Lightmode
            client.println("<p>Current Light Mode : " + String(ModeToString[LightMode]) + "</p>");
            client.println("<p><a href=\"/lightmode\"><button class=\"button\">Reactive Color</button></a></p>");
            client.println("<p><a href=\"/CandleLight\"><button class=\"button\"> Candle's Color </button></a></p>");
            client.println("</body></html>");
            // The HTTP response ends with another blank line
            client.println();
            // Break out of the while loop
            break;
          } else { // if you got a newline, then clear currentLine
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      }
    }
    header = ""; // Clear the header variable
    client.stop(); // Close the connection
  }

//   Serial.println(automodechange);
  // Light Mode 9 = Auto Rotate Mode 1-8
   if (automodechange == true && millis()-modechangetmr > cycleTime){
    //Serial.println("Mode Cycling...");
    if(LightMode >= 9) 
      {LightMode = 1;}
    else 
      {LightMode += 1;}

    RGBIndex = LightMode * 3;
    modechangetmr = millis(); 
   }
  /////////////////////////////////////
  
  n   = analogRead(MIC_PIN);                 // Raw reading from mic 
  n   = abs(n - 512 - DC_OFFSET);            // Center on zero
  n   = (n <= NOISE) ? 0 : (n - NOISE);      // Remove noise/hum
  lvl = ((lvl * 7) + n) >> 3;    // "Dampened" reading (else looks twitchy)
//  Serial.println(lvl);
  
  // Calculate bar height based on dynamic min/max levels (fixed point):
  height = TOP * (lvl - minLvlAvg) / (long)(maxLvlAvg - minLvlAvg);

  if(height < 0L)       height = 0;      // Clip output
  else if(height > TOP) height = TOP;
  if(height > peak)     peak   = height; // Keep 'peak' dot at top

  // Color pixels based on rainbow gradient
  for(i=0; i<N_PIXELS; i++) {  
    if((lvl == 0) && (LightMode == 0))
    {
       brightness = random(75,230);
       strip.setBrightness(brightness);               
       strip.setPixelColor(i, random(115, 150), random(55, 60), random(10, 20));  // Candle Fire
    }
    else if((i >= height) && (LightMode != 0))
    {
       strip.setPixelColor(i, 0, 0, 0);
    }
    else { 
       if (LightMode == 1) strip.setPixelColor(i,Wheel(map(i,0,strip.numPixels()-1,30,150)));  // Various Color
       else strip.setPixelColor(i, RGBValue[RGBIndex],RGBValue[RGBIndex+1],RGBValue[RGBIndex+2]);
    }
  } 
  strip.show(); // Update strip

  vol[volCount] = n;                      // Save sample for dynamic leveling
  if(++volCount >= SAMPLES) volCount = 0; // Advance/rollover sample counter

  // Get volume range of prior frames
  minLvl = maxLvl = vol[0];
  for(i=1; i<SAMPLES; i++) {
    if(vol[i] < minLvl)      minLvl = vol[i];
    else if(vol[i] > maxLvl) maxLvl = vol[i];
  }
  // minLvl and maxLvl indicate the volume range over prior frames, used
  // for vertically scaling the output graph (so it looks interesting
  // regardless of volume level).  If they're too close together though
  // (e.g. at very low volume levels) the graph becomes super coarse
  // and 'jumpy'...so keep some minimum distance between them (this
  // also lets the graph go to zero when no sound is playing):
  if((maxLvl - minLvl) < TOP) maxLvl = minLvl + TOP;
  minLvlAvg = (minLvlAvg * 63 + minLvl) >> 6; // Dampen min/max levels
  maxLvlAvg = (maxLvlAvg * 63 + maxLvl) >> 6; // (fake rolling average)
}

// Input a value 0 to 255 to get a color value.
uint32_t Wheel(byte WheelPos) {
  if(WheelPos < 85) {
   return strip.Color(WheelPos * 3, 255 - WheelPos * 3, random(WheelPos * 30,255 - WheelPos * 3));
  } else if(WheelPos < 170) {
   WheelPos -= 85;
   return strip.Color(255 - WheelPos * 3, random(WheelPos * 30,255 - WheelPos * 3), WheelPos * 3);
  } else {
   WheelPos -= 170;
   return strip.Color(random(WheelPos * 30,255 - WheelPos * 3), WheelPos * 3, 255 - WheelPos * 3);
  }
}

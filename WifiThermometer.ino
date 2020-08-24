/*
 *  This sketch is to update a Hubitat virtual thermometer via HTTP using the "Maker API" app
 *
 *  Enter your: 
 *    WIFI details, 
 *    Hubitat IP address, 
 *    device ID of the virtual thermometer
 *    unique key from the maker API for authentication
 *    Number of minutes between updates (Fewer updates = better battery life.  It spends the rest of it's time in deep sleep)
 *    
 *  Since the 8266 always starts from the beginning (reboots) when it comes out of deep sleep, the entire program is in the "setup" section
 *  There's no reason for a "loop" since it's going to reboot each time.  For this reason, remember there will be no variables maintained between runs
 *
 */

#include <ESP8266WiFi.h>

const char* ssid     = "MySSID";
const char* password = "MyPass";

const char* host = "Hubitat local IP";
const char* deviceid   = "device number";
const char* key = "***Insert access token from MakerAPI config***";
const int minutes = 15; 

//Connected DS18B20 as per this project:  https://simple-circuit.com/nodemcu-esp8266-ds18b20-sensor-st7789-display/
//Ignoring the screen, since I didn't use a screen!
//
//The rest of these DS18B20 functions were stolen from the same project:

#define DS18B20_PIN   D2   // DS18B20 data pin is connected to NodeMCU pin D2 (GPIO4)

bool ds18b20_start()
{
  bool ret = 0;
  digitalWrite(DS18B20_PIN, LOW);  // send reset pulse to the DS18B20 sensor
  pinMode(DS18B20_PIN, OUTPUT);
  delayMicroseconds(500);          // wait 500 us
  pinMode(DS18B20_PIN, INPUT);
  delayMicroseconds(100);          // wait to read the DS18B20 sensor response
  if (!digitalRead(DS18B20_PIN))
  {
    ret = 1;                  // DS18B20 sensor is present
    delayMicroseconds(400);   // wait 400 us
  }
  return(ret);
}
 
void ds18b20_write_bit(bool value)
{
  digitalWrite(DS18B20_PIN, LOW);
  pinMode(DS18B20_PIN, OUTPUT);
  delayMicroseconds(2);
  digitalWrite(DS18B20_PIN, value);
  delayMicroseconds(80);
  pinMode(DS18B20_PIN, INPUT);
  delayMicroseconds(2);
}
 
void ds18b20_write_byte(byte value)
{
  byte i;
  for(i = 0; i < 8; i++)
    ds18b20_write_bit(bitRead(value, i));
}
 
bool ds18b20_read_bit(void)
{
  bool value;
  digitalWrite(DS18B20_PIN, LOW);
  pinMode(DS18B20_PIN, OUTPUT);
  delayMicroseconds(2);
  pinMode(DS18B20_PIN, INPUT);
  delayMicroseconds(5);
  value = digitalRead(DS18B20_PIN);
  delayMicroseconds(100);
  return value;
}
 
byte ds18b20_read_byte(void)
{
  byte i, value;
  for(i = 0; i < 8; i++)
    bitWrite(value, i, ds18b20_read_bit());
  return value;
}
 
bool ds18b20_read(int *raw_temp_value)
{
  if (!ds18b20_start())  // send start pulse
    return(0);
  ds18b20_write_byte(0xCC);   // send skip ROM command
  ds18b20_write_byte(0x44);   // send start conversion command
  while(ds18b20_read_byte() == 0);  // wait for conversion complete
  if (!ds18b20_start())             // send start pulse
    return(0);                      // return 0 if error
  ds18b20_write_byte(0xCC);         // send skip ROM command
  ds18b20_write_byte(0xBE);         // send read command
 
  // read temperature LSB byte and store it on raw_temp_value LSB byte
  *raw_temp_value = ds18b20_read_byte();
  // read temperature MSB byte and store it on raw_temp_value MSB byte
  *raw_temp_value |= (unsigned int)(ds18b20_read_byte() << 8);
 
  return(1);  // OK --> return 1
}

//
//
//  End of functions stolen from the thermometer project
//

//This is where the 8266 is going to wake up each time:

void setup() {
  //Dont know if this is a waste of time or power or not..  I figure it might help the thermometer
  //stabilize..  If power turns out to be a problem I'll probably take out all my debug prints too, but 
  //left them in for now!
  
  delay (1000);  
  //
  Serial.begin(115200);
 

  // We start by connecting to a WiFi network  (Stolen from WIFI_CLIENT demo code)

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
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  

 
  // get temperature in °C ( actual temperature in °C = c_temp/16)
  int c_temp;
  float f_temp;
  
  if( ds18b20_read( &c_temp ) ) {  
    // read from DS18B20 sensor OK
 
    // calculate temperature in °F (actual temperature in °F = f_temp/160)
    // °F = °C x 9/5 + 32
    f_temp = (int32_t)c_temp * 90/5 + 5120;  // 5120 = 32 x 16 x 10
    f_temp=f_temp/160;
    Serial.print("Temp is ");
    Serial.println(f_temp);
    } 
    else {
    f_temp = 123.45;  //If it didn't read the sensor correctly, I'll throw in a known out of bounds value so I can detect the failure on Hubitat!
    Serial.println ("Error reading thermometer!");
  } //End of whether thermometer worked or not!

  
  Serial.println ("Now time to connect to Wifi and set it on the Hubitat");
  Serial.print("connecting to ");
  Serial.println(host);
  
  // Use WiFiClient class to create TCP connections
  WiFiClient client;
  const int httpPort = 80;
  if (!client.connect(host, httpPort)) {
    Serial.println("connection failed");
    return;
  }
  
  // We now create a URI for the request
  String url = "/apps/api/7/devices/";
  url += deviceid;
  url += "/setTemperature/";
  url += f_temp;
  url += "?access_token=";
  url += key;
  
  Serial.print("Requesting URL: ");
  Serial.println(url);
  
  // This will send the request to the server
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" + 
               "Connection: close\r\n\r\n");
  delay(100);
  
  // Read all the lines of the reply from server and print them to Serial
  while(client.available()){
    String line = client.readStringUntil('\r');
    Serial.print(line);
  }
  
 
  
  Serial.println();
  Serial.println("closing connection");
  Serial.println("ESP8266 in sleep mode");
  pinMode(DS18B20_PIN, INPUT);
  int sleepTimeS = 60 * minutes;
  ESP.deepSleep(sleepTimeS * 1000000);
}

void loop()
{
  //Nothing Goes here because we go to sleep before we get here!
}

// end of code.

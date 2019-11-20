#include <EEPROM.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <Adafruit_NeoPixel.h>

typedef struct {
  char ssid[35];
  char password[35];
  char host[35];
  char clientId[35];
  char userName[35];
  int httpsPort;
} configData_t;

configData_t cfg;
int UID;
#define MAX_TELNET_CLIENTS 2
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(256, 5, NEO_GRB + NEO_KHZ800);
WiFiClientSecure client;
WiFiServer TelnetServer(23);
WiFiClient TelnetClient[MAX_TELNET_CLIENTS];
bool ConnectionEstablished;

void setup() {
  pixels.begin();
  pixels.setBrightness(10);
  drawTwitch(0,0);
  Serial.begin(115200);
  loadSettings();
  if(cfg.userName == "" || true) { //Später muss das true wohl weg :)
      applyDefaultSettings();
      saveSettings(); //und das wohl rein :)
  }
  Serial.println("");
  Serial.print("connecting to ");
  Serial.println(cfg.ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(cfg.ssid, cfg.password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  TelnetServer.begin();
  TelnetServer.setNoDelay(true);
  
  client.setTimeout(15000);
  client.setInsecure();
  delay(1000);
  getUserId();
}

void getUserId() {
  String url = String("/helix/users?login=")+cfg.userName;
  String line = WebGet(url);
  Serial.println(line);
  StaticJsonDocument<100> doc;
  deserializeJson(doc, line);
  UID=doc["data"][0]["id"];
  Serial.print("UID:");
  Serial.println(UID);
  }

void loop() {
  String url = String("/helix/users/follows?to_id=")+String(UID);
  String r = WebGet(url);
  StaticJsonDocument<200> doc2;
  Serial.println(r);
  deserializeJson(doc2, r);
  String followers=doc2["total"];
  Serial.print("Followers: ");
  drawNumber(followers.toInt());
  Serial.println(followers);
  int wait = 0;
  while (wait < 15) {
    Telnet();
    delay(1000);
    Serial.print(".");
    wait++;
  }
  Serial.println("");
}

String WebGet(String url) {
  int retry = 0;
  while ((!client.connect(cfg.host, cfg.httpsPort)) && (retry < 15)) {
    delay(100);
    Serial.print(".");
    retry++;
  }
  if (retry == 15) {
    Serial.println("Connection failed");
  }
  else {
    Serial.print("requesting URL: ");
    Serial.println(url);
    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + cfg.host + "\r\n" +
               "Client-ID: " + cfg.clientId + "\r\n" +
               "Connection: close\r\n\r\n");
    //Serial.println("request sent");
    String line = "";
    while (client.connected()) {
      String l = client.readStringUntil('\n');
      if (l == "\r") {
        //Serial.println("headers received");
        break;
      }
    }
    line = client.readStringUntil('\n');
    return line;
  }
  return "";
}

void TelnetMsg(String text)
{
  for(int i = 0; i < MAX_TELNET_CLIENTS; i++)
  {
    if (TelnetClient[i] || TelnetClient[i].connected())
    {
      TelnetClient[i].println(text);
    }
  }
  delay(10);  // to avoid strange characters left in buffer
}
      
void Telnet()
{
  // Cleanup disconnected session
  for(int i = 0; i < MAX_TELNET_CLIENTS; i++)
  {
    if (TelnetClient[i] && !TelnetClient[i].connected())
    {
      Serial.print("Client disconnected ... terminate session "); Serial.println(i+1); 
      TelnetClient[i].stop();
    }
  }
  
  // Check new client connections
  if (TelnetServer.hasClient())
  {
    ConnectionEstablished = false; // Set to false
    
    for(int i = 0; i < MAX_TELNET_CLIENTS; i++)
    {
      // Serial.print("Checking telnet session "); Serial.println(i+1);
      
      // find free socket
      if (!TelnetClient[i])
      {
        TelnetClient[i] = TelnetServer.available(); 
        
        Serial.print("New Telnet client connected to session "); Serial.println(i+1);
        
        TelnetClient[i].flush();  // clear input buffer, else you get strange characters
        showSettings();
        ConnectionEstablished = true; 
        
        break;
      }
      else
      {
        // Serial.println("Session is in use");
      }
    }
 
    if (ConnectionEstablished == false)
    {
      Serial.println("No free sessions ... drop connection");
      TelnetServer.available().stop();
      // TelnetMsg("An other user cannot connect ... MAX_TELNET_CLIENTS limit is reached!");
    }
  }
 
  for(int i = 0; i < MAX_TELNET_CLIENTS; i++)
  {
    if (TelnetClient[i] && TelnetClient[i].connected())
    {
      if(TelnetClient[i].available())
      { 
        //get data from the telnet client
        String command="";
        while(TelnetClient[i].available())
        {
          command+=char(TelnetClient[i].read());
        }
        TelnetCommand(command);
      }
    }
  }
}

void TelnetCommand(String c) {
  Serial.println("TelnetCommand: "+c);
  if(c.startsWith("set ")){
    if(c.startsWith("set username=")) {
        String dummy = c.substring(c.indexOf('=')+1,c.length()-1);
        dummy.toCharArray(cfg.userName,dummy.length());
        //char c2[35];
        //dummy.toCharArray(c2, dummy.length());
        //strcpy(cfg.userName, c2);
      }
    showSettings();
    saveSettings();
    getUserId(); // Eigentlich restart
    }
  }
     
void showSettings() {
  TelnetMsg(String(""));
  TelnetMsg(String("CONFIG:"));
  TelnetMsg(String("SSID: ")+cfg.ssid);
  TelnetMsg(String("Passwort: ")+cfg.password);
  TelnetMsg(String("Host: ")+cfg.host);
  TelnetMsg(String("ClientId: ")+cfg.clientId);
  TelnetMsg(String("Port: ")+cfg.httpsPort);
  TelnetMsg(String("Username: ")+cfg.userName);
}

void applyDefaultSettings() {
  strcpy(cfg.ssid,"FritzBoxHarry");
  strcpy(cfg.password,"Harry1960Heidi1965");
  strcpy(cfg.host,"api.twitch.tv");
  strcpy(cfg.clientId,"eurzdb7y4misq0fb47s6u0glmegov3");
  strcpy(cfg.userName,"RealForTN0X");
  cfg.httpsPort = 443;
  }

void saveSettings() {
  EEPROM.begin(512);
  EEPROM.put(0,cfg);
  delay(200);
  EEPROM.commit();
  EEPROM.end();
  }

void loadSettings() {
  EEPROM.begin(512);
  EEPROM.get(0,cfg);
  EEPROM.end();
  }

const uint64_t TWITCH[] = {
  0x040cf781a9a981ff,
  0x0000087e56567e00
};
const uint64_t NUMBER[] = {
  0x1c08080808080c08,
  0x3e0408102020221c,
  0x1c2220201820221c,
  0x20203e2224283020,
  0x1c2220201e02023e,
  0x1c2222221e02221c,
  0x040404081020203e,
  0x1c2222221c22221c,
  0x1c22203c2222221c,
  0x1c2222222222221c
};
const int NUMBER_LEN = sizeof(NUMBER)/8;

void drawTwitch(int x,int y) {
  for (int i = 0; i <= 8; i++) {
    byte row1 = (TWITCH[0] >> i * 8) & 0xFF;
    byte row2 = (TWITCH[1] >> i * 8) & 0xFF;
    for (int j = 0; j <= 8; j++) {
      pixels.setPixelColor(getLED(i+1+x,j+1+y), pixels.Color(bitRead(row1, j)*100,bitRead(row1, j)*65,bitRead(row1, j)*165));
      if(bitRead(row2, j)){
        pixels.setPixelColor(getLED(i+1+x,j+1+y), pixels.Color(255,255,255));
      }
    }
  }
  pixels.show();
}

void drawNumber(int number) {
  int t = number%10000/1000-1;
  int h = number%1000/100-1;
  int z = number%100/10-1;
  int e = number%10-1;
  if(number>9999) {
    t=h=z=e=8;
  }
  if(e!=0) {
    if(z!=0) {
      if(h!=0) {
        if(t!=0) {
          displayImage(NUMBER[t],0,8);
        }
        displayImage(NUMBER[h],0,14);
      }
      displayImage(NUMBER[z],0,20);
    }
    displayImage(NUMBER[e],0,26);
  }
}

void displayImage(uint64_t image,int x,int y) {
  for (int i = 0; i <= 8; i++) {
    byte row = (image >> i * 8) & 0xFF;
    for (int j = 0; j <= 8; j++) {
      pixels.setPixelColor(getLED(i+1+x,j+1+y), pixels.Color(bitRead(row, j)*255,bitRead(row, j)*255,bitRead(row, j)*255));
    }
  }
  pixels.show();
}

int getLED(int x, int y) {
    if(x<1||x>8||y<1||y>32) {
      return -1;
    }
    return (8*(y-1))+((y%2)*x)+(((y-1)%2)*((x*-1)+9))-1;
}

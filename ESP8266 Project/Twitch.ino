#include <EEPROM.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>

typedef struct {
  char ssid[30];
  char password[30];
  char host[30];
  char clientId[30];
  char userName[30];
  int httpsPort;
} configData_t;

configData_t cfg;
int UID;
#define MAX_TELNET_CLIENTS 2
WiFiClientSecure client;
WiFiServer TelnetServer(23);
WiFiClient TelnetClient[MAX_TELNET_CLIENTS];
bool ConnectionEstablished;

void setup() {
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
  String url = String("/helix/users?login=")+String(cfg.userName);
  String line = WebGet(url);
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
               "User-Agent: ESP8266/NodeMCU 3.0\r\n" +
               "Connection: close\r\n\r\n");
    //Serial.println("request sent");
    while (client.connected()) {
    String l = client.readStringUntil('\n');
      if (l == "\r") {
        //Serial.println("headers received");
        break;
      }
    }
    String line = client.readStringUntil('\n');
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
        char* c2;
        dummy.toCharArray(c2,30);
        strcpy(cfg.userName,c2);
      }
    showSettings();
    saveSettings();
    getUserId(); // Eigentlich restart
    }
  }
     
void showSettings() {
  TelnetMsg(String("SSID: ")+cfg.ssid);
  TelnetMsg(String("Passwort: ")+cfg.password);
  TelnetMsg(String("Host: ")+cfg.host);
  TelnetMsg(String("ClientId: ")+cfg.clientId);
  TelnetMsg(String("Port: ")+cfg.httpsPort);
  TelnetMsg(String("Username: ")+cfg.userName);
}

void applyDefaultSettings() {
  strcpy(cfg.ssid,"FritzBoxHarry");
  strcpy(cfg.password,"Passwort");
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

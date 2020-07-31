#include <FS.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic

#include <ArduinoJson.h>

const int ButtonPin = 4;
const int GreenLed =  12;
const int BlueLed = 13;
const int RedLed = 14;

int reading;
int state = LOW;
int buttonstate;
long milliseconds = 0;
long debounce = 200;
int length;


WiFiClientSecure client;

char WEB_SERVER[100];
char WEB_URI[100];
char WEB_TOKEN[50];

//flag for saving data
bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void setup(void) {
  Serial.begin ( 115200 );

  pinMode(GreenLed, OUTPUT);
  pinMode(BlueLed, OUTPUT);
  pinMode(RedLed, OUTPUT);
  pinMode(ButtonPin, INPUT);
  
  Serial.println("mounting FS...");
  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");

          strcpy(WEB_SERVER, json["WEB_SERVER"]);
          strcpy(WEB_URI, json["WEB_URI"]);
          strcpy(WEB_TOKEN, json["WEB_TOKEN"]);
        } else {
          Serial.println("failed to load json config");
        }
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }

  WiFiManagerParameter custom_web_server("server", "Webserver name/ip", WEB_SERVER, 100);
  WiFiManagerParameter custom_web_uri("uri", "URI /test.php", WEB_URI, 100);
  WiFiManagerParameter custom_web_token("token", "Token", WEB_TOKEN, 50);

  WiFiManager wifiManager;
  //reset saved settings
  //wifiManager.resetSettings();

  wifiManager.setSaveConfigCallback(saveConfigCallback);

  wifiManager.addParameter(&custom_web_server);
  wifiManager.addParameter(&custom_web_uri);
  wifiManager.addParameter(&custom_web_token);
  
  String hostname = "IT-BUTTON-" + String(ESP.getChipId());
  WiFi.hostname("IT-BUTTON-" + String(ESP.getChipId()));
  
  String ssid = "IT-BUTTON-" + String(ESP.getChipId());
  if(!wifiManager.autoConnect(ssid.c_str())) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }

  //read updated parameters
  strcpy(WEB_SERVER, custom_web_server.getValue());
  strcpy(WEB_URI, custom_web_uri.getValue());
  strcpy(WEB_TOKEN, custom_web_token.getValue());
  Serial.print("Webserver: ");
  Serial.println(custom_web_server.getValue());
  Serial.print("Web URI: ");
  Serial.println(custom_web_uri.getValue());
  Serial.print("Web Token: ");
  Serial.println(custom_web_token.getValue());
  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["WEB_SERVER"] = WEB_SERVER;
    json["WEB_URI"] = WEB_URI;
    json["WEB_TOKEN"] = WEB_TOKEN;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
  }

  Serial.print ( "Connected to your network" );
  Serial.print ( "IP address: " );
  Serial.println ( WiFi.localIP() );

  if(strlen(WEB_SERVER) == 0 || strlen(WEB_URI) == 0 || strlen(WEB_TOKEN) == 0) {
    Serial.print("Config Faulty, Kicking config");
    SPIFFS.format();
    wifiManager.resetSettings();
    delay(2000);
    ESP.reset();
  }

  ToggleLed(digitalRead(ButtonPin));
}



void loop() {
  // read the state of the pushbutton value:
  reading = digitalRead(ButtonPin);

  if(millis() != milliseconds) {
    if(reading != buttonstate) {
      if (reading == HIGH) {
        state = HIGH;
        
        UpdateServer(state);
        ToggleLed(state);
        buttonstate = HIGH;
      }
      if (reading == LOW) {
        state = LOW;
        
        UpdateServer(state);
        ToggleLed(state);
        buttonstate = LOW;
      }
      milliseconds = millis();
    }
  }
}

void ToggleLed(int state) {
  if(state == LOW) {
    digitalWrite(GreenLed, HIGH);
    digitalWrite(RedLed, LOW);
    Serial.println("Toggled to free");
  } else {
    digitalWrite(RedLed, HIGH);
    digitalWrite(GreenLed, LOW);
    Serial.println("Toggled to busy");
  }
}

void UpdateServer(int state) {
  String Message = "token="+String(WEB_TOKEN)+"&state="+state;
 
  length = Message.length();
  client.setInsecure();
  if (client.connect(String(WEB_SERVER), 443)) {
    Serial.println("Sending message…");
    client.println("POST " + String(WEB_URI) + " HTTP/1.1");
    client.println("Host: " + String(WEB_SERVER));
    client.println("Connection: close\r\nContent-Type: application/x-www-form-urlencoded");
    client.print("Content-Length: ");
    client.print(length);
    client.println("\r\n");
    client.print(Message);

    client.stop();
    Serial.println("Web Update Done");
    Serial.println("");
    delay(100);
  } else {
    Serial.println("Connection error");
  }
}

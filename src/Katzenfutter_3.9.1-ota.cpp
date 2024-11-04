#include <Arduino.h>
#include <ESP8266WiFi.h>

#include <Ticker.h>
#include <AsyncMqttClient.h>
#include <ArduinoOTA.h>

#define LEDON  HIGH
#define LEDOFF LOW
#define LED_ROT     D8  
#define LED_GRUEN   D7
#define LED_BLAU    D4

#define SERVOPIN D6 

#define EINMALGEWICHT 5

#define NO_MESSE


#ifdef MESSE
  #define WIFI_SSID "SP-X6"
  #define WIFI_PASSWORD "709iXO!Y6V4q2Z-T0YcMUK"
  #define MQTT_HOST IPAddress(192, 168, 0, 102)
  #define MQTT_PORT 1883
  #define MQTTUSER "ipc"
  #define MQTTPASSWD "24fair!-5"
#endif

#ifndef MESSE
  #define WIFI_SSID "phantomias2"
  #define WIFI_PASSWORD "A3oOzWxxZ"
  #define MQTT_HOST IPAddress(192, 168, 33, 58)
  #define MQTT_PORT 1883
  #define MQTTUSER ""
  #define MQTTPASSWD ""
#endif








#define OTAHOSTNAME "Presse01"
#define OTAPASSWORT "OTAPresse01"

#define TOPIC "werk01/halle01/presse01"
#define DEVICENAME "presse01"

#define BEHAELTERGEWICHT 0

#define PRG_VERSION "3.95"

#include <HX711.h>

const int LOADCELL_DOUT_PIN = D2; // D2
const int LOADCELL_SCK_PIN = D1;  // D1

#define TOUCH_PIN D5

unsigned long startMillis;  //some global variables available anywhere in the program
unsigned long currentMillis;
const unsigned long period = 5000;

HX711 scale;

int debuglevel =0;
int maxpressure= 700;

String Kommando ="";
String MyMACadress ="";
String datetime ="";
char   tmpbuffer[255];
String MYweight = "";
int    myTara = 1371;
String weight;
float  f_weight ;
float  f_gewicht=0;
   
int touchVal = 0;

bool abbruch = false;

#include <Servo.h>
Servo myservo;  // create servo object to control a servo


AsyncMqttClient mqttClient;
Ticker mqttReconnectTimer;

WiFiEventHandler wifiConnectHandler;
WiFiEventHandler wifiDisconnectHandler;
Ticker wifiReconnectTimer;


void LED_light(int red, int green, int blue);
void connectToMqtt() ;
void LED_Gewicht(int gew );
float wiegen(bool forceMessage);


void OTA_Setup() {
  Serial.println("OTA Setup");
  ArduinoOTA.setPort(8266);       // Port defaults to 8266
  ArduinoOTA.setHostname(OTAHOSTNAME);
  ArduinoOTA.setPassword(OTAPASSWORT);

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
    
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
}


void connectToWifi() {
  Serial.println("Connecting to Wi-Fi...");
  
  WiFi.hostname(OTAHOSTNAME);
  WiFi.mode(WIFI_STA);

  WiFi.setAutoReconnect(false);
  
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void onWifiConnect(const WiFiEventStationModeGotIP& event) {
  Serial.println("Connected to Wi-Fi.");

  LED_light(0,0,0);
  // OTA_Setup(); // NEU 
  connectToMqtt();
}

void handleWiFiConnectivity(){

  Serial.println( F("\nHandeling WiFi Connectivity") );
  // if( !WiFi.localIP().isSet() || !WiFi.isConnected() ){
  if(  !WiFi.isConnected() ){
    Serial.println( F("Handeling WiFi Reconnect Manually.") );
    WiFi.reconnect();
  }else{
    Serial.print(F("IP address: "));
    Serial.println(WiFi.localIP());
  }
}

void onWifiDisconnect(const WiFiEventStationModeDisconnected& event) {
  Serial.println("Disconnected from Wi-Fi.");
  mqttReconnectTimer.detach(); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
  wifiReconnectTimer.once(2, connectToWifi);
}

void connectToMqtt() {
  Serial.println("Connecting to MQTT...");
  mqttClient.setKeepAlive(5).setCleanSession(false).setWill(TOPIC"/online" , 2, true, "no").setClientId(DEVICENAME);
  mqttClient.setCredentials(MQTTUSER, MQTTPASSWD);
  mqttClient.connect();


 
  
}

void onMqttConnect(bool sessionPresent) {
  Serial.println("Connected to MQTT.");
  Serial.print("Session present: ");
  Serial.println(sessionPresent);
//  uint16_t packetIdSub = mqttClient.subscribe("test/lol", 2);
//  Serial.print("Subscribing at QoS 2, packetId: ");
//  Serial.println(packetIdSub);
//  mqttClient.publish("test/lol", 0, true, "test 1");
//  Serial.println("Publishing at QoS 0");
//  uint16_t packetIdPub1 = mqttClient.publish("test/lol", 1, true, "test 2");
//  Serial.print("Publishing at QoS 1, packetId: ");
//  Serial.println(packetIdPub1);
//  uint16_t packetIdPub2 = mqttClient.publish("test/lol", 2, true, "test 3");
//  Serial.print("Publishing at QoS 2, packetId: ");
//  Serial.println(packetIdPub2);
//


  // Baue Konfig-TOPIC für subscribe
  String ConfigTopic = "";
  // ohne IP ConfigTopic += MyMACadress;  ConfigTopic.replace(" ","0"); 
  ConfigTopic = TOPIC"/";  ConfigTopic += "kommando/#"; 

  ConfigTopic.toCharArray(tmpbuffer,255);
  uint16_t packetIdSub = mqttClient.subscribe(tmpbuffer, 2);
  Serial.print("ich subscribe :: "); Serial.println(tmpbuffer);

  mqttClient.subscribe("datetime", 2);

  // mqttClient.publish(TOPIC"/status", 0, true, "online");

  WiFi.localIP().toString().toCharArray(tmpbuffer,255);
  mqttClient.publish(TOPIC"/ip", 1, true, tmpbuffer);

  mqttClient.publish(TOPIC"/online", 2, true, "yes");
  mqttClient.publish(TOPIC"/version",  2,  true, PRG_VERSION);

  sprintf(tmpbuffer,"%i",(int) maxpressure);  
  mqttClient.publish(TOPIC"/maxpressure",  2,  true, tmpbuffer);
  
  
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.println("Disconnected from MQTT.");

  if (WiFi.isConnected()) {
    mqttReconnectTimer.once(2, connectToMqtt);
  }
}

void onMqttSubscribe(uint16_t packetId, uint8_t qos) {
  Serial.println("Subscribe acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
  Serial.print("  qos: ");
  Serial.println(qos);
}

void onMqttUnsubscribe(uint16_t packetId) {
  Serial.println("Unsubscribe acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
}

void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
  char macAddr[20];
  byte mac[6];    
  String ConfigTopic = "";
  
    WiFi.macAddress(mac);
    // if (debuglevel) 
    // sprintf(macAddr, "%2X:%2X:%2X:%2X:%2X:%2X\0", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    sprintf(macAddr, "%2X:%2X:%2X:%2X:%2X:%2X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    // Serial.println(macAddr);
    MyMACadress = macAddr;

    Serial.print("Receive:");
    Serial.println(topic);
    Serial.println("  Payload:");
    Serial.println(payload);
   
    // payload mit \0 abschchliesen
    strncpy(tmpbuffer,payload,len);
    tmpbuffer[len] = '\0';
    if (debuglevel) {
      Serial.print("Publish received.");
      Serial.print("  topic:");
      Serial.print(topic);
      Serial.print("  payload: ");
      Serial.println(tmpbuffer);
    }
  // Initialsierung durch MAC-Adresse
  ConfigTopic = TOPIC"/";  
  // keine IP Adresse 
  ConfigTopic += macAddr;  
  ConfigTopic.replace(" ","0");
  ConfigTopic += "/artikel"; 
    
  if (ConfigTopic.equals(topic)){
     // MyArtikel = tmpbuffer;
     // war nur zum Test 
     // RFIDs = ""; // Wenn neue Location die akte gelesenen löschen
//     if (debuglevel) {
//        Serial.println("MyArtikel ist:");  Serial.print(MyArtikel);;Serial.print("#"); Serial.print(tmpbuffer);
//     }
  }
  
  ConfigTopic = TOPIC"/"; ConfigTopic += "debuglevel";     
  if (ConfigTopic.equals(topic)){
     debuglevel = atoi(tmpbuffer);
     if (debuglevel) {
        Serial.println("Neuer debuglevel"); Serial.print(debuglevel);;Serial.print("#"); Serial.print(tmpbuffer);
     }
  } 


  ConfigTopic = TOPIC"/"; ConfigTopic += "kommando/maxpressure";     
  Serial.println(ConfigTopic);
  if (ConfigTopic.equals(topic)){
     maxpressure = atoi(tmpbuffer);
     Serial.print("Neuer Maxpressure:"); 
     Serial.println(tmpbuffer);
     sprintf(tmpbuffer,"Meuer Maxpressure %i",(int) maxpressure);Serial.println(tmpbuffer);
     mqttClient.publish(TOPIC"/feedback", 0, true, tmpbuffer);

     sprintf(tmpbuffer,"%i",(int) maxpressure);Serial.println(tmpbuffer);
     mqttClient.publish(TOPIC"/maxpressure", 0, true, tmpbuffer);

  } 

   ConfigTopic = TOPIC"/";  ConfigTopic +="kommando";
  if (ConfigTopic.equals(topic)){
     Kommando = tmpbuffer;
     if (   debuglevel  ) {
      Serial.println(Kommando);
     }
  }
  
  ConfigTopic = TOPIC"/";  ConfigTopic +="led";
  if (ConfigTopic.equals(topic)){
     Kommando = tmpbuffer;
  }
  
  ConfigTopic = "datetime";
  if (ConfigTopic.equals(topic)){
     datetime  = tmpbuffer;
     if (debuglevel) {
        Serial.println("datetime");Serial.print("#"); Serial.print(datetime );
     }
  }


  
  
//  Serial.println("Publish received.");
//  Serial.print("  topic: ");
//  Serial.println(topic);
//  Serial.println(":payload:");
//  Serial.println(tmpbuffer);
//  Serial.print("  qos: ");
//  Serial.println(properties.qos);
//  Serial.print("  dup: ");
//  Serial.println(properties.dup);
//  Serial.print("  retain: ");
//  Serial.println(properties.retain);
//  Serial.print("  len: ");
//  Serial.println(len);
//  Serial.print("  index: ");
//  Serial.println(index);
//  Serial.print("  total: ");
//  Serial.println(total);
}

void onMqttPublish(uint16_t packetId) {
  Serial.println("Publish acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
}

void ICACHE_RAM_ATTR handleInterrupt() {
  Serial.println("Interrupt Detected");
  abbruch = true;
  // nur einmal
  detachInterrupt(digitalPinToInterrupt(TOUCH_PIN));
  LED_light(255,0,0);
  
}

void StempelInit() {
   myservo.attach(SERVOPIN,544,2400);;  // attaches the servo on GPIO2 to the servo object
   
   myservo.write(160);   delay(500);
   myservo.write(180);  delay(500); 
   // myservo.write(0);  
   
   myservo.detach();  // attaches the servo on GPIO2 to the servo object
}

void ruetteln(bool force, float alt, float neu)
{
  float diff;
  diff = alt - neu;
  
  if (f_weight > 100 && !force && diff > 10 ) 
    return;

  mqttClient.publish(TOPIC"/feedback", 0, true, "ruetteln ...");
  
  myservo.attach(SERVOPIN,544,2400);  // attaches the servo on GPIO2 to the servo object
  LED_Gewicht( (int) f_weight);
 
  for (int n =0; n <= 5 ; n++ ) 
  {                                   
    myservo.write(150);   delay(100);
    myservo.write(180);  delay(100);    
  }

  myservo.detach();     
  LED_light(0,0,0);
}

void Stempel()
{
  int pos;
  int Offset;

  float vorher, aktuell;
  
  attachInterrupt(digitalPinToInterrupt(TOUCH_PIN), handleInterrupt, CHANGE);
  
  if (abbruch) return; // falls schon abgebrochen 

  vorher = wiegen(false);
  
 

  myservo.attach(SERVOPIN,544,2400);  // attaches the servo on GPIO2 to the servo object
  LED_Gewicht( (int) f_weight);

  Offset = 10;
  // stempel hinten :: for (pos = 0; pos <= 180; pos += 3) // goes from 0 degrees to 180 degrees
  for (pos = 180; pos >= 60 && !abbruch ; pos -= 3) // goes from 180 degrees to 0 degrees
  {                                   // in steps of 1 degree
    myservo.write(pos);               // tell servo to go to position in variable 'pos'
    delay(50);    
  
  }
  
  for (pos = 60; pos >= 0 && !abbruch ; pos -= 3) // goes from 180 degrees to 0 degrees
  {                                   // in steps of 1 degree
    myservo.write(pos);               // tell servo to go to position in variable 'pos'
    delay(100);    
    wiegen(true);
    sprintf(tmpbuffer,"F_ %i",(int) f_gewicht);Serial.println(tmpbuffer);
    sprintf(tmpbuffer,"M_ %i",(int) maxpressure);Serial.println(tmpbuffer);



    if (f_gewicht >= maxpressure) { 
      abbruch = true;
      mqttClient.publish(TOPIC"/feedback", 0, true, "MAX Pressure erreicht");
      Serial.println("_________ABRUCH___MAX_Pressure_________");
    }

  }
  delay(1000);
  mqttClient.publish(TOPIC"/feedback", 0, true, "Pause ENDE");

  abbruch = false;
  // stempel hinten ::for (pos = 180; pos >= 0; pos -= 3) // goes from 180 degrees to 0 degrees
  // Neu von aktueller Position zurück, nicht von 0
  for (; pos <= 180 && !abbruch  ; pos += 3)
  {
    myservo.write(pos);
        // tell servo to go to position in variable 'pos'
    delay(100);  // 50                       // waits 15ms for the servo to reach the position
    
  }

  if ( abbruch)   { 
    myservo.write(180); delay(500); 
    mqttClient.publish(TOPIC"/feedback", 0, true, "Stempel ABBRUCH");
  } else {
    mqttClient.publish(TOPIC"/feedback", 0, true, "Stempel ENDE");
  }
  
  myservo.detach();  // attaches the servo on GPIO2 to the servo object
   
  while(digitalRead(TOUCH_PIN)== HIGH) { // Warten bis der Finger weg ist 
    Serial.print(":.");
    delay(10);
  }
  LED_light(0,0,0);

  aktuell = wiegen(false);
  ruetteln( false , vorher, aktuell );  
}

void warte( unsigned long wtime){
  unsigned long starttime;
  
  starttime = millis();


  while ((millis() - starttime ) <=  wtime ){
          // nix tun 
           yield();       
  }
}

void getRGB(String request) {
    int goOn = 1;
    int count = -1;
    int pos1;
    int pos2;
    String params[5];

    request.replace("rgb(","");
    request.replace(")",",");
    request.length();

    while( goOn == 1 ) {
        pos1 = request.lastIndexOf(",", pos2);
        pos2 = request.lastIndexOf(",", pos1 - 1);

        if( pos2 <= 0 ) goOn = 0;

        String tmp = request.substring(pos2 + 1, pos1);

        count++;
        params[count] = tmp;
        Serial.print(count) ; Serial.print(":") ;Serial.println( params[count]);
        if( goOn != 1) break;
    }
    LED_light(params[2].toInt(),params[1].toInt(),params[0].toInt() );
}


void LED_test() {
//  for(int n=0; n< 100;n++) {
//    LED_light(random(0,255),random(0,255),random(0,255) );
//    delay(10);
//  }
    LED_light(255,0,0); delay(200);
    LED_light(255,255,0);delay(200);
    LED_light(000,255,0);delay(200);
    LED_light(000,000,000);

}

void LED_Gewicht(int gew ) {
   if (gew < 200) {
            LED_light(255,0,0);
       } else if (gew < 400)  {
            LED_light(255,255,0);
       } else  {
            LED_light(000,255,0);
       }
}

void LED_light(int red, int green, int blue) {
  analogWrite(LED_ROT, red);
  analogWrite(LED_GRUEN, green);
  analogWrite(LED_BLAU, blue);
}

void check_gewicht(float alt, float neu, float normal) {
  String Msg;
  float diff;

  diff = alt - neu;
  Serial.print("Differenz:" );Serial.println( diff );
  Serial.print("Werte:" ); Serial.print(neu);Serial.print(":" );Serial.println(alt );

  Msg = String(diff);
  // diff.toString().toCharArray(tmpbuffer,255);
  Msg.toCharArray(tmpbuffer,255);
  mqttClient.publish(TOPIC"/delta", 0, true, tmpbuffer );

  Msg = datetime + "::" + String(diff );
  Msg.toCharArray(tmpbuffer,255);
  mqttClient.publish(TOPIC"/lastfeed", 0, true, tmpbuffer );
   
  if ( diff < normal  ) {  // 100 ist kritische Menge: kurz vor leer
    // Meldung
    Msg = "{ \"timestamp\":\"";
    Msg  +=  datetime + "\", ";
    Msg  +=  "\"message\":\"FUTTERABGABE GESTOERRT ";  
    Msg  += String(diff);
    Msg  += "\"}";
    Msg.toCharArray(tmpbuffer,255);

    mqttClient.publish(TOPIC"/alarmmsg", 0, true,tmpbuffer );
    mqttClient.publish(TOPIC"/alarm", 0, true, "FUTTERABGABE GESTOERRT");
  }
  
}

float wiegen(bool forceMessage) {

    // f_weight hat das alte Gewicht
    if (scale.is_ready()) {           // Waage    
      // Serial.print("a) ..... ");
      weight = String(scale.get_units(3), 1);
      // Serial.println("b)");
      // Serial.println(weight);

      f_gewicht = weight.toFloat() * 1000 - myTara ;
      if  ( forceMessage ||  ( f_gewicht < (f_weight -3) ) || ( f_gewicht > (f_weight + 3)) ) {
        forceMessage = true;
      }
       
      sprintf(tmpbuffer,"%i",(int) f_gewicht);
 
      if (forceMessage) {
        mqttClient.publish(TOPIC"/weight", 0 ,  false, tmpbuffer); // war 2  
      }
       startMillis = currentMillis;
       if (true) {
           LED_Gewicht( (int) f_gewicht);
       }
    }
    return f_gewicht;
}

void setup() {
  Serial.begin(115200);
  while (! Serial);
  
  Serial.println();

  pinMode(LED_ROT, OUTPUT);
  pinMode(LED_GRUEN, OUTPUT);
  pinMode(LED_BLAU, OUTPUT);
  
  pinMode(TOUCH_PIN, INPUT);

  LED_test();
  LED_light(0,0,0);

  wifiConnectHandler = WiFi.onStationModeGotIP(onWifiConnect);
  wifiDisconnectHandler = WiFi.onStationModeDisconnected(onWifiDisconnect);

  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onSubscribe(onMqttSubscribe);
  mqttClient.onUnsubscribe(onMqttUnsubscribe);
  mqttClient.onMessage(onMqttMessage);
  mqttClient.onPublish(onMqttPublish);
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);

  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  
    Serial.println("Waage ..");
    scale.set_scale( 557047. / 1.410 ) ;  // 557047. / 1.245
    scale.tare();
    // scale.set_offset(520.0);
    myTara = BEHAELTERGEWICHT;

    currentMillis = millis();
    startMillis = currentMillis;

    StempelInit();
    OTA_Setup();
    connectToWifi();
}
 



void loop() {
   unsigned long dauer = 0 ;
   float aktuell; // Gewicht vor/nach Stempel move
   float vorher;

   if (!WiFi.isConnected() && !mqttClient.connected()  ) {
//    Serial.println(WiFi.status());      
      LED_light(255,0,0);  
      return;
   }
   ArduinoOTA.handle();
   
   if (!mqttClient.connected()) {
       Serial.println(WiFi.localIP());
       //  handleWiFiConnectivity();
      LED_light(255,255,0);     warte(100);
      LED_light(0,255,0);       warte(100);  

      if (digitalRead(TOUCH_PIN)== HIGH) {
        LED_light(255,0,0);       warte(1000);  
        ESP.restart();
      }
       
      return;
   }
   
   LED_light(0,0,0);

    if ( Kommando.indexOf("rgb") >=0 ) {
      Kommando="";
      getRGB(Kommando);
      warte(5000);
    }
    if ( ( Kommando.equals("reset")) ) {
        for ( int n=0; n < 100; n++) {
          LED_light(255,0,0); warte(50);  
          LED_light(0,0,0);warte(50);
        }
        ESP.restart(); 
    }
    
    if ( ( Kommando.equals("tara")) ) {
       Kommando = "";
       scale.tare();
       StempelInit();
    }
    if ( ( Kommando.equals("ruetteln")) ) {
       Kommando = "";
       ruetteln( true,0,0 ); 
    }
    
    if ( Kommando.equals("feed")) {
      mqttClient.publish(TOPIC"/feedback", 0, true, "feed received"); 
      mqttClient.publish(TOPIC"/kommando", 0, true, "received");
      Kommando = "";
      abbruch = false;
      vorher = wiegen(false);
       for (int n=0; n<=1 && ! abbruch ;n++){ // war zwei 2
           Stempel();  
       }
       aktuell = wiegen(true);
       check_gewicht( vorher, aktuell , EINMALGEWICHT);
      
      mqttClient.publish(TOPIC"/feedback", 0, true, "feed-ENDE"); // war 2   
    }
    
    if ( Kommando.equals("move")) {
        mqttClient.publish(TOPIC"/kommando", 0, true, "received");
        mqttClient.publish(TOPIC"/feedback", 0, true, "move received");
        Kommando = "";
        abbruch = false;
        
        vorher = wiegen(false);
        Stempel();  
        aktuell = wiegen(true);
        check_gewicht( vorher, aktuell , EINMALGEWICHT);   
           
        mqttClient.publish(TOPIC"/feedback", 0, true, "mode ENDE");
    }

    currentMillis = millis();
    
    if ( currentMillis - startMillis >= period ) {
          f_weight = wiegen(false); 
          startMillis = currentMillis;
    }

    currentMillis = millis(); 
    if (digitalRead(TOUCH_PIN)== HIGH) Kommando = "";  // löschen wenn gedrückt, nur dann
   
   while (digitalRead(TOUCH_PIN)== HIGH) {
    dauer =  millis() - currentMillis;

    if (Kommando.equals("")) {                              // Start, bis etwas gedrückt
       LED_Gewicht( (int) f_weight);                        // Farbe nach gewicht
    }
    if (  Kommando.equals("feed")  &&  dauer > 6000) {      //  tara 
      Kommando = "tara";
      LED_light(0,0,255) ;  warte(100);
      LED_light(0,0,0)   ;  warte(100);
      
    } else if ( Kommando.equals("move")  && dauer > 3000) { //  feed c
      Kommando = "feed";
      mqttClient.publish(TOPIC"/temp", 0, true, "--> feed"); 
      LED_light(0,255,255);
      Serial.print("debug 004b");  Serial.println(dauer); 
   
    } else if (  Kommando.equals("")  &&  dauer > 1000) {   //  Move
      mqttClient.publish(TOPIC"/temp", 0, true, "--> move"); 
      Kommando = "move";
      LED_light(255,0,255) ;
      // currentMillis += 1000; // eine Sekunde wieder herstellen
      Serial.print("debug 004c");  Serial.println(dauer); 
    } 
     warte(100);
   }
   //  warte(500);
   if (dauer > 0 && dauer < 1000 ) { // update gewicht , sag mir was drin ist :-)
    // f_weight + 1 gramm, damit ein change ausgelösdt wird im openhab
        sprintf(tmpbuffer,"%i",(int) f_gewicht -1) ; 
        mqttClient.publish(TOPIC"/weight", 1 , true, tmpbuffer); // war 2  
   }
    
}

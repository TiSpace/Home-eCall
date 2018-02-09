/*
 *    Home eCall
 *    Emergency button connected via wifi to MQTT server
 *    
 *    function
 *    - alarm button sets alarmflag               -> MQTT:  Notruf/Alarm
 *    - during alarm will be saved, too
 *      * the time                                -> MQTT:  Notruf/Zeit
 *      * battery voltage                         -> MQTT:  Notruf/BatterieSpg
 *      * time of battery measurement             -> MQTT:  Notruf/BatterieZeit
 *    - pressing reset button will reset alarm
 *    - transmitter will go to modem sleep in order to reduce current consumption from ~80mA to 20mA
 *      and extend battery life time. Wake up only triggered by pressing a button
 *    - wifi setting is done on start based on Wifimanager-library
 *    
 *    setting
 *    - define unique ID                                        -> MQTT_ClientID 
 *    - define if device has to act as sender (battery driven)  -> #define   Sender
 *    - setup MQTT parameters
 */





#include "setting.h" // contains all settings
#include <Streaming.h>
#include <Time.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <NTPClient2.h>// source: https://github.com/arduino-libraries/NTPClient
#include <WiFiUdp.h>

// Use WiFiClient class to create TCP connections
WiFiClient client;

//MQTT Instanz erstellen
PubSubClient MQTT_Client(client);

//WiFiUDP for NTP connection
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

// create instance of WM
WiFiManager wifiManager;



// Variables
byte            Taste[2]            =   {1, 1};
byte            TastePrev[2]        =   {1, 1};
char            buffer[50];                       // buffer for converting strings/char
unsigned long   timeSinceActive;                  // time since last publish action
float           readBatteryVolt     =   9;        // returned battery voltage
unsigned long   timeRefresh         =   0;        //timer to do regular status update to MQTT

void setup() {
  Serial.begin(myBaudRate);
  Serial.println();
  Serial << ("Willkommen  ") << "DeviceID: " << MQTT_ClientID << endl;
  Serial << "Version: " << myVersion << endl;

  pinMode(LED_rot, OUTPUT);
  pinMode(LED_Warn, OUTPUT);
  pinMode(TasteAlarm, INPUT);   // pressed =0
  pinMode(TasteReset, INPUT);   // pressed =0
  pinMode(TasteWifi, INPUT);    // to reset wifi

  digitalWrite(LED_rot, HIGH);
  digitalWrite(LED_Warn, HIGH);


  digitalWrite(LED_rot, !digitalRead(LED_rot));
  digitalWrite(LED_Warn, !digitalRead(LED_Warn));
  delay(100);

  digitalWrite(LED_rot, !digitalRead(LED_rot));
  digitalWrite(LED_Warn, !digitalRead(LED_Warn));
  delay(100);




  // Einstellungen MQTT Server vervollständingen
  MQTT_Client.setServer(mqtt_server, cloudmqttport);
  MQTT_Client.setCallback(callback);





  // We start by connecting to a WiFi network
  verbinde();
  timeClient.begin();


  Serial.println("Ready for action!");


}
/*  ---------------------------------------------------------------------------
                      main loop
*/
void loop() {

  Taste[0] = digitalRead(TasteAlarm);
  Taste[1] = digitalRead(TasteReset);

  // is ResetWiFi button pressed?
  if (digitalRead(TasteWifi) == 0) {
    WiFi.forceSleepWake();
    WiFiManager wifiManager;      //restart AP following example OnDemandAP

    if (!wifiManager.startConfigPortal(APName)) {
      Serial.println("failed to connect and hit timeout");
      delay(3000);
      //reset and try again, or maybe put it to deep sleep
      ESP.reset();
      delay(5000);
    }
  }



  int voltageBattery = measureADC(10);  // measure and average ADC

  // evaluate battery of the remote sender
#ifndef Sender
  // react on battery status of sender
  if (readBatteryVolt < batThreshold) {
    if ((millis() / 1000) % 10) {
      // buzzer on
      digitalWrite(LED_Warn, HIGH);
    } else {
      digitalWrite(LED_Warn, LOW);

    }
  }
#endif

#if (enBuzAlarm==1)
  // if alarm is on, it will be supported by the buzzer
  if ((millis() / 1000) % 3) {
    digitalWrite(LED_Warn, digitalRead(LED_rot));
  } else {
    digitalWrite(LED_Warn, HIGH);
  }
#endif



  String strZeit ;
#ifdef Sender
  if (((Taste[0] == 0 || Taste[1] == 0) && ((Taste[0] != TastePrev[0]) || (Taste[1] != TastePrev[1] ))) || (millis() - timeRefresh) > refreshTime)   {
#if (enBuzAlarm==1)
    digitalWrite(LED_Warn, HIGH); //turn off buzzer
#endif


    timeSinceActive = millis();   //store current time

    // mit dem MQTT Server verbinden

    //Serial << "Tasten" << Taste[0] << "(" << TastePrev[0] << ")  " << Taste[1] << "(" << TastePrev[1] << ")  " << endl;
    WiFi.forceSleepWake();
    Serial.println("-- woke up");
    delay(500);

    verbinde();
    reconnect();

    dtostrf((float)voltageBattery * Aufloesung, 5, 2, buffer);

    MQTT_Client.publish("Notruf/BatterieSpg", buffer, true);
    Serial << "Spannung: "  << (float)voltageBattery * Aufloesung << "V" << endl;
    getTimeFromNTP(); // Zeit aktualisieren
    strZeit = timeClient.getFullFormattedTime(); //http://forum.arduino.cc/index.php?topic=241222.0

    strZeit.toCharArray(buffer, strZeit.length() + 1);
    MQTT_Client.publish("Notruf/BatterieZeit", buffer, true);
    Serial.println();

    // reset refresh timer
    timeRefresh = millis();
  }
#endif

#ifndef Sender
    reconnect();  // establish MQTT connection as well
#endif


  // Taste0 auswerten   ->  Button Notruf
  if (Taste[0] == 0 && (Taste[0] != TastePrev[0])) {
    digitalWrite(LED_Warn, !digitalRead(LED_Warn));
    delay(50);
    digitalWrite(LED_Warn, !digitalRead(LED_Warn));

    Serial.println("+ + +   A L A R M   + + +");
    MQTT_Client.publish("Notruf/Alarm", "1", true);

    getTimeFromNTP(); // Zeit aktualisieren
    strZeit = timeClient.getFullFormattedTime(); //http://forum.arduino.cc/index.php?topic=241222.0

    strZeit.toCharArray(buffer, strZeit.length() + 1);
    MQTT_Client.publish("Notruf/Zeit", buffer, true);
  }



  // Taste1 auswerten   ->  Button Reset
  if (Taste[1] != TastePrev[1]) {

    if (Taste[1] == 0) {
      digitalWrite(LED_Warn, !digitalRead(LED_Warn));
      delay(50);
      digitalWrite(LED_Warn, !digitalRead(LED_Warn));
      Serial.println("+ + +   Alarm Reset");
      MQTT_Client.publish("Notruf/Alarm", "0", true);
    }

  }
  // store button status in order to detect slope
  TastePrev[0] = Taste[0];
  TastePrev[1] = Taste[1];


  MQTT_Client.loop(); //get subscribes

#ifdef Sender
  if ((millis() - timeSinceActive) > subsAftAct) { //allow to subscribe within a time window of xx sec and receive messages
    WiFi.forceSleepBegin();
    delay(100);
  }

  // connect to MQTT
  if (!MQTT_Client.connected())
  {
#ifndef Sender
    reconnect(); //er Empfänger muss immer lauschreceiver has to listen
#endif
  }
#endif
  MQTT_Client.loop();



}


/*  -------------------------------------------------------------------------------------
    reconnect to the MQTT Server in order to get subscribed messages

*/
void reconnect() {

  // Loop until we're reconnected
  while (!MQTT_Client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (MQTT_Client.connect(MQTT_ClientID, mqtt_user, mqtt_password)) {
      Serial.println("connected");
      //#ifndef Sender
      MQTT_Client.subscribe("Notruf/#"); // nue subscribe
      //#endif
    } else {
      Serial.print("failed, rc=");
      Serial.print(MQTT_Client.state());
      Serial.println(" try again in 2 seconds");
      
      delay(2000);                              // Wait 2 seconds before retrying

    }
  }
}







void callback(char* topic, byte * payload, unsigned int length)
{
  /* die vom MQTT ankommenden Daten werden ausgewertet und in den Variablen abgelegt.
      Eigentliche Übermittlung passiert in der Kommunikationsroutine
  */


  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  // Eigentliche Payload ausgeben
  for (int i = 0; i < length; i++)  {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  String strTopic((char*)topic);
  if (strTopic == "Notruf/Alarm") {
    if ((char)payload[0] == '1') {
      digitalWrite(LED_rot, LOW);
      Serial.println("+++ Lampe an");
    }
    else {
      digitalWrite(LED_rot, HIGH);
      Serial.println("+++ Lampe aus");
    }
    //Serial.println();
  }
  if (strTopic == "Notruf/BatterieSpg") {
    // convert String (Batterievoltage) to float
    payload[length] = '\0';
    String s = String((char*)payload);
    readBatteryVolt = s.toFloat();


  }

  // Topic trimmen


  String str((char*)topic); //https://stackoverflow.com/questions/17158890/transform-char-array-into-string

  int StartPos = str.indexOf("/") + 1;



  delay(100);
  //Serial.println();
}





void verbinde() {
  // We start by connecting to a WiFi network
  if (!wifiManager.autoConnect(APName)) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    ESP.reset();
    delay(5000);
  }

}



/* --------------------------------------------------------------------------------
     purpose: get time from NTP server and prepare data
*/
void getTimeFromNTP() {
  timeClient.update();        // hole Zeit von NTP

}

/* --------------------------------------------------------------------------------
   purpose: measures for a given number of repetitions the ADC and building average
            this is for one give ADC channel only
*/
int measureADC(byte noMeasures) {
  int var = 0;
  for (int i = 0; i < noMeasures; i++) {
    delay(5); // wait a bit to reduce noise
    var = var + analogRead(readADC);
  }
}




/* --------------------------------------------------------------------------------
    Print an integer in "00" format (with leading zero).
         Input value assumed to be between 0 and 99.
*/

void sPrintI00(int val)
{
  if (val < 10) Serial.print('0');
  Serial.print(val, DEC);
  return;
}

/*  Notrufsender
    - Taste Alarm setzt via MQTT das Alarmflag
    - Taste REset kann Alarm zurücksetzten
    - beim Sender wird Batteriestatus gemessen und übermittelt
    - Sender geht in Modem Sleep und wacht nur bei Tastenbetätigung auf

    Konfiguration
    - SSID/PW
    - Sender oder Empfänger
    - MQTT_ClientID muß einzigartig sein

    Historie:
    V00: funktionale FW
    V01: Wifi-Manager anstatt fixer SSID/PW und AP-Setting on Demand
    V02: subscibtion der LED läuft
         warning low battery-feedback (LED+buzzer)

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
  pinMode(TasteAlarm, INPUT);   // gedrückt =0
  pinMode(TasteReset, INPUT);   // gedrückt =0
  pinMode(TasteWifi, INPUT);    // um WIFI-Setting zurückzusetzten

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
                      Hauptschleife
*/
void loop() {

  //  while(1){
  //int voltageBattery = measureADC(10);  // messe und mittle ADC-Spannung
  //
  //Serial << voltageBattery << "  Spannung: "  << (float)voltageBattery * Aufloesung << "V" << endl;
  //delay(500);
  //  }

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



  int voltageBattery = measureADC(10);  // messe und mittle ADC-Spannung

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

    Serial << "Tasten" << Taste[0] << "(" << TastePrev[0] << ")  " << Taste[1] << "(" << TastePrev[1] << ")  " << endl;
    WiFi.forceSleepWake();
    Serial.println("aufgewacht");
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


  MQTT_Client.loop(); //erlaube Subsrcibes zu holen

#ifdef Sender
  if ((millis() - timeSinceActive) > subsAftAct) { //allow to subscribe within a time window of xx sec and receive messages
    WiFi.forceSleepBegin();
    //Serial.println("schlafe wieder");
    delay(100);
  }

  // mit dem MQTT Server verbinden
  if (!MQTT_Client.connected())
  {

    reconnect(); //der Empfänger muss immer lauschen

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
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
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
  //Serial.println("Zeit angefordert");

  timeClient.update();        // hole Zeit von NTP
  // Serial.println(timeClient.getFormattedTime());  //formatierte Zeit
  /*

    sPrintI00(timeClient.getHours());
    sPrintI00(timeClient.getMinutes());
    sPrintI00(timeClient.getSeconds());
    sPrintI00(timeClient.getYear());
    sPrintI00(timeClient.getMonth());
    sPrintI00(timeClient.getDate());
  */
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

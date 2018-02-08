/*
 * 		Settings
*/

//siehe auch http://blue-pc.net/2014/10/15/mqtt-nachrichten-mit-dem-arduino-empfangen-senden/
// https://www.cs.auckland.ac.nz/references/unix/digital/AQTLTBTE/DOCU_154.HTM#book-index

#define   myBaudRate	38400

#define   myVersion "Notruf V2"

#define   MQTT_ClientID "ESP8266Client01" //muß ein einmaliger Name sein
#define   Sender


// -- WiFi ----------------------------------------
#define   APName  "Home eCall"            // Name showns as AP

// -- MQTT ----------------------------------------
#define   mqtt_server     "mxx.cloudmqtt.com"
#define   mqtt_user       "myUser"
#define   mqtt_password   "myPW"
#define   cloudmqttport   12345


// -- Pins ----------------------------------------
// Pin and Port setting - don'T change as this would require a hardware modification to fit

#define   LED_rot       13
#define   LED_Warn       5
#define   TasteAlarm    12
#define   TasteReset    14
#define   TasteWifi      4
#define   readADC       A0    //ADC

// -- others ----------------------------------------
#define   Aufloesung    3407/771000 //1bit = 4.412mV
#define   subsAftAct    20*1000 // time window, how long subscribtion is enabled afer any publish action
#define   batThreshold  3.2
#define   enBuzAlarm    1   //if defined, the alarm LED will go along buzzer
#define   refreshTime   30*60*1000  // after what time a status update will be proceeded

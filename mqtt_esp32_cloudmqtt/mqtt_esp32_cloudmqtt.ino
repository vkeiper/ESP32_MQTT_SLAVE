/*********
  Rui Santos
  Complete project details at https://randomnerdtutorials.com  
*********/

#include <WiFi.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Ticker.h>


#define pinTSNS1 33
#define pinTSNS2 27
#define pinTSNS3 2
// Data wire is plugged into GPIO 16 on the ESP32
#define ONE_WIRE_BUS 16
#define TEMPERATURE_PRECISION 12 // Lower resolution


uint8_t aryLeds[] = { pinTSNS1,pinTSNS2,pinTSNS3};
// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);
// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire);
int numberOfDevices; // Number of temperature devices found
float tempCH[3];
DeviceAddress tempDeviceAddress; // We'll use this variable to store a found device address

DeviceAddress tsnsRoom      = { 0x28,0x10,0xC3,0x77,0x91,0x11,0x02,0x10 };
DeviceAddress tsnsAttic     = { 0x28,0x96,0x77,0x77,0x91,0x08,0x02,0x46 };
DeviceAddress tsnsCondensor = { 0x28,0xD7,0x92,0x77,0x91,0x08,0x02,0x0C };

const char *ssid =  "NETGEAR24";   // cannot be longer than 32 characters!
const char *pass =  "shinydiamond465";
//const char *ssid =  "NETGEAR26";   // cannot be longer than 32 characters!
//const char *pass =  "fluffyvalley904";   //

// Local MOsquitto MQTT server
//const char *mqtt_server = "192.168.1.18";//"m11.cloudmqtt.com";
//const int  mqtt_port = 1883;//10965;
//const char *mqtt_user = "vk3";//"esp32slave";
//const char *mqtt_pass = "3191";//"esp32slave";
//const char *mqtt_client_name = "ESP32_DAC";//"ESP32_DAC"; // Client connections cant have the same connection name

// CloudMQTT Server
const char *mqtt_server = "m11.cloudmqtt.com";
const int  mqtt_port = 10965;
const char *mqtt_user = "xnaumdgj";
const char *mqtt_pass = "bnWuFjLrnll3";
const char *mqtt_client_name = "ESP32_DAC"; // Client connections cant have the same connection name


#define BUFFER_SIZE 100
 

WiFiClient espClient;
PubSubClient client(espClient);  //instanciates client object
 
long lastMsg = 0;
char msg[50];
int value = 0;
unsigned long previousMillis = 0;
const long interval = 10000;   
 
float temperature = 0;
float humidity = 0;

// LED Pin
const int ledPin = 2;

/** Task handle for the 1 wire temp sensor read task */
TaskHandle_t thdlOneWire = NULL;
/** Ticker for temperature reading */
Ticker tkrTriggerOneWire;
/** Flags for temperature readings finished */
bool gotNewTempData = false;
/* Flag if main loop is running */
bool taskOneWireEnabled = false;



/**
 * Task to reads temperatures from 1 wire sensors
 * @param pvParameters
 *    pointer to task parameters
 */
void tskOneWire(void *pvParameters) {
  Serial.println("OneWire Task loop started");
  while (1) // 1 wire task loop
  {
    if (taskOneWireEnabled && !gotNewTempData) { // Read temperature only if old data was processed already
      // call sensors.requestTemperatures() to issue a global temperature request to all devices on the bus
      Serial.print("Requesting temperatures...");
      sensors.requestTemperatures(); // Send the command to get temperatures
      Serial.println("DONE");
      // Loop through each device, print out temperature data
      for(int i=0;i<numberOfDevices; i++)
      {
          // Search the wire for address
          if(sensors.getAddress(tempDeviceAddress, i))
          {
            // Output the device ID
            Serial.print("Temperature for device: ");
            Serial.println(i,DEC);
            // It responds almost immediately. Let's print out the data
            tempCH[i] = printTemperature(tempDeviceAddress); // Use a simple function to print out the data
        
            digitalWrite(aryLeds[0], LOW); 
            digitalWrite(aryLeds[1], LOW);
            digitalWrite(aryLeds[2], LOW);
                 
            digitalWrite(aryLeds[i], HIGH); 
          } 
          //else ghost device! Check your power requirements and cabling
      }

      // if an alarm condition exists as a result of the most recent 
      // requestTemperatures() request, it exists until the next time 
      // requestTemperatures() is called AND there isn't an alarm condition
      // on the device
      if (sensors.hasAlarm())
      {
        Serial.println("TEMP SENSOR ALARM!!!  At least one alarm on the 1-Wire bus.");
      }
    
      // call alarm handler function defined by sensors.setAlarmHandler
      // for each device reporting an alarm
      sensors.processAlarms();
    
      // inform main loop new temps rxd
      gotNewTempData = true;
    }
    // put task back to sleep until ticker fires
    vTaskSuspend(NULL);
  }
}


/**
 * triggerGetOneWireData
 * Sets flag dhtUpdated to true for handling in loop()
 * called by Ticker tempTicker
 */
void triggerGetOneWireData() {
  if (thdlOneWire != NULL) {
     xTaskResumeFromISR(thdlOneWire);
      Serial.println("1-Wire Sensor Task: Resumed");
  }else{
      Serial.println("1-Wire Sensor Task: Disabled");
  }
}

void CreateTaskOneWire(void)
{
    // Start task to get temperature from 1 wire
  xTaskCreatePinnedToCore(
      tskOneWire,                      /* Function to implement the task */
      "1 Wire Temp Sensor",                    /* Name of the task */
      1000,                          /* Stack size in words */
      NULL,                          /* Task input parameter */
      5,                              /* Priority of the task */
      &thdlOneWire,                /* Task handle. */
      1);                            /* Core where the task should run */

  if (thdlOneWire == NULL) {
    Serial.println("[ERROR] Failed to start task for 1-Wire sensor update");
    delay(5000);
  } else {
    // Start update of environment data every 5 seconds
    tkrTriggerOneWire.attach(5, triggerGetOneWireData);
  }

  // Signal end of setup() to OneWire task 
  taskOneWireEnabled = true;
}

/**
 * Initialize 1-Wire temperature sensor library
 */
void InitOneWireSensors(void)
{
    pinMode(pinTSNS1, OUTPUT);  
    pinMode(pinTSNS2, OUTPUT);  
    pinMode(pinTSNS3, OUTPUT);  
  

  // Start up the library
  sensors.begin();

  // set alarm ranges
  Serial.println("Setting alarm temps...");
  sensors.setHighAlarmTemp(tsnsRoom, DallasTemperature::toCelsius(75));
  sensors.setLowAlarmTemp(tsnsRoom, DallasTemperature::toCelsius(68));
    
  printAlarmInfo(tsnsRoom);
    
  // attach alarm handler
  sensors.setAlarmHandler(&roomAlarmHandler);
  
  // Grab a count of devices on the wire
  numberOfDevices = sensors.getDeviceCount();
  
  // locate devices on the bus
  Serial.print("Locating devices...");
  
  Serial.print("Found ");
  Serial.print(numberOfDevices, DEC);
  Serial.println(" devices.");

  // report parasite power requirements
  Serial.print("Parasite power is: "); 
  if (sensors.isParasitePowerMode()) Serial.println("ON");
  else Serial.println("OFF");
  
  // Loop through each device, print out address
  for(int i=0;i<numberOfDevices; i++)
  {
      // Search the wire for address
      if(sensors.getAddress(tempDeviceAddress, i))
      {
          Serial.print("Found device ");
          Serial.print(i, DEC);
          Serial.print(" with address: ");
          printAddress(tempDeviceAddress);
          Serial.println();
          
          Serial.print("Setting resolution to ");
          Serial.println(TEMPERATURE_PRECISION, DEC);
          
          // set the resolution to TEMPERATURE_PRECISION bit (Each Dallas/Maxim device is capable of several different resolutions)
          sensors.setResolution(tempDeviceAddress, TEMPERATURE_PRECISION);
          
          Serial.print("Resolution actually set to: ");
          Serial.print(sensors.getResolution(tempDeviceAddress), DEC); 
          Serial.println();
      }else{
          Serial.print("Found ghost device at ");
          Serial.print(i, DEC);
          Serial.print(" but could not detect address. Check power and cabling");
      }
  }

  // Must be called before search()
  oneWire.reset_search();
  // assigns the first address found to....
  if (!oneWire.search(tsnsRoom)) Serial.println("Unable to find address for Ambient Room Temp Sensor");
  // assigns the 2nd address found to....
  if (!oneWire.search(tsnsAttic)) Serial.println("Unable to find address for Attic Temp Sensor");
  // assigns the 3rd address found to....
  if (!oneWire.search(tsnsCondensor)) Serial.println("Unable to find address for AC Condensing Coil Temp Sensor");

}

void printAlarmInfo(DeviceAddress deviceAddress)
{
  char temp;
  printAddress(deviceAddress);
  temp = sensors.getHighAlarmTemp(deviceAddress);
  Serial.print("High Alarm: ");
  Serial.print(temp, DEC);
  Serial.print("C");
  Serial.print(" Low Alarm: ");
  temp = sensors.getLowAlarmTemp(deviceAddress);
  Serial.print(temp, DEC);
  Serial.print("C");
  Serial.print(" ");
}

// function to print a device address
void printAddress(DeviceAddress deviceAddress)
{
  for (uint8_t i = 0; i < 8; i++)
  {
    if (deviceAddress[i] < 16) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
  }
}
// function to print the temperature for a device
float printTemperature(DeviceAddress deviceAddress)
{
  float tempC = sensors.getTempC(deviceAddress);
  Serial.print(" Temp C: ");
  Serial.print(tempC);
  Serial.print(" Temp F: ");
  Serial.print(DallasTemperature::toFahrenheit(tempC)); // Converts tempC to Fahrenheit
  return DallasTemperature::toFahrenheit(tempC);
}
// function that will be called when an alarm condition exists during DallasTemperatures::processAlarms();
void roomAlarmHandler(const uint8_t* deviceAddress)
{
  Serial.println("Alarm Handler Start"); 
  Serial.print("Device in alarm: ");
  printAddress((uint8_t*)deviceAddress);
  printTemperature((uint8_t *)deviceAddress);
  Serial.println();
  Serial.println("Alarm Handler Finish");
}




/** Task handle for the MQTT publish task */
TaskHandle_t thdlMqttPub = NULL;
/** Ticker for Mqtt Publishing */
Ticker tkrTriggerMqttPub;
/** Flag for Mqtt Publish finished */
bool bSentMqtt = false;
/* Flag if main loop is running */
bool taskMqttPubEnabled = false;

/**
 * triggerGetOneWireData
 * Resume task to publish Mqtt
 * called by Ticker tkrTriggerMqttPub
 */
void triggerPubMqtt() {
  if (thdlMqttPub != NULL) {
     xTaskResumeFromISR(thdlMqttPub );
      Serial.println("Mqtt Publish Task: Resumed");
  }else{
      Serial.println("Mqtt Task: Disabled");
  }
}

/**
 * Task to Publish Mqtt
 * @param pvParameters
 *    pointer to task parameters
 */
void tskPubMqtt(void *pvParameters) {
  String str="";
  
  Serial.println("Mqtt Publish Task loop started");
  while (1) // 1 wire task loop
  {
    if (taskMqttPubEnabled && !bSentMqtt) { // Send only if data was not yet processed
      if (client.connected()){
        unsigned long currentMillis = millis();
        str = String(currentMillis/1000);
        //srand(currentMillis); //create a random value based on time
        //int h = rand()%100; // sets value between 0-99
        client.publish("/TOHOST/GET/TIME",string2char(str));
     
        str = String("CH0 " + String(tempCH[0]) +" " + "CH1 " +String(tempCH[1]) +" " + "CH2 " + String(tempCH[2]));
        Serial.println(str);
        client.publish("/TOHOST/GET/TEMP/ROOM",string2char(String(tempCH[0])));
        client.publish("/TOHOST/GET/TEMP/ATIC",string2char(String(tempCH[1])));
        client.publish("/TOHOST/GET/TEMP/ACOIL",string2char(String(tempCH[2])));
      }
      bSentMqtt = true;
    }
    vTaskSuspend(NULL);
  }
}

void CreateTaskPubMqtt(void)
{
    // Start task to get temperature from 1 wire
  xTaskCreatePinnedToCore(
      tskPubMqtt,                      /* Function to implement the task */
      "Publish Mqtt",                    /* Name of the task */
      8000,                          /* Stack size in words */
      NULL,                          /* Task input parameter */
      5,                              /* Priority of the task */
      &thdlMqttPub,                /* Task handle. */
      1);                            /* Core where the task should run */

  if (thdlMqttPub == NULL) {
    Serial.println("[ERROR] Failed to start task for Mqtt Publishing");
    delay(5000);
  } else {
    // Start Publishing Mqtt data every x seconds
    tkrTriggerMqttPub.attach(5, triggerPubMqtt);
  }

  // Signal end of setup() to OneWire task 
  taskMqttPubEnabled = true;
}


/**
 * Mqtt messge received callback
 */
void callback(char* topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;
  
  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();

  // Feel free to add more if statements to control more GPIOs with MQTT

  // If a message is received on the topic esp32/output, you check if the message is either "on" or "off". 
  // Changes the output state according to the message
  if (String(topic) == "ESP32/SET/LED/ACMAINS") {
    Serial.print("Changing output to ");
    if(messageTemp == "ON"){
      Serial.println("ON");
      digitalWrite(ledPin, HIGH);
    }
    else if(messageTemp == "OFF"){
      Serial.println("OFF");
      digitalWrite(ledPin, LOW);
    }
  }
}


void setup() {
  Serial.begin(115200);
  
  pinMode(ledPin, OUTPUT);

  InitOneWireSensors();
  
  
  setup_wifi();
  client.setServer(mqtt_server,mqtt_port);
  client.setCallback(callback);

  // Creates Task to periodically poll 1 wire sensors
  CreateTaskOneWire();

  // Creates Task to periodically publish MQTT data
  CreateTaskPubMqtt();
}




void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {  //wifi not connected?
    Serial.print("Connecting to Router");
    Serial.print(ssid);
    Serial.println("...");
    WiFi.mode( WIFI_STA );
    WiFi.begin(ssid, pass);
 
    if (WiFi.waitForConnectResult() != WL_CONNECTED)
      return;
    Serial.println("WiFi connected");
  }
 
  if (WiFi.status() == WL_CONNECTED) {
    //client object makes connection to server
    if (!client.connected()) {
       Serial.println("Attempting connection to MQTT server");
       //Authenticating the client object
       if (client.connect(mqtt_server,mqtt_user, mqtt_pass)) {
        Serial.println("Connected to MQTT server");
        
        //Subscribe code
        client.setCallback(callback);
        client.subscribe("ESP32/SET/LED/ACMAINS");
        
      } else {
        Serial.println("Could not connect to MQTT server");   
      }
    }
 
    if (client.connected()){
      client.loop();
    }
  }

   /*--------------------Start OneWire Main Loop actions --------------------------------------*/
  if (gotNewTempData) {
       Serial.println("Got New 1-Wire Temps....");   
       gotNewTempData  = false;      
  }

  /*-------------------Allow Next Mqtt Publish------------------------------------------------*/
  if (bSentMqtt) {
       Serial.println("Published Mqtt to broker ....");  
       bSentMqtt  = false;
  }
    
}
char* string2char(String command){
    if(command.length()!=0){
        char *p = const_cast<char*>(command.c_str());
        return p;
    }
}


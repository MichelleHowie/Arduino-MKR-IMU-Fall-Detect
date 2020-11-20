// libraries
#include <MKRNB.h>
#include <PubSubClient.h>
#include <Arduino_MKRENV.h>
#include <ArduinoJson.h>
#include <MKRIMU.h>

//The MKRNB lib needs this even if its blank
const char PINNUMBER[]     = "";

//find these in the Watson IoT client
#define token "YOURS_HERE"
#define ORG "YOURS_HERE"
#define DEVICE_TYPE "YOURS_HERE"
#define DEVICE_ID "YOURS_HERE"

//broker url in here, demo is using eclipse test server
char mqttBroker[]  = ORG ".messaging.internetofthings.ibmcloud.com";

//broker port in here
int mqttPort = 1883;
char authMethod[] = "use-token-auth";
char clientId[] = "d:" ORG ":" DEVICE_TYPE ":" DEVICE_ID;

//var things for storage
//char payload[200];
const char topicSubscribe[] = "iot-2/cmd/trig/fmt/text";

// initialize the library instance
NBClient client;
GPRS gprs;
NB nbAccess;

//connect the pubsub client
PubSubClient conn(client);

//for the callbacks from the broker
void callback(char* topic, byte* payload, unsigned int length) {
  char p[length + 1];
  memcpy(p, payload, length);
  p[length] = NULL;
  String msg(p);

  if(msg=="on"){
    StaticJsonDocument<200> doc;
    String output;
    doc["temp"] = ENV.readTemperature();
    doc["humidity"] = ENV.readHumidity();
    doc["pressure"] = ENV.readPressure();
    doc["illuminance"] = ENV.readIlluminance();
    doc["uvi"] = ENV.readUVIndex();
    serializeJson(doc, output);
    char buffer[256];
    size_t n = serializeJson(doc, buffer);
    conn.publish("iot-2/evt/sensor/fmt/json", buffer, n);
    Serial.println(output);
  }  
}

//connection and reconnection function 
void reconnect() {
    while (!conn.connected()) {

    // Attemp to connect
    if (conn.connect(clientId, authMethod, token)) {
      Serial.println("Connected");
      conn.subscribe(topicSubscribe);
    } else {
      Serial.print("Failed, rc=");
      Serial.print(conn.state());
      Serial.println(" try again in 2 seconds");

      // Wait 2 seconds before retrying
      delay(2000);
    }
  }
}
void setup() {

  // initialize serial communications and wait for port to open:
  Serial.begin(9600);
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.println("Warming up....");
  
  // connection state
  boolean connected = false;
  while (!connected) {
    if ((nbAccess.begin(PINNUMBER) == NB_READY) &&
        (gprs.attachGPRS() == GPRS_READY)) {
      connected = true;
    } else {
      Serial.println("Not connected");
      delay(1000);
    }
  }
  Serial.println("connecting...");
  
  //set the connection
  conn.setServer(mqttBroker, mqttPort);
  
  //set the callback 
  conn.setCallback(callback);
  
  //subscribe to a topic 
  conn.subscribe(topicSubscribe);

  //initialize the ENV shield
  if (!ENV.begin()) {
    Serial.println("Failed to initialize MKR ENV shield!");
    while (1);
  }
  
  //initialize the IMU shield
  if (!IMU.begin()) {
    Serial.println("Failed to initialize IMU!");

    while (1);
  }
}

//initialise the variables for storing x,y,z acceleration in the second state
float x2, y2, z2;

void loop() {
  if (!conn.connected()) {
    reconnect();
  }
  conn.loop();

  // declare x,y,z variables that store the first state of acceleration in Gs
  float x1, y1, z1;
  if (IMU.accelerationAvailable()) {
    IMU.readAcceleration(x1, y1, z1);

    //find the difference between the last 2 states of the acceleration coordinates
    float at = sqrt(x1*x1 + y1*y1 + z1*z1);

    //in steady state (i.e not moving) the value of at should be 1
    //so anything significantly above or below that (+-.1G), should indicate that
    //it has moved off of the steady state and hence; a fall can be inferred!
    //if you want to make the fall detect more or less sensitive, 
    //you can play with these threshold numbers
    if(at > 1.10 || at < 0.90){
      char json[] = "{\"fall\":\"true\"}";

      //publish to our iot broker in the 'fall'event
      conn.publish("iot-2/evt/fall/fmt/json", json);

      Serial.println(at);
      
      //in the event of a change in acceleration, the value of 'at' 
      //will be at a state to trigger this if loop for multiple times a second
      //so we delay for 5 seconds so that a fall will only trigger once / 5s
      delay(5000);
    }
  }  
}

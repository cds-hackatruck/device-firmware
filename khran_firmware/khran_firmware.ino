#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <math.h>

#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#include <TinyGPS++.h>
#include <SoftwareSerial.h>

const bool use_gps = false;

static const int RXPin = D5, TXPin = D6;
static const uint32_t GPSBaud = 9600;
// The TinyGPS++ object
TinyGPSPlus gps;
// The serial connection to the GPS device
SoftwareSerial ss(RXPin, TXPin);

//atualize SSID e senha WiFi
const char* ssid = "HackaTruckVisitantes";
const char* password = "";

#define LED_BUILTIN D4

//Atualize os valores de Org, device type, device id e token
#define ORG "74ykm4"
#define DEVICE_TYPE "sensorAcelerometro"
#define DEVICE_ID "accKhran"
#define TOKEN "12345678"

char server[] = ORG ".messaging.internetofthings.ibmcloud.com";
char topic[] = "iot-2/evt/status/fmt/json";
char authMethod[] = "use-token-auth";
char token[] = TOKEN;
char clientId[] = "d:" ORG ":" DEVICE_TYPE ":" DEVICE_ID;

WiFiClient wifiClient;
PubSubClient client(server, 1883, NULL, wifiClient);

// Create an instance of the MPU6050 class
Adafruit_MPU6050 mpu;

const int numReadings = 5;
const float G = 9.81;

float axReadings[numReadings];
float ayReadings[numReadings];
float azReadings[numReadings];
int readIndex = 0;
float axTotal = 0;
float ayTotal = 0;
float azTotal = 0;
float axAverage = 0;
float ayAverage = 0;
float azAverage = 0;
float l_lat = 0;
float l_lng = 0;

void setup() {
  Serial.begin(9600);

  Serial.print("Conectando na rede WiFi "); Serial.print(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("[INFO] Conectado WiFi IP: ");
  Serial.println(WiFi.localIP());

  pinMode(LED_BUILTIN, OUTPUT);

  // Initialize the MPU6050 sensor
  if (!mpu.begin()) {
    Serial.println("Failed to initialize MPU6050 sensor");
    while (1);
  }
  mpu.setAccelerometerRange(MPU6050_RANGE_16_G);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  Serial.println("");

  for (int thisReading = 0; thisReading < numReadings; thisReading++) {
    axReadings[thisReading] = 0;
    ayReadings[thisReading] = 0;
    azReadings[thisReading] = 0;
  }

  ss.begin(GPSBaud);
}

void loop() {
  if (!!!client.connected()) {
    Serial.print("Reconnecting client to ");
    Serial.println(server);
    while (!!!client.connect(clientId, authMethod, token)) {
      Serial.print(".");
      delay(500);
    }
    Serial.println();
  }

  // Get the accelerometer data
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  float ax = a.acceleration.x;
  float ay = a.acceleration.y;
  float az = a.acceleration.z;

  axTotal -= axReadings[readIndex];
  ayTotal -= ayReadings[readIndex];
  azTotal -= azReadings[readIndex];

  // add the new readings
  axReadings[readIndex] = ax;
  ayReadings[readIndex] = ay;
  azReadings[readIndex] = az;

  // add the new readings to the total
  axTotal += axReadings[readIndex];
  ayTotal += ayReadings[readIndex];
  azTotal += azReadings[readIndex];

  // advance to the next position in the array
  readIndex = (readIndex + 1) % numReadings;

  // calculate the running average
  axAverage = axTotal / numReadings;
  ayAverage = ayTotal / numReadings;
  azAverage = azTotal / numReadings;

  float gx = fabs(axAverage)/G;
  float gy = fabs(ayAverage)/G;
  float gz = fabs(azAverage - G)/G;

  float lat = 0;
  float lng = 0;
  if(use_gps){
    while (ss.available() > 0){
      gps.encode(ss.read());
      if (gps.location.isUpdated()){
        lat = gps.location.lat();
        lng = gps.location.lng();

        if(lat != 0 || lng != 0){
          l_lat = lat;
          l_lng = lng;
          break;
        }
      }
    }
  }

  if(lat != 0){
    Serial.println("new Latitude");
  }

  if(gx > 1.5 || gy > 1.5 || gz > 1.5){
    float max_gforce = max(max(gx, gy), gz);
    
    //get GPS positioning data
    Serial.print("Latitude= "); 
    Serial.print(l_lat, 6);
    Serial.print(" Longitude= "); 
    Serial.println(l_lng, 6);

    Serial.print("Crash of ");
    Serial.print(max_gforce);
    Serial.println("Gs");

    char TempString[32];  //  array de character temporario
    // dtostrf( [Float variable] , [Minimum SizeBeforePoint] , [sizeAfterPoint] , [WhereToStoreIt] )
    dtostrf(max_gforce, 2, 1, TempString);
    String gforce =  String(TempString);

    dtostrf(l_lat, 2, 6, TempString);
    String latstring =  String(TempString);

    dtostrf(l_lng, 2, 6, TempString);
    String lngstring =  String(TempString);

    // Prepara JSON para IOT Platform
    int length = 0;


    //String payload = "{\"d\":{\"umidade\":\"" + umidadestr + "\"}}";
    String payload =  "{\"gforce\": \""+gforce+"\", \"pos\": {\"latitute\":\"" +l_lat+ "\", \"longitude\":\""+l_lng+"\"}}";


    // length = payload.length();
    // Serial.print(F("\nData length"));
    // Serial.println(length);


    // Serial.print("Sending payload: ");
    // Serial.println(payload);


    if (client.publish(topic, (char*) payload.c_str())) {
      Serial.println("Publish ok");
      digitalWrite(LED_BUILTIN, LOW);
      delay(500);
      digitalWrite(LED_BUILTIN, HIGH);
      delay(500);
    } else {
      Serial.println("Publish failed");
      Serial.println(client.state());
    }
  }

  delay(10);
}

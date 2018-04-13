#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <keys.h>

#define DHTPIN 2
#define DHTTYPE DHT11

// create HTTP client so we can send requests to Adafruit.io
HTTPClient client;

const size_t bufferSize = JSON_OBJECT_SIZE(10) + 210;

const int PIN_MOTOR_PWM = 4;
const int PIN_MOTOR_PHASE = 0;
const int PIN_PELTIER_PWM = 16;
const int PIN_PELTIER_PHASE = 5;

DHT dht(DHTPIN, DHTTYPE);

// Adafruit_MotorShield motors = Adafruit_MotorShield();
// Adafruit_DCMotor *stir_motor = motors.getMotor(1);

void peltier_heat() {
  Serial.println("heating");
  digitalWrite(PIN_PELTIER_PHASE, 0);
  digitalWrite(PIN_PELTIER_PWM, 1);
}

void peltier_cool() {
  Serial.println("cooling");
  digitalWrite(PIN_PELTIER_PHASE, 1);
  digitalWrite(PIN_PELTIER_PWM, 1);
}

void setup() {

  Serial.begin(115200);

  // configure GPIO directions (motor, peltier, temp sense)
  pinMode(PIN_MOTOR_PWM, OUTPUT);
  pinMode(PIN_MOTOR_PHASE, OUTPUT);
  pinMode(PIN_PELTIER_PWM, OUTPUT);
  pinMode(PIN_PELTIER_PHASE, OUTPUT);

  // set up motor for clockwise rotation
  digitalWrite(5, 1);

  // ssid and password defined as macros in keys.h
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  // establish connection
  Serial.println("\nHello!");
  Serial.print("Connecting to network");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  Serial.println("Got connection!");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  dht.begin();

}

void loop() {

  // get temperature data
  client.begin("http://io.adafruit.com/api/v2/finestbean/feeds/temp-set/data/last");
  client.addHeader("X-AIO-Key", API_KEY);
  int rcode_temp = client.GET();
  String json_temp = client.getString();
  client.end();

  DynamicJsonBuffer json_buffer_temp(bufferSize);
  JsonObject& root_temp = json_buffer_temp.parseObject(json_temp);

  String value_raw_temp = root_temp["value"];
  float temp_set = value_raw_temp.toFloat();

  // DHT11 often fails to read correctly, so do a few
  float temp;
  for (int i=0; i<5; i++) {
    temp = dht.readTemperature(true);
    if (!isnan(temp)) break;
  }

  // control peltier
  Serial.println(temp_set);
  Serial.println(temp);
  Serial.println(temp < temp_set);
  if (!isnan(temp)) {
    if (temp < temp_set) peltier_heat();
    else peltier_cool();
  }

  // peltier_cool();

  // send temperature data
  client.begin("http://io.adafruit.com/api/v2/finestbean/feeds/temp/data");
  client.addHeader("Content-Type", "application/x-www-form-urlencoded");
  client.addHeader("X-AIO-Key", API_KEY);

  String payload = (String) "value=" + temp;
  int rcode_temp_send = client.POST(payload);
  // Serial.println(rcode_temp_send);
  // client.writeToStream(&Serial);
  client.end();


  // get motor speed setting
  client.begin("http://io.adafruit.com/api/v2/finestbean/feeds/motor/data/last");
  client.addHeader("X-AIO-Key", API_KEY);
  int rcode_motor = client.GET();
  String json_motor = client.getString();
  client.end();

  // process JSON, extract motor speed setting as int
  DynamicJsonBuffer json_buffer_motor(bufferSize);
  JsonObject& root_motor = json_buffer_motor.parseObject(json_motor);

  String value_raw_motor = root_motor["value"];
  float motor_set = value_raw_motor.toInt();

  analogWrite(PIN_MOTOR_PWM, map(motor_set, 0, 100, 0, 255));

  // throttle so we don't overload adafruit.io
  delay(3000);

}

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
const int STATUS_LED = 14;

DHT dht(DHTPIN, DHTTYPE);

float temp_set;
float motor_set;
float temp_lowpass;

int throttle_count;

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

void peltier_off() {
  Serial.println("off");
  // digitalWrite(PIN_PELTIER_PHASE, 1);
  digitalWrite(PIN_PELTIER_PWM, 0);
}

void setup() {

  Serial.begin(115200);

  // configure GPIO directions (motor, peltier, temp sense)
  pinMode(PIN_MOTOR_PWM, OUTPUT);
  pinMode(PIN_MOTOR_PHASE, OUTPUT);
  pinMode(PIN_PELTIER_PWM, OUTPUT);
  pinMode(PIN_PELTIER_PHASE, OUTPUT);
  pinMode(STATUS_LED, OUTPUT);

  // turn onff status led
  digitalWrite(STATUS_LED, 0);

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

  temp_set = 90;
  temp_lowpass = 0;
  for (int i=0; i<5; i++) {
    temp_lowpass = dht.readTemperature(true);
    if (!isnan(temp_lowpass)) break;
  }

  throttle_count = 3500;

}

void loop() {

  // DHT11 often fails to read correctly, so do a few reads
  float temp;
  for (int i=0; i<5; i++) {
    temp = dht.readTemperature(true);
    if (!isnan(temp)) break;
  }

  if (!isnan(temp)) {
    // get rid of high frequency noise
    temp_lowpass = 0.05 * temp_lowpass + 0.95 * temp;

    // control peltier (w/ h-bridge)
    Serial.printf ("%.2f, %.2f, %.2f \n", temp_set, temp, temp_lowpass);

    if (!isnan(temp)) {
      if (temp < temp_set - 1) {
        peltier_heat();
      } else if (temp > temp_set + 1) {
        peltier_cool();
      } else {
        peltier_off();
      }
    }
  }

  delay(10);
  throttle_count++;

  if (throttle_count > 350) {
    throttle_count = 0;

    // get temperature setting
    client.begin("http://io.adafruit.com/api/v2/finestbean/feeds/temp-set/data/last");
    client.addHeader("X-AIO-Key", API_KEY);
    int rcode_temp = client.GET();
    String json_temp = client.getString();
    client.end();

    DynamicJsonBuffer json_buffer_temp(bufferSize);
    JsonObject& root_temp = json_buffer_temp.parseObject(json_temp);

    String value_raw_temp = root_temp["value"];
    temp_set = value_raw_temp.toFloat();

    // send filtered temperature data
    client.begin("http://io.adafruit.com/api/v2/finestbean/feeds/temp/data");
    client.addHeader("Content-Type", "application/x-www-form-urlencoded");
    client.addHeader("X-AIO-Key", API_KEY);

    String payload = (String) "value=" + temp;
    int rcode_temp_send = client.POST(payload);
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

    if (motor_set >= 10) digitalWrite(STATUS_LED, 1);
    else digitalWrite(STATUS_LED, 0);
  }

}

#include <FS.h>
#include <Arduino.h>

#include <ESP8266WiFi.h>

// Config portal
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>

#include <Servo.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

////////////////////////////////////////////////////////////////////////////////
#define TCP_SERVER_PORT 23
#define TCP_SERVER_TIMEOUT 5000
#define LED_PIN D0

#define SERVO_PIN D2
#define SERVO_DEFAULT_POS 90
#define SERVO_MINIMUM_POS 60
#define SERVO_MAXIMUM_POS 120

#define MOTOR_L_SPEED_PIN D3
#define MOTOR_L_FORWARD_PIN D5
#define MOTOR_L_BACKWARD_PIN D6
#define MOTOR_R_SPEED_PIN D4
#define MOTOR_R_FORWARD_PIN D7
#define MOTOR_R_BACKWARD_PIN D8
#define MOTOR_MAXIMUM_SPEED 1023

#define WIFI_AP_SSID_DEFAULT "Hoalong-Esp-Config"
#define WIFI_AP_PASS_DEFAULT "nothing123"

#define MQTT_SERVER "broker.mqtt-dashboard.com"
#define MQTT_PORT 1883
#define MQTT_CLIENT_ID "hoalong/racing-car/esp8266"
#define MQTT_PUBLISH_CHANNEL "hoalong/racing-car/esp8266/ip"

////////////////////////////////////////////////////////////////////////////////
// pins configiguration
void configPins() {
  pinMode(MOTOR_L_SPEED_PIN, OUTPUT);
  analogWrite(MOTOR_L_SPEED_PIN, 0);

  pinMode(MOTOR_L_FORWARD_PIN, OUTPUT);
  digitalWrite(MOTOR_L_FORWARD_PIN, LOW);

  pinMode(MOTOR_L_BACKWARD_PIN, OUTPUT);
  digitalWrite(MOTOR_L_BACKWARD_PIN, LOW);

  pinMode(MOTOR_R_SPEED_PIN, OUTPUT);
  analogWrite(MOTOR_R_SPEED_PIN, 0);

  pinMode(MOTOR_R_FORWARD_PIN, OUTPUT);
  digitalWrite(MOTOR_R_FORWARD_PIN, LOW);

  pinMode(MOTOR_R_BACKWARD_PIN, OUTPUT);
  digitalWrite(MOTOR_R_BACKWARD_PIN, LOW);

  // initialize led
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
}

////////////////////////////////////////////////////////////////////////////////
// servo instance
Servo servo;
void configServo() {
  servo.attach(SERVO_PIN);
  servo.write(90);
  delay(15);
}

////////////////////////////////////////////////////////////////////////////////
// wifi manager
WiFiManager wifiManager;
void configWiFi() {
  //wifiManager.setSTAStaticIPConfig(IPAddress(10,10,10,10), IPAddress(10,10,10,1), IPAddress(255,255,255,0));
  wifiManager.setMinimumSignalQuality(20);
  wifiManager.setTimeout(300);

  if (!wifiManager.autoConnect(WIFI_AP_SSID_DEFAULT, WIFI_AP_PASS_DEFAULT)) {
    Serial.println("Failed to connect and hit timeout");
    delay(3000);
    //reset and try again
    ESP.reset();
    delay(5000);
  }

  Serial.println("WiFi connected.");
  Serial.print("Local IP: ");
  Serial.println(WiFi.localIP());
}

////////////////////////////////////////////////////////////////////////////////
// pubsub mqtt
WiFiClient espClient;
PubSubClient pubsubClient(espClient);

boolean retryPublishIp() {
  if (pubsubClient.connect(MQTT_CLIENT_ID)) {
      char localIp[20];
      WiFi.localIP().toString().toCharArray(localIp, 20);
      pubsubClient.publish(MQTT_PUBLISH_CHANNEL, localIp);
  }
  return pubsubClient.connected();
}

void configPubSub() {
  pubsubClient.setServer(MQTT_SERVER, MQTT_PORT);

  Serial.print("Attempting MQTT connection...");
  if (retryPublishIp()) {
    Serial.println("connected");
  } else {
    Serial.print("failed, rc = ");
    Serial.print(pubsubClient.state());
    Serial.println(" try again in 3 seconds");
    // Wait 3 seconds before retrying
  }
}

////////////////////////////////////////////////////////////////////////////////
// wifi tcp server
WiFiServer tcp_server(TCP_SERVER_PORT);
void configTCPServer() {
  tcp_server.begin();
  Serial.println("TCP Server started");
}

////////////////////////////////////////////////////////////////////////////////
// command control
// Accept from -90 to 90
void onServoEvent(String command) {
  int command_pos = command.toInt();
  int servo_pos = SERVO_DEFAULT_POS + command_pos;

  if(servo_pos > SERVO_MAXIMUM_POS) {
    servo_pos = SERVO_MAXIMUM_POS;
  } else if(servo_pos < SERVO_MINIMUM_POS) {
    servo_pos = SERVO_MINIMUM_POS;
  }
  servo.write(servo_pos);
  delay(15);
}

// Accept from -255 to 255
void onMotorEvent(String command) {
  int direction = servo.read();
  int d_speed_l = direction < 90 ? direction - 90 : 0;
  int d_speed_r = direction > 90 ? 90 - direction : 0;

  int speed = command.toInt();
  if (speed > MOTOR_MAXIMUM_SPEED) {
    speed = MOTOR_MAXIMUM_SPEED;
  } else if (speed < -MOTOR_MAXIMUM_SPEED) {
    speed = -MOTOR_MAXIMUM_SPEED;
  }

  if (speed > 5) {
    digitalWrite(MOTOR_L_FORWARD_PIN, HIGH);
    digitalWrite(MOTOR_L_BACKWARD_PIN, LOW);

    digitalWrite(MOTOR_R_FORWARD_PIN, HIGH);
    digitalWrite(MOTOR_R_BACKWARD_PIN, LOW);

    analogWrite(MOTOR_L_SPEED_PIN, speed);
    analogWrite(MOTOR_R_SPEED_PIN, speed);
  } else if(speed <= 5 && speed >= -5) {
    digitalWrite(MOTOR_L_FORWARD_PIN, LOW);
    digitalWrite(MOTOR_L_BACKWARD_PIN, LOW);

    digitalWrite(MOTOR_R_FORWARD_PIN, LOW);
    digitalWrite(MOTOR_R_BACKWARD_PIN, LOW);

    analogWrite(MOTOR_L_SPEED_PIN, 0);
    analogWrite(MOTOR_R_SPEED_PIN, 0);
  } else if(speed < -5) {
    digitalWrite(MOTOR_L_FORWARD_PIN, LOW);
    digitalWrite(MOTOR_L_BACKWARD_PIN, HIGH);

    digitalWrite(MOTOR_R_FORWARD_PIN, LOW);
    digitalWrite(MOTOR_R_BACKWARD_PIN, HIGH);

    analogWrite(MOTOR_L_SPEED_PIN, -speed);
    analogWrite(MOTOR_R_SPEED_PIN, -speed);
  }
}

void onRequest(String req) {
  if(req.startsWith("servo")) {
    String command = req.substring(5);
    command.trim();
    onServoEvent(command);
  } else if(req.startsWith("motor")) {
    String command = req.substring(5);
    command.trim();
    onMotorEvent(command);
  }
}

////////////////////////////////////////////////////////////////////////////////
// setup call
void setup()
{
  // initialize serial
  Serial.begin(115200);
  Serial.setDebugOutput(true);

  // initialiaze motor
  configPins();

  // initialize servo
  configServo();

  // initialize wifi
  configWiFi();

  // initialize pubsub
  configPubSub();

  // Start the server
  configTCPServer();
}

////////////////////////////////////////////////////////////////////////////////
// loop call
void loop()
{
  // Check if a client has connected
  WiFiClient client = tcp_server.available();
  if (!client) {
    retryPublishIp();
    delay(1000);
    return;
  }

  // Wait until the client sends some data
  int waited = 0;
  Serial.println("Client connected");
  digitalWrite(LED_PIN, HIGH);
  while(client.connected()){
    if(waited > TCP_SERVER_TIMEOUT) { break; /* Client idle */ }

    // Wait until client say somethings
    while(!client.available()) {
      delay(1);

      waited++;
      if(waited > TCP_SERVER_TIMEOUT) { break; /* Client idle */ }
    }

    if(client.available()) {
      waited = 0;

      // Read the first line of the request
      digitalWrite(LED_PIN, LOW);
      String req = client.readStringUntil('\r');
      onRequest(req);
      digitalWrite(LED_PIN, HIGH);
    }
  }

  client.flush();
  delay(1);
  Serial.println("Client disconnected");
  digitalWrite(LED_PIN, LOW);
}

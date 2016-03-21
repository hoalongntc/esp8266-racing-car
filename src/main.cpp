#include "Arduino.h"
#include "Servo.h"
#include "ESP8266WiFi.h"
#include "ArduinoJson.h"
#include "FS.h"

////////////////////////////////////////////////////////////////////////////////
#define TCP_SERVER_PORT 23
#define TCP_SERVER_TIMEOUT 5000
#define LED_PIN D0

#define SERVO_PIN D2
#define SERVO_DEFAULT_POS 90
#define SERVO_MINIMUM_POS 30
#define SERVO_MAXIMUM_POS 150

#define MOTOR_SPEED_PIN D3
#define MOTOR_FORWARD_PIN D5
#define MOTOR_BACKWARD_PIN D6
#define MOTOR_MAXIMUM_SPEED 255

#define CONFIG_WIFI_STA_MAX_TRY 10000
#define CONFIG_WIFI_STA_SSID_KEY "wifi_sta_ssid"
#define CONFIG_WIFI_STA_PASS_KEY "wifi_sta_password"

////////////////////////////////////////////////////////////////////////////////
// servo instance
Servo servo;

////////////////////////////////////////////////////////////////////////////////
// wifi config

void WiFiEvent(WiFiEvent_t event) {
    switch(event) {
        case WIFI_EVENT_STAMODE_GOT_IP:
            Serial.println("WiFi connected");
            Serial.println("IP address: ");
            Serial.println(WiFi.localIP());
            digitalWrite(LED_PIN, HIGH);
            break;
        case WIFI_EVENT_STAMODE_DISCONNECTED:
            Serial.println("WiFi lost connection");
            digitalWrite(LED_PIN, LOW);
            break;
    }
}

bool setupWiFiFromConfigFile() {
  File configFile = SPIFFS.open("/config.json", "r");
  if (!configFile) {
    Serial.println("Failed to open config file");
    return false;
  }

  size_t size = configFile.size();
  if (size > 1024) {
    Serial.println("Config file size is too large");
    return false;
  }

  // Allocate a buffer to store contents of the file.
  std::unique_ptr<char[]> buf(new char[size]);

  // We don't use String here because ArduinoJson library requires the input
  // buffer to be mutable. If you don't use ArduinoJson, you may as well
  // use configFile.readString instead.
  configFile.readBytes(buf.get(), size);

  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& json = jsonBuffer.parseObject(buf.get());

  if (!json.success()) {
    Serial.println("Failed to parse config file");
    return false;
  }

  if (!json.containsKey(CONFIG_WIFI_STA_KEY)) {
    Serial.println("Not config for station mode yet");
    return false;
  }

  const char* WIFI_SSID = json[CONFIG_WIFI_STA_KEY][CONFIG_WIFI_STA_SSID_KEY];
  const char* WIFI_PASS = json[CONFIG_WIFI_STA_KEY][CONFIG_WIFI_STA_PASS_KEY];

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  WiFi.mode(WIFI_STA);

  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);
  int waited = 0;
  while (WiFi.status() != WL_CONNECTED) {
    if(waited > CONFIG_WIFI_STA_MAX_TRY) {
      return false;
    }

    delay(500);
    waited += 500;
    Serial.print(".");
  }

  return true;
}

void setupWiFiForConfig() {

}

void setupWiFi() {
  // first, try to read config file and connect to setted up WiFi
  if(!setupWiFiFromConfigFile()) {
    setupWiFiForConfig();
  }
}

////////////////////////////////////////////////////////////////////////////////
// wifi tcp server
WiFiServer tcp_server(TCP_SERVER_PORT);

// Accept from -90 to 90
void onServoEvent(String command) {
  Serial.println("Servo command " + command);

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

void onMotorEvent(String command) {
  Serial.println("Motor command " + command);

  int speed = command.toInt();
  if (speed > MOTOR_MAXIMUM_SPEED) {
    speed = MOTOR_MAXIMUM_SPEED;
  } else if (speed < -MOTOR_MAXIMUM_SPEED) {
    speed = -MOTOR_MAXIMUM_SPEED;
  }
  if (speed > 5) {
    digitalWrite(MOTOR_FORWARD_PIN, HIGH);
    digitalWrite(MOTOR_BACKWARD_PIN, LOW);
    analogWrite(MOTOR_SPEED_PIN, speed);
  } else if(speed <= 5 && speed >= -5) {
    digitalWrite(MOTOR_FORWARD_PIN, LOW);
    digitalWrite(MOTOR_BACKWARD_PIN, LOW);
    analogWrite(MOTOR_SPEED_PIN, 0);
  } else if(speed < -5) {
    digitalWrite(MOTOR_FORWARD_PIN, LOW);
    digitalWrite(MOTOR_BACKWARD_PIN, HIGH);
    analogWrite(MOTOR_SPEED_PIN, -speed);
  }
}

////////////////////////////////////////////////////////////////////////////////
// setup call
void setup()
{
  // initialize serial
  Serial.begin(9600);

  // initialize led
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // initialiaze motor
  pinMode(MOTOR_SPEED_PIN, OUTPUT);
  pinMode(MOTOR_FORWARD_PIN, OUTPUT);
  pinMode(MOTOR_BACKWARD_PIN, OUTPUT);
  analogWrite(MOTOR_SPEED_PIN, 0);
  digitalWrite(MOTOR_FORWARD_PIN, LOW);
  digitalWrite(MOTOR_BACKWARD_PIN, LOW);




  Serial.println("Wait for WiFi... ");
  delay(2000);

  // initialize servo
  servo.attach(SERVO_PIN);
  servo.write(0);
  delay(15);

  // Start the server
  tcp_server.begin();
  Serial.println("TCP Server started");
}

////////////////////////////////////////////////////////////////////////////////
// loop call
void loop()
{
  // Check if a client has connected
  WiFiClient client = tcp_server.available();
  if (!client) {
    return;
  }

  // Wait until the client sends some data
  int waited = 0;
  Serial.println("Client connected");
  while(client.connected()){
    if(waited > TCP_SERVER_TIMEOUT) {
      break; // Client idle
    }

    // Wait until client say somethings
    while(!client.available()) {
      delay(1);

      waited++;
      if(waited > TCP_SERVER_TIMEOUT) {
        break; // Client idle
      }
    }

    if(client.available()) {
      waited = 0;

      // Read the first line of the request
      String req = client.readStringUntil('\r');
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
  }

  // Send the response to the client
  client.flush();
  delay(1);
  Serial.println("Client disonnected");
}

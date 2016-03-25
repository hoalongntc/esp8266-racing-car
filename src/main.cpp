#include "Arduino.h"
#include "Servo.h"
#include "ESP8266WiFi.h"

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

#define WIFI_STA_MAX_TRY 10000
// #define CONFIG_WIFI_STA_SSID_KEY "wifi_sta_ssid"
// #define CONFIG_WIFI_STA_PASS_KEY "wifi_sta_password"
// #define CONFIG_WIFI_AP_SSID_KEY "wifi_ap_ssid"
// #define CONFIG_WIFI_AP_PASS_KEY "wifi_ap_password"
#define WIFI_STA_SSID "DOU_Networks (SCS)"
#define WIFI_STA_PASS "DOU12345"
#define WIFI_AP_SSID_DEFAULT "Hoalong-Racing-Car"
#define WIFI_AP_PASS_DEFAULT "12345678!@#"

////////////////////////////////////////////////////////////////////////////////
// config.json file

// JsonObject& readConfigFileOrCreateInitConfigFileIfNeeded() {
//   StaticJsonBuffer<200> jsonBuffer;
//
//   File configFile;
//   if (SPIFFS.exists("/config.json")) {
//     Serial.println("Config file is already existed!");
//
//     configFile = SPIFFS.open("/config.json", "r");
//     size_t size = configFile.size();
//     if (size > 1024) {
//       // Recreate new init config file
//       Serial.println("Config file size is too large");
//     } else {
//       // Allocate a buffer to store contents of the file.
//       std::unique_ptr<char[]> buf(new char[size]);
//       configFile.readBytes(buf.get(), size);
//       JsonObject& config = jsonBuffer.parseObject(buf.get());
//       if (!config.success()) {
//         // Recreate new init config file
//         Serial.println("Failed to parse config file");
//       } else {
//         return config;
//       }
//     }
//   }
//
//   configFile = SPIFFS.open("/config.json", "w");
//   if (!configFile) {
//     Serial.println("Failed to open config file for writing");
//     return jsonBuffer.createObject();
//   }
//
//   JsonObject& initConfig = jsonBuffer.createObject();
//   initConfig[CONFIG_WIFI_AP_SSID_KEY] = CONFIG_WIFI_AP_SSID_DEFAULT;
//   initConfig[CONFIG_WIFI_AP_PASS_KEY] = CONFIG_WIFI_AP_PASS_DEFAULT;
//
//   initConfig.printTo(configFile);
//   return initConfig;
// }

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

bool setupWiFiStationMode(const char* ssid, const char* password) {
  WiFi.begin(ssid, password);
  WiFi.onEvent(WiFiEvent);

  Serial.print("Connecting to ");
  Serial.println(ssid);

  int waited = 0;
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");

    if(waited > WIFI_STA_MAX_TRY) {
      return false;
    }

    delay(500);
    waited += 500;
  }

  return true;
}

bool setupWiFiConfigMode(const char* ssid, const char* password) {
  WiFi.softAP(ssid, password);

  IPAddress myIP = WiFi.softAPIP();
	Serial.print("AP IP address: ");
	Serial.println(myIP);

  return true;
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

// Accept from -255 to 255
void onMotorEvent(String command) {
  Serial.println("Motor command " + command);

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
  // initialiaze motor
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

  // initialize serial
  Serial.begin(9600);

  // initialize servo
  servo.attach(SERVO_PIN);
  servo.write(90);
  delay(15);

  // setup wifi
  setupWiFiStationMode(WIFI_STA_SSID, WIFI_STA_PASS);
  delay(2000);

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
      onRequest(req);
    }
  }

  // Send the response to the client
  client.flush();
  delay(1);
  Serial.println("Client disonnected");
}

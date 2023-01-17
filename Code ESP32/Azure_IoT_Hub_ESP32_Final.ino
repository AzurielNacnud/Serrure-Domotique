// Copyright (c) Microsoft Corporation. All rights reserved.
// SPDX-License-Identifier: MIT

/*
 * This is an Arduino-based Azure IoT Hub sample for ESPRESSIF ESP32 boards.
 * It uses our Azure Embedded SDK for C to help interact with Azure IoT.
 * For reference, please visit https://github.com/azure/azure-sdk-for-c.
 *
 * To connect and work with Azure IoT Hub you need an MQTT client, connecting, subscribing
 * and publishing to specific topics to use the messaging features of the hub.
 * Our azure-sdk-for-c is an MQTT client support library, helping composing and parsing the
 * MQTT topic names and messages exchanged with the Azure IoT Hub.
 *
 * This sample performs the following tasks:
 * - Synchronize the device clock with a NTP server;
 * - Initialize our "az_iot_hub_client" (struct for data, part of our azure-sdk-for-c);
 * - Initialize the MQTT client (here we use ESPRESSIF's esp_mqtt_client, which also handle the tcp
 * connection and TLS);
 * - Connect the MQTT client (using server-certificate validation, SAS-tokens for client
 * authentication);
 * - Periodically send telemetry data to the Azure IoT Hub.
 *
 * To properly connect to your Azure IoT Hub, please fill the information in the `iot_configs.h`
 * file.
 */

// C99 libraries
#include <cstdlib>
#include <string.h>
#include <time.h>

// Libraries for MQTT client and WiFi connection
#include <WiFi.h>
#include <mqtt_client.h>

// Azure IoT SDK for C includes
#include <az_core.h>
#include <az_iot.h>
#include <azure_ca.h>

// Additional sample headers
#include "AzIoTSasToken.h"
#include "SerialLogger.h"
#include "iot_configs.h"

#include <Wire.h>
#include <Keypad.h>
#include <LiquidCrystal_I2C.h>

// When developing for your own Arduino-based platform,
// please follow the format '(ard;<platform>)'.
#define AZURE_SDK_CLIENT_USER_AGENT "c%2F" AZ_SDK_VERSION_STRING "(ard;esp32)"

// Utility macros and defines
#define sizeofarray(a) (sizeof(a) / sizeof(a[0]))
#define NTP_SERVERS "pool.ntp.org", "time.nist.gov"
#define MQTT_QOS1 1
#define DO_NOT_RETAIN_MSG 0
#define SAS_TOKEN_DURATION_IN_MINUTES 60
#define UNIX_TIME_NOV_13_2017 1510592825

#define PST_TIME_ZONE 1
#define PST_TIME_ZONE_DAYLIGHT_SAVINGS_DIFF 1

#define GMT_OFFSET_SECS (PST_TIME_ZONE * 3600)
#define GMT_OFFSET_SECS_DST ((PST_TIME_ZONE + PST_TIME_ZONE_DAYLIGHT_SAVINGS_DIFF) * 3600)

#define redLightPin 5
#define greenLightPin 18
#define blueLightPin 19

#define R_Channel 0
#define G_Channel 1
#define B_Channel 2

#define pwm_Frequency 5000
#define pwm_Reslotuion 8

#define relayPin 13

// Translate iot_configs.h defines into variables used by the sample
static const char* ssid = IOT_CONFIG_WIFI_SSID;
static const char* password = IOT_CONFIG_WIFI_PASSWORD;
static const char* host = IOT_CONFIG_IOTHUB_FQDN;
static const char* mqtt_broker_uri = "mqtts://" IOT_CONFIG_IOTHUB_FQDN;
static const char* device_id = IOT_CONFIG_DEVICE_ID;
static const int mqtt_port = AZ_IOT_DEFAULT_MQTT_CONNECT_PORT;

// Memory allocated for the sample's variables and structures.
static esp_mqtt_client_handle_t mqtt_client;
static az_iot_hub_client client;

static char mqtt_client_id[128];
static char mqtt_username[128];
static char mqtt_password[200];
static uint8_t sas_signature_buffer[256];
static unsigned long next_telemetry_send_time_ms = 0;
static char telemetry_topic[128];
static uint8_t telemetry_payload[100];
static uint32_t telemetry_send_count = 0;

// Variables

String adminPwd = "9991";
//String pwd1 = "7812", pwd2 = "9023", pwd3 = "1832", pwd4 = "3674", pwd5 = "2874", pwd6 = "9384", pwd7 = "4221", pwd8 = "4287", pwd9 = "9212", pwd10 = "2242";
String passwords[100] = { "7812" };
String enteredPwd, userChoice = "";
int attenteMiseEnVeille = 0;
int count = 0;

bool lastState = -1;
int essais = 3;

bool isCorrectPwd = false;

char keys[4][4] = {
  { '1', '2', '3', 'A' },
  { '4', '5', '6', 'B' },
  { '7', '8', '9', 'C' },
  { '*', '0', '#', 'D' }
};

byte pinsRangees[4] = { 12, 14, 27, 26 };
byte pinsColonnes[4] = { 25, 33, 32, 35 };

// Senseurs
LiquidCrystal_I2C lcd(0x27, 20, 4);
Keypad keypad = Keypad(makeKeymap(keys), pinsRangees, pinsColonnes, 4, 4);

#define INCOMING_DATA_BUFFER_SIZE 128
static char incoming_data[INCOMING_DATA_BUFFER_SIZE];

// Auxiliary functions
#ifndef IOT_CONFIG_USE_X509_CERT
static AzIoTSasToken sasToken(
  &client,
  AZ_SPAN_FROM_STR(IOT_CONFIG_DEVICE_KEY),
  AZ_SPAN_FROM_BUFFER(sas_signature_buffer),
  AZ_SPAN_FROM_BUFFER(mqtt_password));
#endif  // IOT_CONFIG_USE_X509_CERT

static void connectToWiFi() {
  Logger.Info("Connecting to WIFI SSID " + String(ssid));

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");

  Logger.Info("WiFi connected, IP address: " + WiFi.localIP().toString());
}

static void initializeTime() {
  Logger.Info("Setting time using SNTP");

  configTime(GMT_OFFSET_SECS, GMT_OFFSET_SECS_DST, NTP_SERVERS);
  time_t now = time(NULL);
  while (now < UNIX_TIME_NOV_13_2017) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println("");
  Logger.Info("Time initialized!");
}

void receivedCallback(char* topic, byte* payload, unsigned int length) {
  Logger.Info("Received [");
  Logger.Info(topic);
  Logger.Info("]: ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println("");
}

static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event) {
  switch (event->event_id) {
    int i, r;

    case MQTT_EVENT_ERROR:
      Logger.Info("MQTT event MQTT_EVENT_ERROR");
      break;
    case MQTT_EVENT_CONNECTED:
      Logger.Info("MQTT event MQTT_EVENT_CONNECTED");

      r = esp_mqtt_client_subscribe(mqtt_client, AZ_IOT_HUB_CLIENT_C2D_SUBSCRIBE_TOPIC, 1);
      if (r == -1) {
        Logger.Error("Could not subscribe for cloud-to-device messages.");
      } else {
        Logger.Info("Subscribed for cloud-to-device messages; message id:" + String(r));
      }

      break;
    case MQTT_EVENT_DISCONNECTED:
      Logger.Info("MQTT event MQTT_EVENT_DISCONNECTED");
      break;
    case MQTT_EVENT_SUBSCRIBED:
      Logger.Info("MQTT event MQTT_EVENT_SUBSCRIBED");
      break;
    case MQTT_EVENT_UNSUBSCRIBED:
      Logger.Info("MQTT event MQTT_EVENT_UNSUBSCRIBED");
      break;
    case MQTT_EVENT_PUBLISHED:
      Logger.Info("MQTT event MQTT_EVENT_PUBLISHED");
      break;
    case MQTT_EVENT_DATA:
      {
        Logger.Info("MQTT event MQTT_EVENT_DATA");

        for (i = 0; i < (INCOMING_DATA_BUFFER_SIZE - 1) && i < event->topic_len; i++) {
          incoming_data[i] = event->topic[i];
        }
        incoming_data[i] = '\0';
        Logger.Info("Topic: " + String(incoming_data));

        for (i = 0; i < (INCOMING_DATA_BUFFER_SIZE - 1) && i < event->data_len; i++) {
          incoming_data[i] = event->data[i];
        }
        incoming_data[i] = '\0';
        Logger.Info("Data: " + String(incoming_data));

        char* strings[100];
        char* ptr = NULL;

        byte index = 0;
        ptr = strtok(incoming_data, ",");
        while (ptr != NULL) {
          strings[index] = ptr;
          index++;
          ptr = strtok(NULL, ",");
        }

        String function_name = strings[0];

        if (function_name == "changePwds") {
          for (int i = 0; i < 100; i++){
            passwords[i] = "";
          }

          for (int i = 1; i < index; i++){
            passwords[i] = strings[i];
          }
          Serial.println("Passwords change");
        }

        break;
      }
    case MQTT_EVENT_BEFORE_CONNECT:
      Logger.Info("MQTT event MQTT_EVENT_BEFORE_CONNECT");
      break;
    default:
      Logger.Error("MQTT event UNKNOWN");
      break;
  }

  return ESP_OK;
}

static void initializeIoTHubClient() {
  az_iot_hub_client_options options = az_iot_hub_client_options_default();
  options.user_agent = AZ_SPAN_FROM_STR(AZURE_SDK_CLIENT_USER_AGENT);

  if (az_result_failed(az_iot_hub_client_init(
        &client,
        az_span_create((uint8_t*)host, strlen(host)),
        az_span_create((uint8_t*)device_id, strlen(device_id)),
        &options))) {
    Logger.Error("Failed initializing Azure IoT Hub client");
    return;
  }

  size_t client_id_length;
  if (az_result_failed(az_iot_hub_client_get_client_id(
        &client, mqtt_client_id, sizeof(mqtt_client_id) - 1, &client_id_length))) {
    Logger.Error("Failed getting client id");
    return;
  }

  if (az_result_failed(az_iot_hub_client_get_user_name(
        &client, mqtt_username, sizeofarray(mqtt_username), NULL))) {
    Logger.Error("Failed to get MQTT clientId, return code");
    return;
  }

  Logger.Info("Client ID: " + String(mqtt_client_id));
  Logger.Info("Username: " + String(mqtt_username));
}

static int initializeMqttClient() {
#ifndef IOT_CONFIG_USE_X509_CERT
  if (sasToken.Generate(SAS_TOKEN_DURATION_IN_MINUTES) != 0) {
    Logger.Error("Failed generating SAS token");
    return 1;
  }
#endif

  esp_mqtt_client_config_t mqtt_config;
  memset(&mqtt_config, 0, sizeof(mqtt_config));
  mqtt_config.uri = mqtt_broker_uri;
  mqtt_config.port = mqtt_port;
  mqtt_config.client_id = mqtt_client_id;
  mqtt_config.username = mqtt_username;

#ifdef IOT_CONFIG_USE_X509_CERT
  Logger.Info("MQTT client using X509 Certificate authentication");
  mqtt_config.client_cert_pem = IOT_CONFIG_DEVICE_CERT;
  mqtt_config.client_key_pem = IOT_CONFIG_DEVICE_CERT_PRIVATE_KEY;
#else  // Using SAS key
  mqtt_config.password = (const char*)az_span_ptr(sasToken.Get());
#endif

  mqtt_config.keepalive = 30;
  mqtt_config.disable_clean_session = 0;
  mqtt_config.disable_auto_reconnect = false;
  mqtt_config.event_handle = mqtt_event_handler;
  mqtt_config.user_context = NULL;
  mqtt_config.cert_pem = (const char*)ca_pem;

  mqtt_client = esp_mqtt_client_init(&mqtt_config);

  if (mqtt_client == NULL) {
    Logger.Error("Failed creating mqtt client");
    return 1;
  }

  esp_err_t start_result = esp_mqtt_client_start(mqtt_client);

  if (start_result != ESP_OK) {
    Logger.Error("Could not start mqtt client; error code:" + start_result);
    return 1;
  } else {
    Logger.Info("MQTT client started");
    return 0;
  }
}

/*
 * @brief           Gets the number of seconds since UNIX epoch until now.
 * @return uint32_t Number of seconds.
 */
static uint32_t getEpochTimeInSecs() {
  return (uint32_t)time(NULL);
}

static void establishConnection() {
  connectToWiFi();
  initializeTime();
  initializeIoTHubClient();
  (void)initializeMqttClient();
}

static void getTelemetryPayload(az_span payload, az_span* out_payload) {
  az_span original_payload = payload;

  long code = enteredPwd.toInt();

  time_t now = time(nullptr);
  tm* local = localtime(&now);

  int day = local->tm_mday;
  int month = local->tm_mon;
  int year = local->tm_year;
  int hour = local->tm_hour;
  int minute = local->tm_min;

  int valuePwd =  (isCorrectPwd == true ? 1 : 0);

  payload = az_span_copy(payload, AZ_SPAN_FROM_STR("{ \"code\": "));
  (void)az_span_u32toa(payload, code, &payload);
  payload = az_span_copy(payload, AZ_SPAN_FROM_STR(" ,"));
  
  payload = az_span_copy(payload, AZ_SPAN_FROM_STR(" \"state\": "));
  (void)az_span_u32toa(payload, valuePwd, &payload);
  payload = az_span_copy(payload, AZ_SPAN_FROM_STR(" ,"));

  payload = az_span_copy(payload, AZ_SPAN_FROM_STR(" \"day\": "));
  (void)az_span_u32toa(payload, day, &payload);
  payload = az_span_copy(payload, AZ_SPAN_FROM_STR(" ,"));

  payload = az_span_copy(payload, AZ_SPAN_FROM_STR(" \"month\": "));
  (void)az_span_u32toa(payload, month + 1, &payload);
  payload = az_span_copy(payload, AZ_SPAN_FROM_STR(" ,"));

  payload = az_span_copy(payload, AZ_SPAN_FROM_STR(" \"year\": "));
  (void)az_span_u32toa(payload, year, &payload);
  payload = az_span_copy(payload, AZ_SPAN_FROM_STR(" ,"));

  payload = az_span_copy(payload, AZ_SPAN_FROM_STR(" \"hour\": "));
  (void)az_span_u32toa(payload, hour, &payload);
  payload = az_span_copy(payload, AZ_SPAN_FROM_STR(" ,"));

  payload = az_span_copy(payload, AZ_SPAN_FROM_STR(" \"minute\": "));
  (void)az_span_u32toa(payload, minute, &payload);
  payload = az_span_copy(payload, AZ_SPAN_FROM_STR(" }"));

  payload = az_span_copy_u8(payload, '\0');

  *out_payload = az_span_slice(
    original_payload, 0, az_span_size(original_payload) - az_span_size(payload) - 1);
}

static void sendTelemetry() {
  az_span telemetry = AZ_SPAN_FROM_BUFFER(telemetry_payload);

  Logger.Info("Sending telemetry ...");

  // The topic could be obtained just once during setup,
  // however if properties are used the topic need to be generated again to reflect the
  // current values of the properties.
  if (az_result_failed(az_iot_hub_client_telemetry_get_publish_topic(
        &client, NULL, telemetry_topic, sizeof(telemetry_topic), NULL))) {
    Logger.Error("Failed az_iot_hub_client_telemetry_get_publish_topic");
    return;
  }

  getTelemetryPayload(telemetry, &telemetry);

  if (esp_mqtt_client_publish(
        mqtt_client,
        telemetry_topic,
        (const char*)az_span_ptr(telemetry),
        az_span_size(telemetry),
        MQTT_QOS1,
        DO_NOT_RETAIN_MSG)
      == 0) {
    Logger.Error("Failed publishing");
  } else {
    Logger.Info("Message published successfully");
  }
}

// Arduino setup and loop main functions.

void setup() {
  ledcAttachPin(redLightPin, R_Channel);
  ledcAttachPin(greenLightPin, G_Channel);
  ledcAttachPin(blueLightPin, B_Channel);
  ledcSetup(R_Channel, pwm_Frequency, pwm_Reslotuion);
  ledcSetup(G_Channel, pwm_Frequency, pwm_Reslotuion);
  ledcSetup(B_Channel, pwm_Frequency, pwm_Reslotuion);

  pinMode(relayPin, OUTPUT);
  closeLock();

  lcd.begin();
  lcd.noBacklight();

  establishConnection();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectToWiFi();
  }
#ifndef IOT_CONFIG_USE_X509_CERT
  else if (sasToken.IsExpired()) {
    Logger.Info("SAS token expired; reconnecting with a new one.");
    (void)esp_mqtt_client_destroy(mqtt_client);
    initializeMqttClient();
  }
#endif
  else {
    char key = keypad.getKey();

    if (key) {
      mainScreen();
      waitingForUserEntry();
    }

    if (!key) {
      sleepScreen();
      delay(100);
    }
  }
}



void RGB_Red() {
  RGB_Color(255, 0, 0);
}

void RGB_Green() {
  RGB_Color(0, 255, 0);
}

void RGB_Blue() {
  RGB_Color(0, 0, 255);
}

void RGB_Input() {
  RGB_Color(0, 100, 100);
  delay(50);
  RGB_Color(0, 0, 0);
}

void RGB_exhaust() {
  RGB_Color(0, 0, 0);
}

void RGB_Color(int redLightValue, int greenLightValue, int blueLightValue) {
  ledcWrite(R_Channel, redLightValue);
  ledcWrite(G_Channel, greenLightValue);
  ledcWrite(B_Channel, blueLightValue);
}

void waitingForUserEntry() {
  while (attenteMiseEnVeille != 101) {
    char key = keypad.getKey();
    if (key) {
      attenteMiseEnVeille = 0;
      char userEntry = key;

      if (userEntry == '1' || userEntry == '2' || userEntry == '3' || userEntry == '4' || userEntry == '5' || userEntry == '6' || userEntry == '7' || userEntry == '8' || userEntry == '9' || userEntry == '0') {
        enteringPwdScreen();
        enteredPwd = enteredPwd + userEntry;
        enteringPwdScreen();

        if (checkIfPassWordIsKnown()) {
          openLock();
          correctPwdScreen();
          RGB_Green();
          isCorrectPwd = true;          
          sendTelemetry();
          enteredPwd = "";
          delay(5000);
          RGB_exhaust();
          closeLock();
          return;
        } else {
          incorrectPwdScreen();
          RGB_Red();
          isCorrectPwd = false;
          sendTelemetry();
          enteredPwd = "";
          delay(2500);
          RGB_exhaust();
          return;
        }
      }
    }

    if (!key && attenteMiseEnVeille == 60) {
      Serial.println("Mise en Veille");
      attenteMiseEnVeille = 0;
      return;
    }
    attenteMiseEnVeille++;
    delay(100);
  }
}

bool askAdminPwd() {
  askForAdminPwdScreen();
  askAdminPwdEntry();
  if (checkIfAdminPassWordIsRight()) {
    return true;
  } else return false;
}

void askUserEntry() {
  char userEntry = keypad.getKey();

  if (userEntry == '1') {
    userChoice = "1";
    enteringUserEntryScreen();
    return;
  }
  if (userEntry == '2') {
    userChoice = "2";
    enteringUserEntryScreen();
    return;
  }
  if (userEntry == '3') {
    userChoice = "3";
    enteringUserEntryScreen();
    return;
  }
  if (userEntry == '4') {
    userChoice = "4";
    enteringUserEntryScreen();
    return;
  }
  if (userEntry == '5') {
    userChoice = "5";
    enteringUserEntryScreen();
    return;
  }
  if (userEntry == '6') {
    userChoice = "6";
    enteringUserEntryScreen();
    return;
  }
  if (userEntry == '7') {
    userChoice = "7";
    enteringUserEntryScreen();
    return;
  }
  if (userEntry == '8') {
    userChoice = "8";
    enteringUserEntryScreen();
    return;
  }
  if (userEntry == '9') {
    userChoice = "9";
    enteringUserEntryScreen();
    return;
  }
  if (userEntry == '0') {
    userChoice = "0";
    enteringUserEntryScreen();
    return;
  }
}


void askPwdEntry() {
  while (enteredPwd.length() <= 3) {
    char userEntry = keypad.getKey();

    if (userEntry == '1') {
      enteredPwd = enteredPwd + "1";
      enteringPwdScreen();
      RGB_Input();
      continue;
    }
    if (userEntry == '2') {
      enteredPwd = enteredPwd + "2";
      enteringPwdScreen();
      RGB_Input();
      continue;
    }
    if (userEntry == '3') {
      enteredPwd = enteredPwd + "3";
      enteringPwdScreen();
      RGB_Input();
      continue;
    }
    if (userEntry == '4') {
      enteredPwd = enteredPwd + "4";
      enteringPwdScreen();
      RGB_Input();
      continue;
    }
    if (userEntry == '5') {
      enteredPwd = enteredPwd + "5";
      enteringPwdScreen();
      RGB_Input();
      continue;
    }
    if (userEntry == '6') {
      enteredPwd = enteredPwd + "6";
      enteringPwdScreen();
      RGB_Input();
      continue;
    }
    if (userEntry == '7') {
      enteredPwd = enteredPwd + "7";
      enteringPwdScreen();
      RGB_Input();
      continue;
    }
    if (userEntry == '8') {
      enteredPwd = enteredPwd + "8";
      enteringPwdScreen();
      RGB_Input();
      continue;
    }
    if (userEntry == '9') {
      enteredPwd = enteredPwd + "9";
      enteringPwdScreen();
      RGB_Input();
      continue;
    }
    if (userEntry == '0') {
      enteredPwd = enteredPwd + "0";
      enteringPwdScreen();
      RGB_Input();
      continue;
    }
  }
}

void askAdminPwdEntry() {
  while (enteredPwd.length() <= 3) {
    char userEntry = keypad.getKey();

    if (userEntry == '1') {
      enteredPwd = enteredPwd + "1";
      enteringAdminPwdScreen();
      RGB_Input();
      continue;
    }
    if (userEntry == '2') {
      enteredPwd = enteredPwd + "2";
      enteringAdminPwdScreen();
      RGB_Input();
      continue;
    }
    if (userEntry == '3') {
      enteredPwd = enteredPwd + "3";
      enteringAdminPwdScreen();
      RGB_Input();
      continue;
    }
    if (userEntry == '4') {
      enteredPwd = enteredPwd + "4";
      enteringAdminPwdScreen();
      RGB_Input();
      continue;
    }
    if (userEntry == '5') {
      enteredPwd = enteredPwd + "5";
      enteringAdminPwdScreen();
      RGB_Input();
      continue;
    }
    if (userEntry == '6') {
      enteredPwd = enteredPwd + "6";
      enteringAdminPwdScreen();
      RGB_Input();
      continue;
    }
    if (userEntry == '7') {
      enteredPwd = enteredPwd + "7";
      enteringAdminPwdScreen();
      RGB_Input();
      continue;
    }
    if (userEntry == '8') {
      enteredPwd = enteredPwd + "8";
      enteringAdminPwdScreen();
      RGB_Input();
      continue;
    }
    if (userEntry == '9') {
      enteredPwd = enteredPwd + "9";
      enteringAdminPwdScreen();
      RGB_Input();
      continue;
    }
    if (userEntry == '0') {
      enteredPwd = enteredPwd + "0";
      enteringAdminPwdScreen();
      RGB_Input();
      continue;
    }
  }
}

bool checkIfAdminPassWordIsRight() {
  askAdminPwdEntry();
  if (enteredPwd == adminPwd) {
    correctAdminPwdScreen();
    enteredPwd = "";
    return true;
  } else incorrectAdminPwdScreen();
  delay(1000);
  enteredPwd = "";
  return false;
}

bool checkIfPassWordIsKnown() {
  askPwdEntry();

  for (int i = 0; i < 100; i++) {
    //if (passwords[i] == null) break;
    if (enteredPwd == passwords[i]) {
      Serial.println("Code connu");
      return true;
    }
  }

  /*if (enteredPwd == pwd1 || enteredPwd == pwd2 || enteredPwd == pwd3 || enteredPwd == pwd4 || enteredPwd == pwd5 || enteredPwd == pwd6 || enteredPwd == pwd7 || enteredPwd == pwd8 || enteredPwd == pwd9 || enteredPwd == pwd10) {
    Serial.println("Code connu");
    return true;
  }*/
  Serial.println("Code inconnu");
  return false;
}

void sleepScreen() {
  lcd.clear();
  lcd.noBacklight();
}

void mainScreen() {
  lcd.clear();
  lcd.backlight();
  lcd.setCursor(1, 1);
  lcd.print("A AJOUTER DOIGT");
  lcd.setCursor(1, 2);
  lcd.print("D EMPREINTE");
}

void incorrectAdminPwdScreen() {
  lcd.clear();
  lcd.setCursor(1, 0);
  lcd.print("LE MOT DE PASSE DE ");
  lcd.setCursor(3, 1);
  lcd.print("VERIFICATION");
  lcd.setCursor(2, 2);
  lcd.print("ADMINISTRATEUR");
  lcd.setCursor(3, 3);
  lcd.print("EST INCORRECT.");
}

void askForAdminPwdScreen() {
  lcd.clear();
  lcd.setCursor(1, 0);
  lcd.print("VEUILLEZ ENTRER LE");
  lcd.setCursor(1, 1);
  lcd.print("MOT DE PASSE ADMIN");
}

void correctAdminPwdScreen() {
  lcd.clear();
  lcd.setCursor(2, 1);
  lcd.print("CODE ADMIN BON");
  delay(2000);
}

void correctPwdScreen() {
  lcd.clear();
  lcd.setCursor(5, 1);
  lcd.print("CODE CONNU");
}

void incorrectPwdScreen() {
  lcd.clear();
  lcd.setCursor(2, 1);
  lcd.print("CODE NON VALIDE");
}

void enteringPwdScreen() {
  lcd.clear();
  lcd.setCursor(4, 0);
  lcd.print("ENTREE CODE");
  lcd.setCursor(7, 1);
  lcd.print(enteredPwd);
  if (enteredPwd.length() == 4) {
    delay(1000);
  }
}

void enteringAdminPwdScreen() {
  lcd.clear();
  lcd.setCursor(4, 0);
  lcd.print("ENTREE CODE");
  lcd.setCursor(2, 1);
  lcd.print("ADMINISTRATEUR");
  lcd.setCursor(7, 2);
  lcd.print(enteredPwd);
  if (enteredPwd.length() == 4) {
    delay(1000);
  }
}

void enteringUserEntryScreen() {
  lcd.setCursor(8, 3);
  lcd.print(userChoice);
}

void openLock() {
  digitalWrite(relayPin, HIGH);
}

void closeLock() {
  digitalWrite(relayPin, LOW);
}

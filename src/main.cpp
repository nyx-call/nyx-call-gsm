#include <Arduino.h>
#include <ArduinoJson.h>
#include <TinyGsmClient.h>
#include <PubSubClient.h>
#include <params.h>

String mqttClientId;

long lastHeartBeat = 0;

String lastShiftNumber;
String lastShiftHolder;

FwStatus fwStatus;

TinyGsm modem(Serial1);
TinyGsmClient client(modem);

PubSubClient nyxCallMqtt(client); // nyx-call broker
PubSubClient nyxCallGsmMqtt(client); // nyx-call-gsm broker


void initGsm();
bool connect();
bool connectInternet();
bool connectNyxCallMqtt();
bool connectNyxCallGsmMqtt();
bool connected();
void processNyxCallQueue();
void heartBeat();
// void checkCalls();
void checkMessages();


void nyxCallMqttCallback(char* topic, byte* payload, unsigned int len);
void nyxCallGsmMqttCallback(char* topic, byte* payload, unsigned int len);

void setup() {
  // Serial.begin(115200);
  // while (!Serial) {
  //   ; // wait for serial port to connect. Needed for native USB port only
  // }

  initGsm();
  connect();
}

void loop() {
  if (connected()) {
    heartBeat();
    processNyxCallQueue();
    // checkCalls();
    // checkMessages();
  } else {
    delay(5000L);
    connect();
  }

  delay(1000L);
}

void initGsm() {
  Serial1.begin(115200);
  delay(3000);

  modem.init();

  if (!modem.waitForNetwork()) {
    while (true);
  }

  fwStatus = modem.getForwardingStatus(FW_NO_REPLY);
}

bool connectInternet() {
  return modem.gprsConnect(APN_NAME, APN_USER, APN_PASS);
}

bool connectNyxCallMqtt() {
  nyxCallMqtt.setServer(NYX_CALL_MQTT_SERVER, 1883);
  nyxCallMqtt.setCallback(nyxCallMqttCallback);
  if (nyxCallMqtt.connect("GsmClientTest", NYX_CALL_MQTT_USER, NYX_CALL_MQTT_PASS)) {
    // subscribe to interested events
    nyxCallMqtt.subscribe("nyxCall/shiftStarted");
    nyxCallMqtt.subscribe("nyxCall/shiftEnded");
  }

  return nyxCallMqtt.connected();
}

bool connectNyxCallGsmMqtt() {
  nyxCallGsmMqtt.setServer(NYX_CALL_GSM_MQTT_SERVER, 1883);
  nyxCallGsmMqtt.setCallback(nyxCallGsmMqttCallback);

  return nyxCallGsmMqtt.connect("GsmClientTest", NYX_CALL_GSM_MQTT_USER, NYX_CALL_GSM_MQTT_PASS);
}

bool connected() {
  return nyxCallGsmMqtt.connected() && nyxCallMqtt.connected();
}

bool connect()
{
  return connectInternet() && connectNyxCallGsmMqtt() && connectNyxCallMqtt();
}

void heartBeat() {
  long now = millis();

  if ((0 != lastHeartBeat) && (now - lastHeartBeat < 10000L)) {
    return;
  }

  DynamicJsonBuffer jsonBuffer;
  JsonObject& heartBeatJson = jsonBuffer.createObject();

  heartBeatJson["imei"] = modem.getIMEI();
  heartBeatJson["forwardEnabled"] = fwStatus.enabled;
  heartBeatJson["forwardNumber"] = fwStatus.number;

  String messageString;
  heartBeatJson.printTo(messageString);
  char message[messageString.length()+1];
  messageString.toCharArray(message, messageString.length()+1);
  if (nyxCallGsmMqtt.publish("nyxCallGsm/heartBeat", message)) {
    lastHeartBeat = now;
  }
}

void processNyxCallQueue() {
  nyxCallMqtt.loop();
}

void checkMessages() {

}

void checkCalls() {

}

void nyxCallMqttCallback(char* topic, byte* payload, unsigned int len) {
  String strTopic = String(topic);
  DynamicJsonBuffer jsonBuffer;
  JsonObject& payloadJson = jsonBuffer.parseObject(payload);

  if (strTopic == "nyxCall/shiftStarted") {

    String number = payloadJson["number"];
    String holder = payloadJson["holder"];

    modem.registerForwarding(FW_NO_REPLY, number, 5);
    fwStatus = modem.getForwardingStatus(FW_NO_REPLY);

    if (!number.equals(lastShiftNumber)) {
      modem.sendSMS(number, "Hello " + holder + ", this is reminder that you on-call shift has started.");
      modem.sendSMS(lastShiftNumber, "Hello " + lastShiftHolder + ", you on call shift is over.");
      lastShiftNumber = number;
      lastShiftHolder = holder;
    }
  }

  if (strTopic == "nyxCall/shiftEnded") {
    modem.eraseForwarding(FW_NO_REPLY);
    fwStatus = modem.getForwardingStatus(FW_NO_REPLY);
  }
}

void nyxCallGsmMqttCallback(char* topic, byte* payload, unsigned int len) {
  // we are not interested in our event
}

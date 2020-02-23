#include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson
#include <SPI.h>
#include <ATM90E36.h>
#include <PubSubClient.h>

//define your default values here, if there are different values in config.json, they are overwritten.
char monitor_name[40] = "EnergyMonitor";
char mqtt_server[40];
char mqtt_port[6] = "8080";
char line_freq[6] = "4485";
char pga_gain[6] = "21";
char voltage_gain[6] = "29462";
char current_gain_st1[6] = "25498";
char current_gain_st2[6] = "25498";
char current_gain_st3[6] = "25498";
char current_gain_st4[6] = "25498";
char current_gain_st5[6] = "25498";
char current_gain_st6[6] = "25498";
char chip_select_bank1[3] = "15";
char chip_select_bank2[3] = "16";

ATM90E36 *energyMonitor1;
ATM90E36 *energyMonitor2;

WiFiClient esp_wifi_client;
PubSubClient mqtt_client(esp_wifi_client);
unsigned long last_message_publish = 0;

//flag for saving data
bool shouldSaveConfig = false;

//topics to publish
char* energy_monitor1_system_status0_topic;
char* energy_monitor1_system_status1_topic;
char* energy_monitor1_meter_status0_topic;
char* energy_monitor1_meter_status1_topic;
char* energy_monitor2_system_status0_topic;
char* energy_monitor2_system_status1_topic;
char* energy_monitor2_meter_status0_topic;
char* energy_monitor2_meter_status1_topic;
char* line_voltage_topic;
char* line_frequency_topic;
char* line_current1_topic;
char* line_current2_topic;
char* line_current3_topic;
char* line_current4_topic;
char* line_current5_topic;
char* line_current6_topic;
char* energy_monitor1_total_import_energy_topic;
char* energy_monitor1_total_export_energy_topic;
char* energy_monitor2_total_import_energy_topic;
char* energy_monitor2_total_export_energy_topic;

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println();

  //clean FS, for testing
  //SPIFFS.format();
  readConfigsFromFileSystem();
  setupWiFi();
  initializeEnerygyMonitors();
  initializeMQTTClient();
}

void initializeMQTTClient() {
  mqtt_client.setServer(mqtt_server, atoi(mqtt_port));
  mqtt_client.setCallback(callback);
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  //TODO:
}

void setupWiFi() {
  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_monitor_name("monitor_name", "monitor name", monitor_name, 40);
  WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
  WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 6);
  WiFiManagerParameter custom_line_freq("line_freq", "line freq option", line_freq, 5);
  WiFiManagerParameter custom_pga_gain("pga_gain", "PGA Gain", pga_gain, 5);
  WiFiManagerParameter custom_voltage_gain("voltage_gain", "Voltage Gain", voltage_gain, 5);
  WiFiManagerParameter custom_current_gain_st1("current_gain_st1", "Current Gain Port 1", current_gain_st1, 5);
  WiFiManagerParameter custom_current_gain_st2("current_gain_st2", "Current Gain Port 2", current_gain_st2, 5);
  WiFiManagerParameter custom_current_gain_st3("current_gain_st3", "Current Gain Port 3", current_gain_st3, 5);
  WiFiManagerParameter custom_current_gain_st4("current_gain_st4", "Current Gain Port 4", current_gain_st4, 5);
  WiFiManagerParameter custom_current_gain_st5("current_gain_st5", "Current Gain Port 5", current_gain_st5, 5);
  WiFiManagerParameter custom_current_gain_st6("current_gain_st6", "Current Gain Port 6", current_gain_st6, 5);
  WiFiManagerParameter custom_chip_select_bank1("chip_select_bank1", "Chip 1 Select Pin", chip_select_bank1, 2);
  WiFiManagerParameter custom_chip_select_bank2("chip_select_bank2", "Chip 2 Select Pin", chip_select_bank2, 2);

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //set static ip
  wifiManager.setSTAStaticIPConfig(IPAddress(10,0,1,99), IPAddress(10,0,1,1), IPAddress(255,255,255,0));
  
  //add all your parameters here
  wifiManager.addParameter(&custom_monitor_name);
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_line_freq);
  wifiManager.addParameter(&custom_pga_gain);
  wifiManager.addParameter(&custom_voltage_gain);
  wifiManager.addParameter(&custom_current_gain_st1);
  wifiManager.addParameter(&custom_current_gain_st2);
  wifiManager.addParameter(&custom_current_gain_st3);
  wifiManager.addParameter(&custom_current_gain_st4);
  wifiManager.addParameter(&custom_current_gain_st5);
  wifiManager.addParameter(&custom_current_gain_st6);
  wifiManager.addParameter(&custom_chip_select_bank1);
  wifiManager.addParameter(&custom_chip_select_bank2);

  //reset settings - for testing
  //wifiManager.resetSettings();

  //set minimu quality of signal so it ignores AP's under that quality
  //defaults to 8%
  //wifiManager.setMinimumSignalQuality();
  
  //sets timeout until configuration portal gets turned off
  //useful to make it all retry or go to sleep
  //in seconds
  //wifiManager.setTimeout(120);

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect(monitor_name, "Id0ntknow")) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }

  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");

  //read updated parameters
  strcpy(monitor_name, custom_monitor_name.getValue());
  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());
  strcpy(line_freq, custom_line_freq.getValue());
  strcpy(pga_gain, custom_pga_gain.getValue());
  strcpy(voltage_gain, custom_voltage_gain.getValue());
  strcpy(current_gain_st1, custom_current_gain_st1.getValue());
  strcpy(current_gain_st2, custom_current_gain_st2.getValue());
  strcpy(current_gain_st3, custom_current_gain_st3.getValue());
  strcpy(current_gain_st4, custom_current_gain_st4.getValue());
  strcpy(current_gain_st5, custom_current_gain_st5.getValue());
  strcpy(current_gain_st6, custom_current_gain_st6.getValue());
  strcpy(chip_select_bank1, custom_chip_select_bank1.getValue());
  strcpy(chip_select_bank2, custom_chip_select_bank1.getValue());

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonDocument jsonBuffer(10000);
    JsonObject& json = jsonBuffer.createObject();
    json["monitor_name"] = monitor_name;
    json["mqtt_server"] = mqtt_server;
    json["mqtt_port"] = mqtt_port;
    json["line_freq"] = line_freq;
    json["pga_gain"] = pga_gain;
    json["voltage_gain"] = voltage_gain;
    json["current_gain_st1"] = current_gain_st1;
    json["current_gain_st2"] = current_gain_st2;
    json["current_gain_st3"] = current_gain_st3;
    json["current_gain_st4"] = current_gain_st4;
    json["current_gain_st5"] = current_gain_st5;
    json["current_gain_st6"] = current_gain_st6;
    json["chip_select_bank1"] = chip_select_bank1;
    json["chip_select_bank2"] = chip_select_bank2;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
  }

  Serial.println("local ip");
  Serial.println(WiFi.localIP());
}

void readConfigsFromFileSystem() {
  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);
        configFile.readBytes(buf.get(), size);
        DynamicJsonDocument jsonBuffer(size);
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");
          strcpy(monitor_name, json["monitor_name"]);
          strcpy(mqtt_server, json["mqtt_server"]);
          strcpy(mqtt_port, json["mqtt_port"]);
          strcpy(line_freq, json["line_freq"]);
          strcpy(pga_gain, json["pga_gain"]);
          strcpy(voltage_gain, json["voltage_gain"]);
          strcpy(current_gain_st1, json["current_gain_st1"]);
          strcpy(current_gain_st2, json["current_gain_st2"]);
          strcpy(current_gain_st3, json["current_gain_st3"]);
          strcpy(current_gain_st4, json["current_gain_st4"]);
          strcpy(current_gain_st5, json["current_gain_st5"]);
          strcpy(current_gain_st6, json["current_gain_st6"]);
          strcpy(chip_select_bank1, json["chip_select_bank1"]);
          strcpy(chip_select_bank2, json["chip_select_bank2"]);

          String monitor_name_string = String(monitor_name);
          energy_monitor1_system_status0_topic = (new String(monitor_name_string + "/energy_monitor1/system_status0"))->c_str();
          energy_monitor1_system_status1_topic = (new String(monitor_name_string + "/energy_monitor1/system_status1"))->c_str();
          energy_monitor1_meter_status0_topic = (new String(monitor_name_string + "/energy_monitor1/meter_status0"))->c_str();
          energy_monitor1_meter_status1_topic = (new String(monitor_name_string + "/energy_monitor1/meter_status1"))->c_str();
          energy_monitor2_system_status0_topic = (new String(monitor_name_string + "/energy_monitor2/system_status0"))->c_str();
          energy_monitor2_system_status1_topic = (new String(monitor_name_string + "/energy_monitor2/system_status1"))->c_str();
          energy_monitor2_meter_status0_topic = (new String(monitor_name_string + "/energy_monitor2/meter_status0"))->c_str();
          energy_monitor2_meter_status1_topic = (new String(monitor_name_string + "/energy_monitor2/meter_status1"))->c_str();
          line_voltage_topic = (new String(monitor_name_string + "/line_voltage"))->c_str();
          line_frequency_topic = (new String(monitor_name_string + "/line_frequency"))->c_str();
          line_current1_topic = (new String(monitor_name_string + "/port1/current"))->c_str();
          line_current2_topic = (new String(monitor_name_string + "/port2/current"))->c_str();
          line_current3_topic = (new String(monitor_name_string + "/port3/current"))->c_str();
          line_current4_topic = (new String(monitor_name_string + "/port4/current"))->c_str();
          line_current5_topic = (new String(monitor_name_string + "/port5/current"))->c_str();
          line_current6_topic = (new String(monitor_name_string + "/port6/current"))->c_str();
          energy_monitor1_total_import_energy_topic = (new String(monitor_name_string + "/energy_monitor1/total_import_energy"))->c_str();
          energy_monitor1_total_export_energy_topic = (new String(monitor_name_string + "/energy_monitor1/total_export_energy"))->c_str();
          energy_monitor2_total_import_energy_topic = (new String(monitor_name_string + "/energy_monitor2/total_import_energy"))->c_str();
          energy_monitor2_total_export_energy_topic = (new String(monitor_name_string + "/energy_monitor2/total_export_energy"))->c_str();
        } else {
          Serial.println("failed to load json config");
        }
        configFile.close();
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
}

void initializeEnerygyMonitors() {  
  energyMonitor1 = new ATM90E36(
    (unsigned short) atoi(chip_select_bank1),
    (unsigned short) atoi(line_freq),
    (unsigned short) atoi(pga_gain),
    (unsigned short) atoi(voltage_gain),
    (unsigned short) atoi(current_gain_st1),
    (unsigned short) atoi(current_gain_st2),
    (unsigned short) atoi(current_gain_st3));

  energyMonitor2 = new ATM90E36(
    (unsigned short) atoi(chip_select_bank2),
    (unsigned short) atoi(line_freq),
    (unsigned short) atoi(pga_gain),
    (unsigned short) atoi(voltage_gain),
    (unsigned short) atoi(current_gain_st4),
    (unsigned short) atoi(current_gain_st5),
    (unsigned short) atoi(current_gain_st6));
  
  delay(1000);
  energyMonitor1.begin();
  delay(1000);
  energyMonitor2.begin();
}

void reconnect() {
  // Loop until we're reconnected
  while (!mqtt_client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = String(monitor_name);
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void mqttLoop() {
  if (!mqtt_client.connected()) {
    reconnect();
  }
  mqtt_client.loop();
}

void loop() {
  unsigned long now = millis();
  if (now - last_message_publish > 500) {
    lastMsg = now;
    mqtt_client.publish(energy_monitor1_system_status0_topic, String(energyMonitor1->GetSysStatus0()));
    mqtt_client.publish(energy_monitor1_system_status1_topic, String(energyMonitor1->GetSysStatus1()));
    mqtt_client.publish(energy_monitor1_meter_status0_topic, String(energyMonitor1->GetMeterStatus0()));
    mqtt_client.publish(energy_monitor1_meter_status1_topic, String(energyMonitor1->GetMeterStatus1()));
    mqtt_client.publish(energy_monitor2_system_status0_topic, String(energyMonitor2->GetSysStatus0()));
    mqtt_client.publish(energy_monitor2_system_status1_topic, String(energyMonitor2->GetSysStatus1()));
    mqtt_client.publish(energy_monitor2_meter_status0_topic, String(energyMonitor2->GetMeterStatus0()));
    mqtt_client.publish(energy_monitor2_meter_status1_topic, String(energyMonitor2->GetMeterStatus1()));
    mqtt_client.publish(line_voltage_topic, String(energyMonitor1->GetLineVoltageA()));
    mqtt_client.publish(line_frequency_topic, String(energyMonitor1->GetFrequency()));
    mqtt_client.publish(line_current1_topic, String(energyMonitor1->GetLineCurrentA()));
    mqtt_client.publish(line_current2_topic, String(energyMonitor1->GetLineCurrentB()));
    mqtt_client.publish(line_current3_topic, String(energyMonitor1->GetLineCurrentC()));
    mqtt_client.publish(line_current4_topic, String(energyMonitor2->GetLineCurrentA()));
    mqtt_client.publish(line_current5_topic, String(energyMonitor2->GetLineCurrentB()));
    mqtt_client.publish(line_current6_topic, String(energyMonitor2->GetLineCurrentC()));
    mqtt_client.publish(energy_monitor1_total_import_energy_topic, String(energyMonitor1->GetImportEnergy()));
    mqtt_client.publish(energy_monitor1_total_export_energy_topic, String(energyMonitor1->GetExportEnergy()));
    mqtt_client.publish(energy_monitor2_total_import_energy_topic, String(energyMonitor2->GetImportEnergy()));
    mqtt_client.publish(energy_monitor2_total_export_energy_topic, String(energyMonitor2->GetExportEnergy()));
  }
}

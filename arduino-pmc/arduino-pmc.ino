#include "esp_camera.h" 
#include <WiFi.h>
#include <DNSServer.h>
#include <EEPROM.h>

//
// WARNING!!! Make sure that you have either selected ESP32 Wrover Module,
//            or another board which has PSRAM enabled
//

// Select camera model
//#define CAMERA_MODEL_WROVER_KIT
//#define CAMERA_MODEL_ESP_EYE
//#define CAMERA_MODEL_M5STACK_PSRAM
//#define CAMERA_MODEL_M5STACK_WIDE
#define CAMERA_MODEL_AI_THINKER

#include "camera_pins.h"

const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 1, 1);
DNSServer dnsServer;
WiFiServer server(80);

String getPortalResponse() {
  String responseHTML = ""
  "<!DOCTYPE html><html><head><title>Patient Monitoring</title>"
  "<style>  body {width: 100%; overflow-x: hidden;font-family: Arial;  }  button, input {    font-size: 18pt;  }    #cam-title {    font-size: 20pt;  }  #cam-title input {    border-style: dotted;    border-width: 0;    width: 100%;  }    #title-edit {    display: none;  }</style>"
  "</head><body>"
  "<div style='text-align: center;'>  <h1 style='display: inline;'>Patient Monitoring Camera 2020</h1>  <h5 style='display: inline;'>by #lockdownlab</h5>  <p style='margin:0; font-size: 12pt;'>Open in browser: http://192.168.1.1</p>  </div>  <br>  "
  "<div id='title-display'><div style='float: left; padding: 5px;'><div>Camera Name:</div></div>    <div style='float: right;'><button onClick='onClickEdit();'>edit</button></div>    <div id='cam-title' style='border-style: solid; overflow: hidden;'>Room Name</div>  </div>"
  "<div id='title-edit'><div style='float: left; padding: 5px;'><div>Camera Name:</div></div>    <div style='float: right;'><button onClick='onClickSave();'>save</button></div>    <div id='cam-title' style='border-style: dotted; overflow: hidden;'><input id='title-input' type='text' value=''></div>  </div>"
  "<div style='width: 100%;'>    <img style='width:130%; transform: rotate(90deg) translateX(15%) translateY(16%);'  src='http://192.168.1.1:82/stream'/>  </div>"
  "<script>"
  "document.getElementById('cam-title').innerHTML = unescape('" + mainFetchString(0) + "');"
  "function onClickEdit() {    document.getElementById('title-display').style.display = 'none';    document.getElementById('title-edit').style.display = 'block';    var oldTitle = document.getElementById('cam-title').innerHTML;    document.getElementById('title-input').value = oldTitle;  }"
  "function onClickSave() {    document.getElementById('title-edit').style.display = 'none';    document.getElementById('title-display').style.display = 'block';    var newTitle = document.getElementById('title-input').value;    document.getElementById('cam-title').innerHTML = newTitle;        const Http = new XMLHttpRequest();    const url='http://192.168.1.1:81/update?var=title&val='+newTitle+'&ts='+Date.now();    Http.open('GET', url);    Http.send();  }"
  "</script></body></html>";
  return responseHTML;  
}

void startCameraServer();


String mainFetchString(char add)
{
  int i;
  char data[100]; //Max 100 Bytes
  int len=0;
  unsigned char k;
  k=EEPROM.read(add);
  while(k != '\0' && len<500)   //Read until null character
  {    
    k=EEPROM.read(add+len);
    data[len]=k;
    len++;
  }
  data[len]='\0';
  return String(data);
}

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  //init with high specs to pre-allocate larger buffers
  if (psramFound()) {
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  // camera init

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  //drop down frame size for higher initial frame rate
  sensor_t * s = esp_camera_sensor_get();
  s->set_framesize(s, FRAMESIZE_UXGA);

  WiFi.mode(WIFI_AP);
  delay(100);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 0, 0));
  WiFi.softAP("Patient Monitoring Camera");

  // if DNSServer is started with "*" for domain name, it will reply with
  // provided IP to all DNS request
  dnsServer.start(DNS_PORT, "*", apIP);

  server.begin();


  startCameraServer();

  EEPROM.begin(512);



  String retrievedString = mainFetchString(0);
  Serial.print("The String we read from EEPROM: ");
  Serial.println(retrievedString);
}


void loop() {
  dnsServer.processNextRequest();
  WiFiClient client = server.available();   // listen for incoming clients

  if (client) {
    String currentLine = "";
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        if (c == '\n') {
          if (currentLine.length() == 0) {
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println();
            client.print(getPortalResponse());

            break;
          } else {
            currentLine = "";
          }
        } else if (c != '\r') {
          currentLine += c;
        }
      }
    }
    // client.stop();
  }
}

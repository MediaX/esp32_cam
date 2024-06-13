#include <ESPmDNS.h>

#include "esp_camera.h"
#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <iostream>
#include <sstream>
#include <ESP32Servo.h>
#include "nvs_flash.h"
#include "nvs.h"

#define DUMMY_SERVO1_PIN 12
#define DUMMY_SERVO2_PIN 13

#define PAN_PIN 14
#define TILT_PIN 15

Servo dummyServo1;
Servo dummyServo2;
Servo panServo;
Servo tiltServo;

//Camera related constants
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

const char* hotspotSSID     = "esp32cam-wifi";
const char* hotspotPassword = "12345678";

AsyncWebServer server(80);
AsyncWebSocket wsCamera("/Camera");
AsyncWebSocket wsServoInput("/ServoInput");
AsyncWebSocket wsSettings("/Settings");
uint32_t cameraClientId = 0;

#define LIGHT_PIN 4
const int PWMLightChannel = 4;

const char* htmlHomePage PROGMEM = R"HTMLHOMEPAGE(
<!DOCTYPE html>
<html>
  <head>
  <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no">
    <style>
    .noselect {
      -webkit-touch-callout: none; /* iOS Safari */
        -webkit-user-select: none; /* Safari */
         -khtml-user-select: none; /* Konqueror HTML */
           -moz-user-select: none; /* Firefox */
            -ms-user-select: none; /* Internet Explorer/Edge */
                user-select: none; /* Non-prefixed version, currently
                                      supported by Chrome and Opera */
    }

    .slidecontainer {
      width: 100%;
    }

    .slider {
      -webkit-appearance: none;
      width: 100%;
      height: 20px;
      border-radius: 5px;
      background: #d3d3d3;
      outline: none;
      opacity: 0.7;
      -webkit-transition: .2s;
      transition: opacity .2s;
    }

    .slider:hover {
      opacity: 1;
    }
  
    .slider::-webkit-slider-thumb {
      -webkit-appearance: none;
      appearance: none;
      width: 40px;
      height: 40px;
      border-radius: 50%;
      background: red;
      cursor: pointer;
    }

    .slider::-moz-range-thumb {
      width: 40px;
      height: 40px;
      border-radius: 50%;
      background: red;
      cursor: pointer;
    }

    </style>
  
  </head>
  <body class="noselect" align="center" style="background-color:white">
     
    <!--h2 style="color: teal;text-align:center;">Wi-Fi Camera &#128663; Control</h2-->
    
    <table id="mainTable" style="width:400px;margin:auto;table-layout:fixed" CELLSPACING=10>
      <tr>
        <img id="cameraImage" src="" style="width:400px;height:250px"></td>
      </tr> 
      <tr/><tr/>
      <tr>
        <td style="text-align:left"><b>Pan:</b></td>
        <td colspan=2>
         <div class="slidecontainer">
            <input type="range" min="0" max="180" value="90" class="slider" id="Pan" oninput='sendButtonInput("Pan",value)'>
          </div>
        </td>
      </tr> 
      <tr/><tr/>       
      <tr>
        <td style="text-align:left"><b>Tilt:</b></td>
        <td colspan=2>
          <div class="slidecontainer">
            <input type="range" min="0" max="180" value="90" class="slider" id="Tilt" oninput='sendButtonInput("Tilt",value)'>
          </div>
        </td>   
      </tr>
      <tr/><tr/>       
      <tr>
        <td style="text-align:left"><b>Light:</b></td>
        <td colspan=2>
          <div class="slidecontainer">
            <input type="range" min="0" max="255" value="0" class="slider" id="Light" oninput='sendButtonInput("Light",value)'>
          </div>
        </td>   
      </tr>      
    </table>
  
    <script>
      var webSocketCameraUrl = "ws:\/\/" + window.location.hostname + "/Camera";
      var webSocketServoInputUrl = "ws:\/\/" + window.location.hostname + "/ServoInput";      
      var websocketCamera;
      var websocketServoInput;
      
      function initCameraWebSocket() 
      {
        websocketCamera = new WebSocket(webSocketCameraUrl);
        websocketCamera.binaryType = 'blob';
        websocketCamera.onopen    = function(event){};
        websocketCamera.onclose   = function(event){setTimeout(initCameraWebSocket, 2000);};
        websocketCamera.onmessage = function(event)
        {
          var imageId = document.getElementById("cameraImage");
          imageId.src = URL.createObjectURL(event.data);
        };
      }
      
      function initServoInputWebSocket() 
      {
        websocketServoInput = new WebSocket(webSocketServoInputUrl);
        websocketServoInput.onopen    = function(event)
        {
          var panButton = document.getElementById("Pan");
          sendButtonInput("Pan", panButton.value);
          var tiltButton = document.getElementById("Tilt");
          sendButtonInput("Tilt", tiltButton.value);
          var lightButton = document.getElementById("Light");
          sendButtonInput("Light", lightButton.value);          
        };
        websocketServoInput.onclose   = function(event){setTimeout(initServoInputWebSocket, 2000);};
        websocketServoInput.onmessage = function(event){};        
      }
      
      function initWebSocket() 
      {
        initCameraWebSocket ();
        initServoInputWebSocket();
      }

      function sendButtonInput(key, value) 
      {
        var data = key + "," + value;
        websocketServoInput.send(data);
      }
    
      window.onload = initWebSocket;
      document.getElementById("mainTable").addEventListener("touchend", function(event){
        event.preventDefault()
      });      
    </script>
  </body>    
</html>
)HTMLHOMEPAGE";

const char* htmlSettingsPage PROGMEM = R"HTMLHOMEPAGE(
<!DOCTYPE html>
<html>
  <head>
  <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no">
    <style>
    .noselect {
      -webkit-touch-callout: none; /* iOS Safari */
        -webkit-user-select: none; /* Safari */
         -khtml-user-select: none; /* Konqueror HTML */
           -moz-user-select: none; /* Firefox */
            -ms-user-select: none; /* Internet Explorer/Edge */
                user-select: none; /* Non-prefixed version, currently
                                      supported by Chrome and Opera */
    }

    .slidecontainer {
      width: 100%;
    }

    .slider {
      -webkit-appearance: none;
      width: 100%;
      height: 20px;
      border-radius: 5px;
      background: #d3d3d3;
      outline: none;
      opacity: 0.7;
      -webkit-transition: .2s;
      transition: opacity .2s;
    }

    .slider:hover {
      opacity: 1;
    }
  
    .slider::-webkit-slider-thumb {
      -webkit-appearance: none;
      appearance: none;
      width: 40px;
      height: 40px;
      border-radius: 50%;
      background: red;
      cursor: pointer;
    }

    .slider::-moz-range-thumb {
      width: 40px;
      height: 40px;
      border-radius: 50%;
      background: red;
      cursor: pointer;
    }

    </style>
  
  </head>
  <body class="noselect" align="center" style="background-color:white">
     
    <!--h2 style="color: teal;text-align:center;">Wi-Fi Camera &#128663; Control</h2-->
    
    <table id="mainTable" style="width:400px;margin:auto;table-layout:fixed" CELLSPACING=10>
      <tr>
        <td style="text-align:left"><b>WIFI(SSID):</b></td>
        <td colspan=2>
         <div class="">
            <input type="text" placeholder="Type your SSID" value="" class="" id="SSID" oninput='mySendValue("SSID",value)'>
          </div>
        </td>
      </tr> 
      <tr/><tr/>       
      <tr>
        <td style="text-align:left"><b>Password:</b></td>
        <td colspan=2>
          <div class="">
            <input type="text" placeholder="Type your password" value="" class="" id="password" oninput='mySendValue("password",value)'>
          </div>
        </td>   
      </tr>
</table>
<button id="buttonSubmit" onclick='sendButtonInput()'>Hello</button>    
  
    <script>
      var values = Array("", "");
      var webSocketSettingsUrl = "ws:\/\/" + window.location.hostname + "/Settings";
      var websocketSettings;
      
      function initSettingsWebSocket() 
      {
        websocketSettings = new WebSocket(webSocketSettingsUrl);
        websocketSettings.binaryType = 'blob';
        websocketSettings.onopen    = function(event){};
        websocketSettings.onclose   = function(event){setTimeout(initSettingsWebSocket, 2000);};
        websocketSettings.onmessage = function(event)
        {
          var imageId = document.getElementById("cameraImage");
          imageId.src = URL.createObjectURL(event.data);
        };
      }
      
      function initWebSocket() 
      {
        initSettingsWebSocket ();
      }

      function mySendValue(key, value) 
      {
        console.log("mySendValue");
        if (key === "SSID")
            values[0] = value;
        else if (key == "password")
            values[1] = value;
        var data = key + "," + value;
        console.log(data);
      }

      function sendButtonInput() 
      {
        console.log("sendButtonInput");
        if (values[0].length != 0) {
            var data = values[0] + "," + values[1];
            console.log(data);
            websocketSettings.send(data);
        }
      }
    
      window.onload = initWebSocket;
      document.getElementById("mainTable").addEventListener("touchend", function(event){
        event.preventDefault()
      });      
    </script>
  </body>    
</html>
)HTMLHOMEPAGE";

void handleRoot(AsyncWebServerRequest *request) 
{
  request->send_P(200, "text/html", htmlHomePage);
}

void handleSettingsRoot(AsyncWebServerRequest *request) 
{
  request->send_P(200, "text/html", htmlSettingsPage);
}

void handleNotFound(AsyncWebServerRequest *request) 
{
    request->send(404, "text/plain", "File Not Found");
}

void write_to_EEPROM(const char *value, uint32_t size, const char* stringID)
{
    char *saveId;
    uint32_t sizeSave = (size);
    saveId = (char*)malloc(sizeSave);
    strcpy(saveId,value);
    nvs_handle my_handle;
    nvs_open("storage", NVS_READWRITE, &my_handle);
    nvs_set_str(my_handle, stringID, saveId);
    nvs_commit(my_handle);
    nvs_close(my_handle); 
    free(saveId);  
}

int read_to_EEPROM(char* value, uint32_t size, const char* stringID)
{
    nvs_handle my_handle;
    size_t nvs_required_size_id;
    nvs_open("storage", NVS_READWRITE, &my_handle);
    nvs_get_str(my_handle, stringID, NULL,&nvs_required_size_id);

    if (nvs_required_size_id > size) {
      nvs_close(my_handle);
      return -1;
    }
    
    nvs_get_str(my_handle, stringID, (char *)value, &nvs_required_size_id);
    nvs_close(my_handle);

    return nvs_required_size_id;                                   
}

const char ssidMAGIC[] = "##SSID##";
const char passwordMAGIC[] = "##PASS##";

void onServoInputWebSocketEvent(AsyncWebSocket *server, 
                      AsyncWebSocketClient *client, 
                      AwsEventType type,
                      void *arg, 
                      uint8_t *data, 
                      size_t len) 
{                      
  switch (type) 
  {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\r\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\r\n", client->id());
      panServo.write(90);
      tiltServo.write(90);
      ledcWrite(PWMLightChannel, 0);
      break;
    case WS_EVT_DATA:
      AwsFrameInfo *info;
      info = (AwsFrameInfo*)arg;
      if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) 
      {
        std::string myData = "";
        myData.assign((char *)data, len);
        Serial.printf("Key,Value = [%s]\n", myData.c_str());        
        std::istringstream ss(myData);
        std::string key, value;
        std::getline(ss, key, ',');
        std::getline(ss, value, ',');
        if ( value != "" )
        {
          int valueInt = atoi(value.c_str());
          if (key == "Pan")
          {
            panServo.write(valueInt);
          }
          else if (key == "Tilt")
          {
            tiltServo.write(valueInt);   
          }
          else if (key == "Light")
          {
            ledcWrite(PWMLightChannel, valueInt);         
          }           
        }
      }
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
    default:
      break;  
  }
}

void onCameraWebSocketEvent(AsyncWebSocket *server, 
                      AsyncWebSocketClient *client, 
                      AwsEventType type,
                      void *arg, 
                      uint8_t *data, 
                      size_t len) 
{                      
  switch (type) 
  {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\r\n", client->id(), client->remoteIP().toString().c_str());
      cameraClientId = client->id();
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\r\n", client->id());
      cameraClientId = 0;
      break;
    case WS_EVT_DATA:
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
    default:
      break;  
  }
}

void onSettingsWebSocketEvent(AsyncWebSocket *server, 
                      AsyncWebSocketClient *client, 
                      AwsEventType type,
                      void *arg, 
                      uint8_t *data, 
                      size_t len) 
{    
  char ssid_cstr[100];
  char password_cstr[100]; 
                   
  switch (type) 
  {
    case WS_EVT_CONNECT:
      Serial.printf("#Settings# WebSocket client #%u connected from %s\r\n", client->id(), client->remoteIP().toString().c_str());
      cameraClientId = client->id();
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("#Settings# client #%u disconnected\r\n", client->id());
      cameraClientId = 0;
      break;
    case WS_EVT_DATA:
      AwsFrameInfo *info;
      info = (AwsFrameInfo*)arg;
      if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) 
      {
        std::string myData = "";
        myData.assign((char *)data, len);
        Serial.printf("#Settings# Key,Value = [%s]\r\n", myData.c_str());  
        std::istringstream ss(myData);
        std::string ssid, password;
        std::getline(ss, ssid, ',');
        std::getline(ss, password, ',');
        strcpy(ssid_cstr, ssid.c_str());
        strcpy(password_cstr, password.c_str());
        Serial.printf("#Settings# after parse <%s> <%s>\r\n", ssid_cstr, password_cstr);
        if (ssid.length()) {
          write_to_EEPROM(ssid_cstr, 100, ssidMAGIC);
          write_to_EEPROM(password_cstr, 100, passwordMAGIC);
        }      
      }
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
    default:
      break;  
  }
}

void setupCamera()
{
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
  
  config.frame_size = FRAMESIZE_VGA;
  config.jpeg_quality = 10;
  config.fb_count = 1;

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK)
  {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }  

  if (psramFound())
  {
    heap_caps_malloc_extmem_enable(20000);  
    Serial.printf("PSRAM initialized. malloc to take memory from psram above this size");    
  }  
}

void sendCameraPicture()
{
  if (cameraClientId == 0)
  {
    return;
  }
  unsigned long  startTime1 = millis();
  //capture a frame
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) 
  {
      Serial.println("Frame buffer could not be acquired");
      return;
  }

  unsigned long  startTime2 = millis();
  wsCamera.binary(cameraClientId, fb->buf, fb->len);
  esp_camera_fb_return(fb);
    
  //Wait for message to be delivered
  while (true)
  {
    AsyncWebSocketClient * clientPointer = wsCamera.client(cameraClientId);
    if (!clientPointer || !(clientPointer->queueIsFull()))
    {
      break;
    }
    delay(1);
  }
  
  unsigned long  startTime3 = millis();  
//  Serial.printf("Time taken Total: %d|%d|%d\n",startTime3 - startTime1, startTime2 - startTime1, startTime3-startTime2 );
}

void setUpPinModes()
{
  dummyServo1.attach(DUMMY_SERVO1_PIN);
  dummyServo2.attach(DUMMY_SERVO2_PIN);  
  panServo.attach(PAN_PIN);
  tiltServo.attach(TILT_PIN);

  //Set up flash light
  ledcSetup(PWMLightChannel, 1000, 8);
  pinMode(LIGHT_PIN, OUTPUT);    
  ledcAttachPin(LIGHT_PIN, PWMLightChannel);
}

bool settingMode = false;
bool serverSet = false;

void setupHotSpot()
{
  WiFi.disconnect();
  if (serverSet)
    server.end();

  Serial.println("Setting hotspot");

  WiFi.softAP(hotspotSSID, hotspotPassword);
 
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);
  
  server.on("/", HTTP_GET, handleSettingsRoot);
  server.onNotFound(handleNotFound);
      
  wsSettings.onEvent(onSettingsWebSocketEvent);
  server.addHandler(&wsSettings);

  server.begin();
  Serial.println("Settings HTTP server started");

  settingMode = true;

  for (int i = 0; i < 5; i++){
    ledcWrite(PWMLightChannel, 100);
    delay(200);
    ledcWrite(PWMLightChannel, 0);
    delay(200);
  }
}

// номер контакта для кнопки
const int buttonPin = 2;

// переменная для хранения статуса кнопки:
int buttonState = 0;

void setup(void) 
{
  setUpPinModes();
  Serial.begin(115200);


  char ssid[100] = {0};
  char password[100] = {0};
  int retValSSID = read_to_EEPROM(ssid, 100, ssidMAGIC);
  Serial.printf("ReadSSID %d <%s>\r\n", retValSSID, ssid);
  int retValPassword = read_to_EEPROM(password, 100, passwordMAGIC);
  Serial.printf("ReadPassword %d <%s>\r\n", retValPassword, password);

  bool confAsHotSpot = false;

  if (retValSSID > 0 && retValPassword >= 0) {
    Serial.printf("Trying wifi ssid <%s> password <%s>\r\n", ssid, password);
    WiFi.begin(ssid, password);

    unsigned int wifiTryTime = millis();

    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
      if ((millis() - wifiTryTime) >= 15000) {
        setupHotSpot();
        confAsHotSpot = true;
        break;
      }
    }
  } else {
    confAsHotSpot = true;
  }

  if (confAsHotSpot) {
    setupHotSpot();
  } else {
    Serial.println("");
    Serial.println("WiFi connected");
  
    Serial.print("Camera Ready! Use 'http://");
    Serial.print(WiFi.localIP());
    Serial.println("' to connect");

    server.on("/", HTTP_GET, handleRoot);
    server.onNotFound(handleNotFound);
      
    wsCamera.onEvent(onCameraWebSocketEvent);
    server.addHandler(&wsCamera);

    wsServoInput.onEvent(onServoInputWebSocketEvent);
    server.addHandler(&wsServoInput);

    server.begin();
    Serial.println("HTTP server started");

    setupCamera();
  }

  serverSet = true;

  pinMode(buttonPin, INPUT_PULLUP);
  buttonState = digitalRead(buttonPin);
  Serial.print("ButtonState ");
  Serial.println(buttonState);
}

unsigned long firstTimeDetect = 0;
bool buttonTrack = false;

unsigned long buttonLastCheckTime = 0;

void loop() 
{
  if (!settingMode) {
    if (!buttonLastCheckTime) {
      buttonLastCheckTime = millis();
      if (!digitalRead(buttonPin)) {
        buttonTrack = true;
        firstTimeDetect = buttonLastCheckTime;
      }
    } else if ((millis() - buttonLastCheckTime) >= 400) {
      buttonLastCheckTime = millis();
      // Serial.print("ButtonState ");
      // Serial.println(digitalRead(buttonPin));

      if (buttonTrack) {
        if (digitalRead(buttonPin)) {
          buttonTrack = false;
          firstTimeDetect = 0;
        } else {
          if ((millis() - firstTimeDetect) >= 3000) {
            Serial.println("NEED SWITCH MODE");
            buttonTrack = false;
            firstTimeDetect = 0;
            setupHotSpot();
            cameraClientId = 0;
          }
        }
      } else {
        if (!digitalRead(buttonPin)) {
          buttonTrack = true;
          firstTimeDetect = buttonLastCheckTime;
        }
      }
    }

    wsCamera.cleanupClients(); 
    wsServoInput.cleanupClients(); 
    sendCameraPicture(); 
  }
}

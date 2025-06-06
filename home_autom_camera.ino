#include <esp_camera.h>
#include <WiFi.h>
#include <WebSocketsServer.h>

#include "constants.h"

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

WebSocketsServer webSocket = WebSocketsServer(81);
WiFiServer server(80);
uint8_t cam_num;
bool connected = false;

String SSID = DEF_SSID;
String WIFIPWD = DEF_WIFIPWD;
String REMOTEIP = DEF_REMOTEIP;
String REMOTEPORT = DEF_REMOTEWSPORT;

String index_html =   "<html>\n \
<head>\n \
<title> WebSockets Client</title>\n \
<script src='http://code.jquery.com/jquery-1.9.1.min.js'></script>\n \
</head>\n \
<body>\n \
<img id='live' style=\"height:-webkit-fill-available;\" src=''>\n \
</body>\n \
</html>\n \
<script>\n \
jQuery(function($){\n \
if (!('WebSocket' in window)) {\n \
alert('Your browser does not support web sockets');\n \
}else{\n \
setup();\n \
}\n \
function setup(){\n \
var host = 'ws://"+REMOTEIP+":"+REMOTEPORT+"';\n \
var socket = new WebSocket(host);\n \
socket.binaryType = 'arraybuffer';\n \
if(socket){\n \
socket.onopen = function(){\n \
}\n \
socket.onmessage = function(msg){\n \
var bytes = new Uint8Array(msg.data);\n \
var binary= '';\n \
var len = bytes.byteLength;\n \
for (var i = 0; i < len; i++) {\n \
binary += String.fromCharCode(bytes[i])\n \
}\n \
var img = document.getElementById('live');\n \
img.src = 'data:image/jpg;base64,'+window.btoa(binary);\n \
}\n \
socket.onclose = function(){\n \
showServerResponse('The connection has been closed.');\n \
}\n \
}\n \
}\n \
});\n \
</script>";

//once
void configCamera(){
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

  config.frame_size = FRAMESIZE_HD;
  config.jpeg_quality = 10;
  config.fb_count = 2;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }
}

//each program loop if camera connected
void liveCam(uint8_t num){
  //capture a frame
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
      Serial.println("Frame buffer could not be acquired");
      return;
  }
  //origin : replace this with your own function
  webSocket.sendBIN(num, fb->buf, fb->len);

  //origin : return the frame buffer back to be reused
  esp_camera_fb_return(fb);
}

//each event (bi-directional), connection, disconnection
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {

    switch(type) {
        case WStype_DISCONNECTED:
            Serial.printf("[%u] Disconnected!\n", num);
            // if (cam_num == num) connected = false;
            break;
        case WStype_CONNECTED:
            cam_num = num;
            connected = true;
            Serial.printf("[%u] Connected!\n", num);
            break;
        case WStype_TEXT:
        case WStype_BIN:
        case WStype_ERROR:      
        case WStype_FRAGMENT_TEXT_START:
        case WStype_FRAGMENT_BIN_START:
        case WStype_FRAGMENT:
        case WStype_FRAGMENT_FIN:
            break;
    }
}

//once
void setup() {
  Serial.begin(115200);
  Serial.println("");
  Serial.print("Connection to " + SSID);
  WiFi.begin(SSID, WIFIPWD);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println(" ok");

  String IP = WiFi.localIP().toString();
  Serial.println("Local IP address: " + IP);
  Serial.println("Remote IP address (hard configured): " + REMOTEIP);
  // index_html.replace("server_ip", IP);
  server.begin();
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  configCamera();
}

//each program loop, handle new connection if necessary
void http_resp(){
  WiFiClient client = server.available();
  if (client.connected() && client.available()) {
    Serial.println("Read from client :");
    String client_request = client.readString();
    String resource = client_request.substring(0, client_request.indexOf(" HTTP"));
    Serial.println("resource : " + resource);
    client.flush();
    if (resource == "GET /getcam")
      client.print("HTTP/1.1 200 OK\r\n\r\n" + index_html);   //send js script to connect the websocket
    else if (resource == "GET /metrics")
    {
      // String metrics = "# TYPE camera_frames_total counter\n";
      // metrics += "camera_frames_total " + String(frameCount) + "\n";
      String metrics = "# TYPE camera_health gauge\n";
      metrics += "camera_health 1\n";
      client.print("HTTP/1.1 200 OK\r\n\r\n" + metrics);
    }
    else
      client.print("HTTP/1.1 404");
    client.stop();
  }
}

//program loop
void loop() {
  http_resp();
  webSocket.loop();   //to trigger event if needed
  if(connected == true){
    liveCam(cam_num);
  }
  delay(33);
}

#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <ESP8266WebServer.h>
#include <FS.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>

#include <NTPClient.h>
#include <WiFiUdp.h>

#include <math.h>

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

ESP8266WebServer server(80);    // create a web server on port 80
WebSocketsServer webSocket(81); // create a websocket server on port 81

File fsUploadFile; // a File variable to temporarily store the received file

const char *OTAName = "ESP8266"; // A name and a password for the OTA service
// const char *OTAPassword = "esp8266";

const char *ssid = "ssid";           // The SSID (name) of the Wi-Fi network you want to connect to
const char *password = "pass"; // The password of the Wi-Fi network

#define BUILT_IN 2 // specify the pins with an RGB LED connected
#define LED_STR 0  // specify the pins with an RGB LED connected

#define FADE_DELAY_FAST 1
#define FADE_DELAY_SLOW 5

int CURRENT_BRIGHTNESS = 0;

/*__________________________________________________________SETUP__________________________________________________________*/

void setup()
{
  pinMode(LED_STR, OUTPUT);

  fadeToLevel(20, FADE_DELAY_FAST);

  Serial.begin(115200); // Start the Serial communication to send messages to the computer
  delay(10);
  Serial.println("\r\n");

  startWiFi();

  startOTA(); // Start the OTA service

  startSPIFFS(); // Start the SPIFFS and list all contents

  startWebSocket(); // Start a WebSocket server

  startServer(); // Start a HTTP server with a file read handler and an upload handler

  //timeClient.begin();
  //timeClient.setUpdateInterval(600000);
}

/*__________________________________________________________LOOP__________________________________________________________*/

void loop()
{
  webSocket.loop();      // constantly check for websocket events
  server.handleClient(); // run the server
  ArduinoOTA.handle();   // listen for OTA events
  //handleTime();
}

/*__________________________________________________________SETUP_FUNCTIONS__________________________________________________________*/

void startWiFi()
{ // Try to connect to WiFi

  WiFi.begin(ssid, password); // Connect to the network
  Serial.print("Connecting to ");
  Serial.print(ssid);
  Serial.println(" ...");

  int i = 0;
  while (WiFi.status() != WL_CONNECTED)
  { // Wait for the Wi-Fi to connect
    delay(100);
    Serial.print(++i);
    Serial.print(' ');

    if ((i % 2) == 0)
    {
      fadeToLevel(200, FADE_DELAY_SLOW);
    }
    else
    {
      fadeToLevel(20, FADE_DELAY_SLOW);
    }
  }

  fadeToLevel( 0, FADE_DELAY_FAST);

  Serial.println('\n');
  Serial.println("Connection established!");
  Serial.print("IP address:\t");
  Serial.println(WiFi.localIP()); // Send the IP address of the ESP8266 to the computer
  Serial.println("\r\n");
}

void startOTA()
{ // Start the OTA service
  ArduinoOTA.setHostname(OTAName);
  //ArduinoOTA.setPassword(OTAPassword);

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\r\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR)
      Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR)
      Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR)
      Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR)
      Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR)
      Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("OTA ready\r\n");
}

void startSPIFFS()
{                 // Start the SPIFFS and list all contents
  SPIFFS.begin(); // Start the SPI Flash File System (SPIFFS)
  Serial.println("SPIFFS started. Contents:");
  {
    Dir dir = SPIFFS.openDir("/");
    while (dir.next())
    { // List the file system contents
      String fileName = dir.fileName();
      size_t fileSize = dir.fileSize();
      Serial.printf("\tFS File: %s, size: %s\r\n", fileName.c_str(), formatBytes(fileSize).c_str());
    }
    Serial.printf("\n");
  }
}

void startWebSocket()
{                                    // Start a WebSocket server
  webSocket.begin();                 // start the websocket server
  webSocket.onEvent(webSocketEvent); // if there's an incomming websocket message, go to function 'webSocketEvent'
  Serial.println("WebSocket server started.");
}

void startServer()
{                                           // Start a HTTP server with a file read handler and an upload handler
  server.on("/edit.html", HTTP_POST, []() { // If a POST request is sent to the /edit.html address,
    server.send(200, "text/plain", "");
  },
            handleFileUpload); // go to 'handleFileUpload'

  server.onNotFound(handleNotFound); // if someone requests any other file or page, go to function 'handleNotFound'
                                     // and check if the file exists

  server.begin(); // start the HTTP server
  Serial.println("HTTP server started.");
}

/*__________________________________________________________TIME_HANDLERS__________________________________________________________*/
void handleTime()
{
  timeClient.update();

  int minutes = timeClient.getMinutes();
  float temp = 300 * (minutes / 30);
  
  if (timeClient.getHours() == 4 && minutes < 15 && CURRENT_BRIGHTNESS != 300) {
    fadeToLevel(300, 50);
  } else if (timeClient.getHours() == 3 && minutes > 15 && CURRENT_BRIGHTNESS != 1000) {
    fadeToLevel(1000, 50);    
  }

  delay(1000);
}


/*__________________________________________________________SERVER_HANDLERS__________________________________________________________*/

void handleNotFound()
{ // if the requested file or page doesn't exist, return a 404 not found error
  if (!handleFileRead(server.uri()))
  { // check if the file exists in the flash memory (SPIFFS), if so, send it
    server.send(404, "text/plain", "404: File Not Found");
  }
}

bool handleFileRead(String path)
{ // send the right file to the client (if it exists)
  Serial.println("handleFileRead: " + path);
  if (path.endsWith("/"))
    path += "index.html";                    // If a folder is requested, send the index file
  String contentType = getContentType(path); // Get the MIME type
  String pathWithGz = path + ".gz";
  if (SPIFFS.exists(pathWithGz) || SPIFFS.exists(path))
  {                                                     // If the file exists, either as a compressed archive, or normal
    if (SPIFFS.exists(pathWithGz))                      // If there's a compressed version available
      path += ".gz";                                    // Use the compressed verion
    File file = SPIFFS.open(path, "r");                 // Open the file
    size_t sent = server.streamFile(file, contentType); // Send it to the client
    file.close();                                       // Close the file again
    Serial.println(String("\tSent file: ") + path);
    return true;
  }
  Serial.println(String("\tFile Not Found: ") + path); // If the file doesn't exist, return false
  return false;
}

void handleFileUpload()
{ // upload a new file to the SPIFFS
  HTTPUpload &upload = server.upload();
  String path;
  if (upload.status == UPLOAD_FILE_START)
  {
    path = upload.filename;
    if (!path.startsWith("/"))
      path = "/" + path;
    if (!path.endsWith(".gz"))
    {                                   // The file server always prefers a compressed version of a file
      String pathWithGz = path + ".gz"; // So if an uploaded file is not compressed, the existing compressed
      if (SPIFFS.exists(pathWithGz))    // version of that file must be deleted (if it exists)
        SPIFFS.remove(pathWithGz);
    }
    Serial.print("handleFileUpload Name: ");
    Serial.println(path);
    fsUploadFile = SPIFFS.open(path, "w"); // Open the file for writing in SPIFFS (create if it doesn't exist)
    path = String();
  }
  else if (upload.status == UPLOAD_FILE_WRITE)
  {
    if (fsUploadFile)
      fsUploadFile.write(upload.buf, upload.currentSize); // Write the received bytes to the file
  }
  else if (upload.status == UPLOAD_FILE_END)
  {
    if (fsUploadFile)
    {                       // If the file was successfully created
      fsUploadFile.close(); // Close the file again
      Serial.print("handleFileUpload Size: ");
      Serial.println(upload.totalSize);
      server.sendHeader("Location", "/success.html"); // Redirect the client to the success page
      server.send(303);
    }
    else
    {
      server.send(500, "text/plain", "500: couldn't create file");
    }
  }
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t lenght)
{ // When a WebSocket message is received
  switch (type)
  {
  case WStype_DISCONNECTED: // if the websocket is disconnected
    Serial.printf("[%u] Disconnected!\n", num);
    break;
  case WStype_CONNECTED:
  { // if a new websocket connection is established
    IPAddress ip = webSocket.remoteIP(num);

    broadcastBrighness();

    Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
  }
  break;
  case WStype_TEXT: // if new text data is received
    Serial.printf("[%u] get Text: %s\n", num, payload);

    String whatWeHave = String((char*) payload);

    DynamicJsonBuffer jb;
    JsonObject& parsed = jb.parseObject((char*) payload);
    
    if (!parsed.success()) {
        webSocket.broadcastTXT(whatWeHave);
        break;
    }
 
    const uint32_t fadeInDelay = atoi((const char *)parsed["fadeIn"]);
    const uint32_t fadeOutDelay = atoi((const char *)parsed["fadeOut"]);
    const uint32_t startBrightness = atoi((const char *)parsed["startBr"]);
    const uint32_t endBrightness = atoi((const char *)parsed["endBr"]);
    
    //uint32_t brightness = atoi((const char *)payload); // decode rgb data

    //fadeToLevel(startBrightness, FADE_DELAY_FAST);
    
    executeParams(fadeInDelay, fadeOutDelay, startBrightness, endBrightness);
    
    break;
  }
}

/*__________________________________________________________HELPER_FUNCTIONS__________________________________________________________*/

void broadcastBrighness()
{
  String charBright = String(CURRENT_BRIGHTNESS);
  webSocket.broadcastTXT(charBright);
}

void fadeToLevel(int toLevel, int fadeDelay)
{
  if (!fadeDelay) {
    CURRENT_BRIGHTNESS = toLevel;
    analogWrite(LED_STR, CURRENT_BRIGHTNESS); 

    return;
  }
  
  int delta = (toLevel - CURRENT_BRIGHTNESS) < 0 ? -1 : 1;

  while (CURRENT_BRIGHTNESS != toLevel)
  {
    CURRENT_BRIGHTNESS += delta;
    analogWrite(LED_STR, CURRENT_BRIGHTNESS);
    delay(fadeDelay);
  }
}

String formatBytes(size_t bytes)
{ // convert sizes in bytes to KB and MB
  if (bytes < 1024)
  {
    return String(bytes) + "B";
  }
  else if (bytes < (1024 * 1024))
  {
    return String(bytes / 1024.0) + "KB";
  }
  else if (bytes < (1024 * 1024 * 1024))
  {
    return String(bytes / 1024.0 / 1024.0) + "MB";
  }
}

String getContentType(String filename)
{ // determine the filetype of a given filename, based on the extension
  if (filename.endsWith(".html"))
    return "text/html";
  else if (filename.endsWith(".css"))
    return "text/css";
  else if (filename.endsWith(".js"))
    return "application/javascript";
  else if (filename.endsWith(".ico"))
    return "image/x-icon";
  else if (filename.endsWith(".gz"))
    return "application/x-gzip";
  return "text/plain";
}

// Blinking logic according to params
void executeParams(int fadeInDelay, int fadeOutDelay, int startBrightness, int endBrightness) {
  fadeToLevel(startBrightness, fadeInDelay);
  fadeToLevel(endBrightness, fadeOutDelay);
}

# esp-led-control

Project for controlling LED strip using ESP-8266

Key features:
* OTA updates
* frontend is stored in SPIFFS 
* possibility of uploading files via WEB interface to SPIFFS
* frontend is storred as compressed GZIP files
* communication between frontend and board is done via websockets

Hardware:
* ESP-8266 (Wemos D1 R1 mini board)
* IRF3205 MOSFET transistor

Modes:
* brightness level
* ...coming soon

Project is based on https://github.com/tttapa/ESP8266

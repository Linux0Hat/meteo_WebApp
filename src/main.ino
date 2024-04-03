#include "DHT.h"
#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_NeoPixel.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <BlueDot_BME280.h>

#define LED 13

int LED_ON;

#define NB_MIN_SAVED 10

float dataT[NB_MIN_SAVED];
float dataH[NB_MIN_SAVED];
float TempSec[60];
float HumiditySec[60];

int cursorSec;
int nbValue;
int start;
int cursorMin;

int saveTime;

#define DHTPIN 5
#define DHTTYPE DHT11

#define NEOPIXELPIN 12
#define NUMPIXELS 4

DHT dht(DHTPIN, DHTTYPE);
Adafruit_NeoPixel pixels(NUMPIXELS, NEOPIXELPIN, NEO_GRB + NEO_KHZ800);

const char *ssid = "<wifi name>";
const char *password = "<wifi password>";
#define SERVERPORT 80
WebServer server(SERVERPORT);

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, 4);

void handleRoot()
{
  const char page[] PROGMEM = R"rawliteral(
  <html lang='fr'>
  <head>
  <meta charset='UTF-8'>
  <link href='https://cdn.jsdelivr.net/npm/bootstrap@5.3.0-alpha1/dist/css/bootstrap.min.css' rel='stylesheet' integrity='sha384-GLhlTQ8iRABdZLl6O3oVMWSktQOp6b7In1Zl3/Jr59b6EGGoI1aFkw7cmDA6j6gD' crossorigin='anonymous'>
  <script src='https://cdn.jsdelivr.net/npm/chart.js'></script>
  </head>
  <body style='padding-right:2rem;padding-left:2rem; padding-up:1rem'>
  <h1>METEO MANAGER</h1>
  <hr>
  <div class="row">
  <div class="col-sm-3">
  <div class='card' ><div class='card-body'>
  <p>Température : </p><p id=temp>Loading...</p>
  <p>Humidité : </p><p id=humid>Loading...</p>
  <button onclick="led()" type="button" class="btn btn-primary">On/Off Leds</button>
  </div></div></div>
  <div class="col-sm-9">
  <div class='card'><div class='card-body'><div><canvas id='myChart'></canvas></div></div></div>
  </div>
  </div>
  <script>
  let xhr = new XMLHttpRequest();
  var dataT = [];
  var dataH = [];
  var dataX = [];
  chart = new Chart("myChart", {
  type: "line",
  data: {
    labels: dataX,
    datasets: [{
      backgroundColor: "rgba(255,0,0,255)",
      borderColor: "rgba(255,0,0,255)",
      label:"Température",
      data: dataT
    },
    {
      backgroundColor: "rgba(0,0,255,255)",
      borderColor: "rgba(0,0,255,255)",
      label:"Humidité",
      data: dataH
    }]
  }
  });
  setInterval(refresh, 5000);
  function refresh() {
  xhr.open('GET', '/api');
  xhr.send();
  xhr.onload = function() {
    var json = JSON.parse(xhr.response);
    temp.innerHTML = Math.round((json.temp) * 100) / 100 + "°C";
    humid.innerHTML = Math.round((json.humid) * 100) / 100 + "%";
    dataT.push(json.temp);
    dataH.push(json.humid); 
    var d = new Date();
    var t = d.toLocaleTimeString();
    dataX.push(t);
    chart.update();
  };
  };
  function led() {
    let led = new XMLHttpRequest();
    led.open('GET', '/led');
    led.send();
   };
  </script>
  </body>
  </html>
  )rawliteral";

  server.send(200, "text/html", page);
  digitalWrite(LED, HIGH);
  delay(750);
  digitalWrite(LED, LOW);
  Serial.print("Received request : ");
  Serial.println(server.client().remoteIP());
}

void handleApi()
{
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  StaticJsonDocument<200> doc;
  doc["temp"] = t;
  doc["humid"] = h;
  char json[80];
  serializeJsonPretty(doc, json);
  server.sendHeader("Location", "/api");
  server.send(200, "text/html", json);
}

void handleLoad()
{
  StaticJsonDocument<200> doc;
  const char* TempString = "";
  const char* HumidString = "";
  //if (NB_MIN_SAVED != nbValue){for (int i = 0; i < nbValue; i++){TempString += dataT[i];TempString += "/";HumidString += dataH[i]; HumidString += '/';}}
  doc["TempString"] = TempString;
  doc["HumidString"] = HumidString;
  doc["startTime"] = clock()/60000;
  char json[80];
  serializeJsonPretty(doc, json);
  server.sendHeader("Location", "/load");
  server.send(200, "text/html", json);
}

void handleLed()
{
  if (LED_ON == 1)
  {
    LED_ON = 0;
  }
  else
  {
    LED_ON = 1;
  }
  StaticJsonDocument<200> doc;
  doc["status"] = "OK";
  char json[80];
  serializeJsonPretty(doc, json);
  server.sendHeader("Location", "/led");
  server.send(200, "text/html", json);
}

void setup()
{
  LED_ON = 1;
  saveTime = 0;
  cursorSec = 0;
  nbValue = 0;
  start = 0;
  cursorMin = 0;
  saveTime = 0;
  Serial.begin(9600);
  dht.begin();
  pinMode(LED, OUTPUT);
  WiFi.persistent(false);
  WiFi.begin(ssid, password);
  Serial.print("Connecting ...");

  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(100);
  }

  Serial.println("\n");
  Serial.println("Connexion success !");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("Seb server on");
  server.on("/", handleRoot);
  server.on("/api", handleApi);
  server.on("/led", handleLed);
  server.on("/load", handleLoad);
  server.begin();

  pixels.begin();
  pixels.clear(); // Pause for 2 seconds
  // Initialisation de l'écran
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 screen unaviable"));
  }
  else
  {
    Serial.println(F("SSD1306 screen found"));
    display.display();
    // Clear the buffer
    display.clearDisplay();
    display.setTextSize(1);      // Normal 1:1 pixel scale
    display.setTextColor(WHITE); // Draw white text
    display.setCursor(0, 0);     // Start at top-left corner
    display.display();
  }
  // Clear the buffer
  display.clearDisplay();
}

void loop()
{
  server.handleClient();
  float t = dht.readTemperature();
  float h = dht.readHumidity();

  if (saveTime <= clock())
  {
    saveTime += 1000;
    if (cursorSec == 60){
      cursorSec = 0;
      int resultT = 0;
      int resultH = 0;
      for (int i = 0; i < 60; i++) {resultT += TempSec[i];resultH += HumiditySec[i];}
      resultT = resultT/60;
      resultH = resultH/60;

      if (cursorMin == NB_MIN_SAVED-1) {cursorMin = 0;} 
      else {cursorMin += 1;}
      if (nbValue == NB_MIN_SAVED) {start = cursorMin + 1;}
      else {nbValue += 1;}
      if (start == NB_MIN_SAVED){start = 0;}
      dataT[cursorMin] = resultT;
      dataH[cursorMin] = resultH;
    }
    TempSec[cursorSec] = t;
    HumiditySec[cursorSec] = h;
    cursorSec += 1;
  }

  pixels.clear();
  if (LED_ON)
  {
    if (t > 24)
    {
      pixels.setPixelColor(0, pixels.Color(255, 0, 0));
      pixels.setPixelColor(1, pixels.Color(255, 0, 0));
    }
    else
    {
      pixels.setPixelColor(0, pixels.Color(0, 255, 0));
      pixels.setPixelColor(1, pixels.Color(0, 255, 0));
    }
    if (t < 15)
    {
      pixels.setPixelColor(0, pixels.Color(0, 0, 255));
      pixels.setPixelColor(1, pixels.Color(0, 0, 255));
    }

    if (h > 70)
    {
      pixels.setPixelColor(2, pixels.Color(0, 0, 255));
      pixels.setPixelColor(3, pixels.Color(0, 0, 255));
    }
    else
    {
      pixels.setPixelColor(2, pixels.Color(0, 255, 0));
      pixels.setPixelColor(3, pixels.Color(0, 255, 0));
    }
    if (h < 30)
    {
      pixels.setPixelColor(2, pixels.Color(255, 165, 0));
      pixels.setPixelColor(3, pixels.Color(255, 165, 0));
    }
  }

  pixels.show();
  display.clearDisplay();
  display.setTextSize(2); // Draw 2X-scale text
  display.setCursor(10, 0);
  display.println(F("METEO"));
  display.setTextSize(0.75);
  char text[80];
  sprintf(text, "Temperature: %.1f C", t);
  display.println(F(text));
  char text_[80];
  sprintf(text_, "Humidity: %.1f", h);
  display.print(F(text_));
  display.println(" %");
  display.print("IP: ");
  display.println(WiFi.localIP());
  display.print("Port: ");
  display.println(SERVERPORT);
  display.display();
}

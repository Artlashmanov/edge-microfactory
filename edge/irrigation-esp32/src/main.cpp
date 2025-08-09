#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>

// === Wi-Fi SETTINGS ===
const char* ssid = "YOUR_WIFI";
const char* password = "YOUR_PASS";

// === MQTT ===
const char* mqtt_server = "192.168.0.86";

// === ESP32 Pins ===
const int MOISTURE_SENSOR_PIN = 36;  // VP (A0)
const int RELAY_PIN           = 26;  // HIGH = pump ON (adjust if inverted)

// === Globals ===
WiFiClient      espClient;
PubSubClient    mqttClient(espClient);
WebServer       server(80);

String logs;
bool   pumpState = false;
int    moistureValue = 0;

// Timer for manual run
bool          manualActive = false;
unsigned long pumpOffAt    = 0;

static inline bool timeReached(unsigned long t) { return (long)(millis() - t) >= 0; }
void pumpOn()  { digitalWrite(RELAY_PIN, HIGH); pumpState = true;  }
void pumpOff() { digitalWrite(RELAY_PIN, LOW);  pumpState = false; }

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message; for (unsigned int i=0;i<length;i++) message += (char)payload[i];
  if (String(topic) == "watering/plant1") {
    if (message == "ON")  { manualActive=false; pumpOffAt=0; pumpOn();  logs += "MQTT: Pump ON\n"; }
    if (message == "OFF") { manualActive=false; pumpOffAt=0; pumpOff(); logs += "MQTT: Pump OFF\n"; }
  }
  if (logs.length() > 10000) logs.remove(0, logs.length()-10000);
}

void connectWiFi() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(300); }
  logs = "WiFi connected! IP: " + WiFi.localIP().toString() + "\n";
}

void connectMQTT() {
  while (!mqttClient.connected()) {
    if (mqttClient.connect("ESP32Polivalkka")) {
      mqttClient.subscribe("watering/plant1");
      logs += "MQTT connected\n";
    } else { delay(1500); }
  }
}

void setupOTA(){ ArduinoOTA.setHostname("esp32-irrigation"); ArduinoOTA.begin(); }

void handleRoot(){
  String html = R"HTML(
  <html><head><meta charset='utf-8'><title>ESP32 Watering</title>
  <style>
    body{background:#1a1a1a;color:#00ff9c;font-family:monospace;padding:14px}
    button{margin:5px;padding:8px 12px;font-size:16px;border:0;border-radius:8px;background:#1f6feb;color:#fff;cursor:pointer}
    button.stop{background:#eb4d4b}
    input{background:#111;color:#9df;border:1px solid #333;padding:6px;border-radius:6px;width:80px;text-align:right}
    #status{margin:10px 0}
    pre{background:#111;color:#9df;padding:10px;max-height:260px;overflow-y:auto;border-radius:8px}
  </style>
  <script>
    function sendCmd(cmd){fetch('/pump?cmd='+cmd).then(r=>r.text()).then(update)}
    function runSec(s){fetch('/pump?cmd=RUN&ms='+(s*1000)).then(r=>r.text()).then(update)}
    function runCustom(){const s=parseInt(document.getElementById('sec').value||0); if(s>0) runSec(s);}
    function update(){
      fetch('/status').then(r=>r.json()).then(d=>{
        document.getElementById('status').innerText =
          `Pump: ${d.pump?'ON':'OFF'} | Moisture: ${d.moisture} | Left: ${d.left_ms} ms${d.manual?' (manual)':''}`;
        document.getElementById('logs').innerText = d.logs;
      });
    }
    setInterval(update,1500); window.onload=update;
  </script></head>
  <body>
    <h2>ESP32 Watering</h2>
    <div>
      <button onclick="sendCmd('ON')">Pump ON</button>
      <button class="stop" onclick="sendCmd('OFF')">Pump OFF</button>
    </div>
    <div style="margin:6px 0">
      <input id="sec" type="number" min="1" max="600" value="10"> sec
      <button onclick="runCustom()">RUN for N s</button>
    </div>
    <div>
      Quick: <button onclick="runSec(5)">5s</button>
             <button onclick="runSec(10)">10s</button>
             <button onclick="runSec(30)">30s</button>
             <button onclick="runSec(60)">60s</button>
    </div>
    <div id="status">...</div>
    <pre id="logs">...</pre>
  </body></html>)HTML";
  server.send(200, "text/html", html);
}

void handleStatus(){
  long left = (manualActive ? max(0L, (long)(pumpOffAt - millis())) : 0L);
  String safe=logs; safe.replace("\\","\\\\"); safe.replace("\"","\\\""); safe.replace("\n","\\n");
  String json = String("{") +
    "\"pump\":" + (pumpState?"true":"false") + "," +
    "\"moisture\":" + moistureValue + "," +
    "\"manual\":" + (manualActive?"true":"false") + "," +
    "\"left_ms\":" + left + "," +
    "\"logs\":\"" + safe + "\"}";
  server.send(200, "application/json", json);
}

void handlePump(){
  String cmd = server.arg("cmd");
  if (cmd=="ON"){ manualActive=false; pumpOffAt=0; pumpOn(); logs+="Web: Pump ON\\n";}
  else if (cmd=="OFF"){ manualActive=false; pumpOffAt=0; pumpOff(); logs+="Web: Pump OFF\\n";}
  else if (cmd=="RUN"){
    long ms = server.hasArg("ms") ? server.arg("ms").toInt() : 0;
    if (ms>0){ pumpOn(); manualActive=true; pumpOffAt=millis()+(unsigned long)ms; logs += "Web: RUN "+String(ms)+" ms\\n"; }
  }
  if (logs.length()>10000) logs.remove(0, logs.length()-10000);
  server.send(200, "text/plain", "OK");
}

void setup(){
  Serial.begin(115200);
  pinMode(RELAY_PIN, OUTPUT);
  pumpOff();

  WiFi.mode(WIFI_STA); connectWiFi();
  server.on("/", handleRoot);
  server.on("/status", handleStatus);
  server.on("/pump", handlePump);
  server.begin();

  ArduinoOTA.setHostname("esp32-irrigation"); ArduinoOTA.begin();
  mqttClient.setServer(mqtt_server, 1883);
  mqttClient.setCallback(mqttCallback);
}

void loop(){
  server.handleClient();
  ArduinoOTA.handle();
  if (!mqttClient.connected()) connectMQTT();
  mqttClient.loop();

  if (manualActive && pumpState && timeReached(pumpOffAt)){
    pumpOff(); manualActive=false; pumpOffAt=0; logs += "Timer: Pump OFF\\n";
  }

  static unsigned long last=0;
  if (millis()-last>2000){
    moistureValue = analogRead(MOISTURE_SENSOR_PIN);
    mqttClient.publish("moisture/plant1", String(moistureValue).c_str());
    String le="Moisture: "+String(moistureValue);
    if (!manualActive){
      if (moistureValue>2500 && !pumpState){ le += " | Dry. Pump ON!"; pumpOn(); }
      else if (moistureValue<=2500 && pumpState){ le += " | Wet. Pump OFF."; pumpOff(); }
    } else le += " | manual RUN active";
    logs += le + "\\n"; if (logs.length()>10000) logs.remove(0, logs.length()-10000);
    last = millis();
  }
}

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>

// ==== Wi-Fi ====
const char* ssid     = "YOUR_WIFI";
const char* password = "YOUR_PASS";

// ==== UART to UNO ====
static int RX2_PIN = 16;
static int TX2_PIN = 17;
HardwareSerial& BOT = Serial2;

// ==== Web ====
WebServer server(80);
String logs;

void appendLog(const String& s) {
  char t[16]; snprintf(t, sizeof(t), "[%lu] ", (unsigned long)(millis()/1000));
  logs += t + s + "\n";
  if (logs.length() > 10000) logs.remove(0, logs.length()-10000);
}
String hex1(uint8_t b){ static const char* H="0123456789ABCDEF"; String s; s+=H[b>>4]; s+=H[b&0xF]; return s; }

volatile uint32_t currentBaud = 115200;
volatile int currentFmt = 1; // A5 <code> 5A
void botBegin(uint32_t b){ BOT.end(); delay(50); BOT.begin(b, SERIAL_8N1, RX2_PIN, TX2_PIN); }
void setBaud(uint32_t b){ currentBaud=b; botBegin(b); appendLog(String("Set baud -> ")+b); }

enum {
  CMD_Forward=0x5C, CMD_Backward=0xA3, CMD_TurnLeft=0x95, CMD_TurnRight=0x6A,
  CMD_Stop=0x00, CMD_TopLeft=0x14, CMD_BottomLeft=0x81, CMD_TopRight=0x48, CMD_BottomRight=0x22,
  CMD_Clockwise=0x53, CMD_CounterClockwise=0xAC,
  CMD_Model1=0x19, CMD_Model2=0x1A, CMD_Model3=0x1B, CMD_Model4=0x1C,
  CMD_MotorLeft=0xE6, CMD_MotorRight=0xE7
};

uint8_t frame[8];
String fmtName(int fmt){ if(fmt==1) return "A5 <code> 5A"; if(fmt==5) return "ASCII+LF"; if(fmt==6) return "<code>"; return "?"; }
size_t makeFrame(int fmt, uint8_t code, uint8_t* out){
  switch(fmt){
    case 1: out[0]=0xA5; out[1]=code; out[2]=0x5A; return 3;
    case 5: { char c='S'; if (code==CMD_Forward)c='F'; else if(code==CMD_Backward)c='B';
              else if(code==CMD_TurnLeft)c='L'; else if(code==CMD_TurnRight)c='R';
              else if(code==CMD_Clockwise)c='C'; else if(code==CMD_Stop)c='S';
              out[0]=(uint8_t)c; out[1]='\n'; return 2; }
    case 6: out[0]=code; return 1;
  } return 0;
}
void sendByFormat(int fmt, uint8_t code, const char* name){
  size_t n = makeFrame(fmt, code, frame);
  size_t sent = BOT.write(frame, n); BOT.flush();
  String hx; for(size_t i=0;i<n;i++){ hx+=" "; hx+=hex1(frame[i]); }
  appendLog(String("TX (fmt=")+fmtName(fmt)+", baud="+currentBaud+") -> "+name+" :"+hx+", sent="+sent);
}

int lastDistance = -1;
String rxLine; String hexbuf;

void page(){
  String h;
  h += "<meta name='viewport' content='width=device-width,initial-scale=1'>"
       "<style>body{font-family:system-ui,Segoe UI,Roboto,Arial;background:#0b0f14;color:#d3f1ff;margin:14px}"
       "h3{margin:0 0 8px 0}#status{margin:8px 0 14px 0}"
       ".pad{display:grid;grid-template-columns:repeat(3,84px);gap:10px;justify-content:center;margin:10px 0 6px}"
       "button{height:84px;width:84px;font-size:28px;border:0;border-radius:14px;background:#1f6feb;color:#fff;cursor:pointer}"
       "button.stop{background:#eb4d4b;font-size:22px}"
       "button.rot{background:#8b5cf6;width:160px;height:52px;font-size:18px;border-radius:10px}"
       ".row{display:flex;gap:12px;justify-content:center;margin:10px 0}"
       "#log{white-space:pre-wrap;background:#0a0e12;border:1px solid #1f2a37;padding:10px;height:280px;overflow:auto;border-radius:10px}</style>"
       "<script>"
       "async function upd(){let r=await fetch('/status');let j=await r.json();"
       "document.getElementById('status').innerText=`IP:${j.ip} | RSSI:${j.rssi}dBm | Uptime:${j.uptime}s | Heap:${j.heap} | UART:${j.baud} | Dist:${j.dist}cm`;"
       "document.getElementById('log').innerText=j.logs;}"
       "function go(p){fetch('/'+p).then(()=>upd());} setInterval(upd,1000);window.onload=upd;"
       "</script>";
  h += "<h3>ESP32 Robot Control</h3><div id='status'>...</div>";
  h += "<div class='pad'>"
       "<button onclick=\\\"go('leftup')\\\">&#8598;</button>"
       "<button onclick=\\\"go('go')\\\">&#8593;</button>"
       "<button onclick=\\\"go('rightup')\\\">&#8599;</button>"
       "<button onclick=\\\"go('left')\\\">&#8592;</button>"
       "<button class='stop' onclick=\\\"go('stop')\\\">STOP</button>"
       "<button onclick=\\\"go('right')\\\">&#8594;</button>"
       "<button onclick=\\\"go('leftdown')\\\">&#8601;</button>"
       "<button onclick=\\\"go('back')\\\">&#8595;</button>"
       "<button onclick=\\\"go('rightdown')\\\">&#8600;</button></div>";
  h += "<div class='row'><button class='rot' onclick=\\\"go('ccw')\\\">⟲ Rotate</button>"
       "<button class='rot' onclick=\\\"go('clockwise')\\\">⟳ Rotate</button></div>";
  h += "<div class='row'><button onclick=\\\"go('motorleft')\\\">Servo ◄</button>"
       "<button onclick=\\\"go('motorright')\\\">Servo ►</button>"
       "<button onclick=\\\"go('model1')\\\">Mode1</button>"
       "<button onclick=\\\"go('model2')\\\">Mode2</button>"
       "<button onclick=\\\"go('model3')\\\">Mode3</button>"
       "<button onclick=\\\"go('model4')\\\">Mode4</button></div>";
  h += "<h4>Logs</h4><div id='log'>...</div>";
  server.send(200,"text/html",h);
}
void jsonStatus(){
  String j="{";
  j += "\"ip\":\""+WiFi.localIP().toString()+"\",";
  j += "\"rssi\":"+String(WiFi.RSSI())+",";
  j += "\"uptime\":" + String(millis()/1000) + ",";
  j += "\"heap\":" + String(ESP.getFreeHeap()) + ",";
  j += "\"baud\":" + String(currentBaud) + ",";
  j += "\"dist\":" + String(lastDistance) + ",";
  String safe=logs; safe.replace("\\","\\\\"); safe.replace("\"","\\\""); safe.replace("\n","\\n");
  j += "\"logs\":\""+safe+"\"}";
  server.send(200,"application/json",j);
}
#define H(path, code, name) server.on(path, [=](){ sendByFormat(currentFmt, code, name); server.send(200,"text/plain","OK"); })

void setup(){
  Serial.begin(115200);
  botBegin(currentBaud);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while(WiFi.status()!=WL_CONNECTED){ delay(300); }
  appendLog("WiFi connected. IP: " + WiFi.localIP().toString());
  if (MDNS.begin("esp32-robot")) { MDNS.addService("http","tcp",80); appendLog("mDNS started: esp32-robot.local"); }
  ArduinoOTA.setHostname("esp32-robot"); ArduinoOTA.begin(); appendLog("OTA ready.");

  server.on("/", page); server.on("/status", jsonStatus);
  H("/go",0x5C,"Forward"); H("/back",0xA3,"Backward"); H("/stop",0x00,"Stop");
  H("/left",0x95,"Left"); H("/right",0x6A,"Right"); H("/clockwise",0x53,"Clockwise");
  H("/ccw",0xAC,"CounterClockwise"); H("/leftup",0x14,"LeftUp"); H("/leftdown",0x81,"LeftDown");
  H("/rightup",0x48,"RightUp"); H("/rightdown",0x22,"RightDown");
  H("/model1",0x19,"model1"); H("/model2",0x1A,"model2"); H("/model3",0x1B,"model3"); H("/model4",0x1C,"model4");
  H("/motorleft",0xE6,"MotorLeft"); H("/motorright",0xE7,"MotorRight");

  server.begin(); appendLog("HTTP server started.");
}

void loop(){
  ArduinoOTA.handle(); server.handleClient();

  while (BOT.available()) {
    int b = BOT.read(); if (b<0) break;
    String hb=" "; hb+=hex1((uint8_t)b); logs += "RX<-" + hb + "\\n"; if (logs.length()>10000) logs.remove(0, logs.length()-10000);
    char c = (char)b;
    if (c=='\n') { int v = rxLine.toInt(); if (v>0 && v<500) lastDistance = v; rxLine = ""; }
    else if (isPrintable(c)) { rxLine += c; if (rxLine.length()>32) rxLine.remove(0); }
  }
}

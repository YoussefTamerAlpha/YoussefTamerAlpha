#include <WiFi.h>
#include <WebServer.h>

// بيانات الواي فاي
const char* ssid = "Naser";
const char* password = "13102003";

IPAddress local_IP(192, 168, 1, 60);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

WebServer server(80);

// ==== أرجل الـ BTS7960 ====
#define R_PWM1  25   // Motor Right - Forward PWM
#define L_PWM1  26   // Motor Right - Backward PWM
#define R_EN1   33   // Motor Right - Enable
#define L_EN1   33   // Motor Right - Enable

#define R_PWM2  34   // Motor Left - Forward PWM
#define L_PWM2  35   // Motor Left - Backward PWM
#define R_EN2   14   // Motor Left - Enable
#define L_EN2   13   // Motor Left - Enable

int speed = 100;  // سرعة افتراضية (0-255)

// صفحة HTML
const char* htmlPage = R"rawliteral(

<!DOCTYPE html>

<html><head><meta charset="utf-8"><title>روبوت 24V 500W</title>
<style>body{font-family:Arial;text-align:center;background:#222;color:#0f0;margin-top:40px}
button{width:120px;height:80px;font-size:24px;margin:10px;border-radius:15px;background:#0f0;color:#000}
.slider{width:80%;height:30px}</style></head><body>
<h1>روبوت 24V 500W BTS7960</h1>
<button ontouchstart="send('f')" ontouchend="send('s')" onmousedown="send('f')" onmouseup="send('s')">↑ تقدم</button><br>
<button ontouchstart="send('l')" ontouchend="send('s')" onmousedown="send('l')" onmouseup="send('s')">← يسار</button>
<button ontouchstart="send('s')" ontouchend="send('s')" onmousedown="send('s')" onmouseup="send('s')">⏹ توقف</button>
<button ontouchstart="send('r')" ontouchend="send('s')" onmousedown="send('r')" onmouseup="send('s')">يمين →</button><br>
<button ontouchstart="send('b')" ontouchend="send('s')" onmousedown="send('b')" onmouseup="send('s')">↓ تراجع</button><br><br>
<input type="range" min="80" max="255" value="180" class="slider" id="spd" onchange="send('v'+this.value)">
<span style="color:white">السرعة: <span id="val">180</span></span>
<script>
function send(cmd){fetch('/cmd?move='+cmd);}
var slider=document.getElementById("spd");
slider.oninput=function(){document.getElementById("val").innerHTML=this.value; send('v'+this.value);}
</script>
</body></html>
)rawliteral";

// دوال التحكم في BTS7960
void motorStop() {
analogWrite(R_PWM1, 0);  analogWrite(L_PWM1, 0);
analogWrite(R_PWM2, 0);  analogWrite(L_PWM2, 0);
digitalWrite(R_EN1, HIGH); digitalWrite(L_EN1, HIGH);
digitalWrite(R_EN2, HIGH); digitalWrite(L_EN2, HIGH);
}

void motorForward() {
digitalWrite(R_EN1, HIGH); digitalWrite(L_EN1, HIGH);
digitalWrite(R_EN2, HIGH); digitalWrite(L_EN2, HIGH);
analogWrite(R_PWM1, speed);  analogWrite(L_PWM1, 0);
analogWrite(R_PWM2, speed);  analogWrite(L_PWM2, 0);
}

void motorBackward() {
digitalWrite(R_EN1, HIGH); digitalWrite(L_EN1, HIGH);
digitalWrite(R_EN2, HIGH); digitalWrite(L_EN2, HIGH);
analogWrite(R_PWM1, 0);  analogWrite(L_PWM1, speed);
analogWrite(R_PWM2, 0);  analogWrite(L_PWM2, speed);
}

void motorLeft() {
digitalWrite(R_EN1, HIGH); digitalWrite(L_EN1, HIGH);
digitalWrite(R_EN2, HIGH); digitalWrite(L_EN2, HIGH);
analogWrite(R_PWM1, speed);  analogWrite(L_PWM1, 0);
analogWrite(R_PWM2, 0);     analogWrite(L_PWM2, speed);
}

void motorRight() {
digitalWrite(R_EN1, HIGH); digitalWrite(L_EN1, HIGH);
digitalWrite(R_EN2, HIGH); digitalWrite(L_EN2, HIGH);
analogWrite(R_PWM1, 0);     analogWrite(L_PWM1, speed);
analogWrite(R_PWM2, speed); analogWrite(L_PWM2, 0);
}

void setup() {
Serial.begin(115200);

// إعداد جميع البنات
pinMode(R_PWM1, OUTPUT); pinMode(L_PWM1, OUTPUT);
pinMode(R_EN1,  OUTPUT); pinMode(L_EN1,  OUTPUT);
pinMode(R_PWM2, OUTPUT); pinMode(L_PWM2, OUTPUT);
pinMode(R_EN2,  OUTPUT); pinMode(L_EN2,  OUTPUT);

// تفعيل الـ Enable من البداية
digitalWrite(R_EN1, HIGH); digitalWrite(L_EN1, HIGH);
digitalWrite(R_EN2, HIGH); digitalWrite(L_EN2, HIGH);

motorStop();

// الواي فاي والـ IP الثابت
WiFi.config(local_IP, gateway, subnet);
WiFi.begin(ssid, password);
while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
Serial.println("\nIP: " + WiFi.localIP().toString());

server.on("/", [](){ server.send(200, "text/html", htmlPage); });

server.on("/cmd", [](){
if (server.hasArg("move")) {
String cmd = server.arg("move");


  if (cmd == "f") motorForward();
  else if (cmd == "b") motorBackward();
  else if (cmd == "l") motorLeft();
  else if (cmd == "r") motorRight();
  else if (cmd == "s") motorStop();
  else if (cmd.startsWith("v")) {
    speed = cmd.substring(1).toInt();
    speed = constrain(speed, 80, 255);
    Serial.println("السرعة الجديدة: " + String(speed));
  }
}
server.send(200, "text/plain", "OK");

});

server.begin();
Serial.println("السيرفر شغال - افتح المتصفح على: " + WiFi.localIP().toString());
}

void loop() {
server.handleClient();
}

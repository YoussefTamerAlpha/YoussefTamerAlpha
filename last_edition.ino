#include <WiFi.h>
#include <WebServer.h>

// ================== PAN TILT PINS ==================
#define PAN_PIN  27
#define TILT_PIN 32

int panAngle  = 90;
int tiltAngle = 90;

// اتجاهات الحركة الحالية (-1 = يسار/أسفل, 0 = متوقف, 1 = يمين/أعلى)
int panDirection  = 0;
int tiltDirection = 0;

unsigned long lastServoUpdate = 0;
const int continuousDelay = 15;  // سرعة الحركة المستمرة (كل 15ms درجة واحدة)

// =======================================================

// بيانات الواي فاي
const char* ssid = "Naser";
const char* password = "13102003";

IPAddress local_IP(192, 168, 1, 60);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

WebServer server(80);

// ==== أرجل الـ BTS7960 ====
#define R_PWM1  25
#define L_PWM1  26
#define R_EN1   33
#define L_EN1   33

#define R_PWM2  18
#define L_PWM2  19
#define R_EN2   14
#define L_EN2   13

int speed = 180;

// ================== HTML مع حركة مستمرة محسنة ==================
const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>Robot Control</title>
<style>
body{font-family:Arial;text-align:center;background:#222;color:#0f0;margin-top:40px}
button{width:120px;height:70px;font-size:22px;margin:8px;border-radius:15px;background:#0f0;color:#000}
.slider{width:80%;height:30px}
</style>
</head>

<body>
<h1>Robot BTS7960 + Pan Tilt</h1>

<h2>🚗 Movement</h2>
<button ontouchstart="send('f')" ontouchend="send('s')" onmousedown="send('f')" onmouseup="send('s')">↑</button><br>
<button ontouchstart="send('l')" ontouchend="send('s')" onmousedown="send('l')" onmouseup="send('s')">←</button>
<button ontouchstart="send('s')" ontouchend="send('s')" onmousedown="send('s')" onmouseup="send('s')">■</button>
<button ontouchstart="send('r')" ontouchend="send('s')" onmousedown="send('r')" onmouseup="send('s')">→</button><br>
<button ontouchstart="send('b')" ontouchend="send('s')" onmousedown="send('b')" onmouseup="send('s')">↓</button>

<h2>🎥 Pan Tilt</h2>
<button onmousedown="startMove('pl')" onmouseup="stopMove()" ontouchstart="startMove('pl')" ontouchend="stopMove()">Pan ◀</button>
<button onmousedown="startMove('pr')" onmouseup="stopMove()" ontouchstart="startMove('pr')" ontouchend="stopMove()">Pan ▶</button><br>
<button onmousedown="startMove('tu')" onmouseup="stopMove()" ontouchstart="startMove('tu')" ontouchend="stopMove()">Tilt ▲</button>
<button onmousedown="startMove('td')" onmouseup="stopMove()" ontouchstart="startMove('td')" ontouchend="stopMove()">Tilt ▼</button><br>
<button onclick="send('pc')">Center</button>

<br><br>
<h3>سرعة الموتور</h3>
<input type="range" min="80" max="255" value="180" class="slider" oninput="send('v'+this.value)">

<script>
function send(cmd){
  fetch('/cmd?move='+cmd);
}

function startMove(dir){
  window.currentDir = dir;
  fetch('/cmd?move=start_' + dir);  // أمر بدء الحركة
  window.moveInterval = setInterval(() => {
    fetch('/cmd?move='+window.currentDir);
  }, 100);
}

function stopMove(){
  fetch('/cmd?move=stop');  // أمر إيقاف الحركة
  clearInterval(window.moveInterval);
}
</script>

</body>
</html>
)rawliteral";
// =======================================================

// ================== MOTOR FUNCTIONS ==================
void motorStop() {
  analogWrite(R_PWM1, 0); analogWrite(L_PWM1, 0);
  analogWrite(R_PWM2, 0); analogWrite(L_PWM2, 0);
  digitalWrite(R_EN1, HIGH); digitalWrite(L_EN1, HIGH);
  digitalWrite(R_EN2, HIGH); digitalWrite(L_EN2, HIGH);
}

void motorForward() {
  digitalWrite(R_EN1, HIGH); digitalWrite(L_EN1, HIGH);
  digitalWrite(R_EN2, HIGH); digitalWrite(L_EN2, HIGH);
  analogWrite(R_PWM1, speed); analogWrite(L_PWM1, 0);
  analogWrite(R_PWM2, speed); analogWrite(L_PWM2, 0);
}

void motorBackward() {
  digitalWrite(R_EN1, HIGH); digitalWrite(L_EN1, HIGH);
  digitalWrite(R_EN2, HIGH); digitalWrite(L_EN2, HIGH);
  analogWrite(R_PWM1, 0); analogWrite(L_PWM1, speed);
  analogWrite(R_PWM2, 0); analogWrite(L_PWM2, speed);
}

void motorLeft() {
  digitalWrite(R_EN1, HIGH); digitalWrite(L_EN1, HIGH);
  digitalWrite(R_EN2, HIGH); digitalWrite(L_EN2, HIGH);
  analogWrite(R_PWM1, speed); analogWrite(L_PWM1, 0);
  analogWrite(R_PWM2, 0); analogWrite(L_PWM2, speed);
}

void motorRight() {
  digitalWrite(R_EN1, HIGH); digitalWrite(L_EN1, HIGH);
  digitalWrite(R_EN2, HIGH); digitalWrite(L_EN2, HIGH);
  analogWrite(R_PWM1, 0); analogWrite(L_PWM1, speed);
  analogWrite(R_PWM2, speed); analogWrite(L_PWM2, 0);
}
// =======================================================

// كتابة زاوية للسيرفو بـ LEDC
void writeServo(int pin, int angle) {
  angle = constrain(angle, 0, 180);
  uint32_t duty = map(angle, 0, 180, 1638, 8192);
  ledcWrite(pin, duty);
}

// ================== SETUP ==================
void setup() {
  Serial.begin(115200);

  // MOTOR PINS
  pinMode(R_PWM1, OUTPUT); pinMode(L_PWM1, OUTPUT);
  pinMode(R_EN1, OUTPUT);  pinMode(L_EN1, OUTPUT);
  pinMode(R_PWM2, OUTPUT); pinMode(L_PWM2, OUTPUT);
  pinMode(R_EN2, OUTPUT);  pinMode(L_EN2, OUTPUT);

  digitalWrite(R_EN1, HIGH); digitalWrite(L_EN1, HIGH);
  digitalWrite(R_EN2, HIGH); digitalWrite(L_EN2, HIGH);

  motorStop();

  // PAN TILT LEDC
  ledcAttach(PAN_PIN, 50, 16);
  ledcAttach(TILT_PIN, 50, 16);

  writeServo(PAN_PIN, panAngle);
  writeServo(TILT_PIN, tiltAngle);

  // WIFI
  WiFi.config(local_IP, gateway, subnet);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nWiFi connected. IP: " + WiFi.localIP().toString());

  server.on("/", [](){ server.send(200,"text/html",htmlPage); });

  server.on("/cmd", [](){
    if(server.hasArg("move")){
      String cmd = server.arg("move");

      // MOTOR COMMANDS
      if (cmd=="f") motorForward();
      else if (cmd=="b") motorBackward();
      else if (cmd=="l") motorLeft();
      else if (cmd=="r") motorRight();
      else if (cmd=="s") motorStop();
      else if (cmd.startsWith("v")){
        speed = constrain(cmd.substring(1).toInt(),80,255);
      }

      // PAN TILT CONTINUOUS - START MOVEMENT
      else if(cmd=="start_pl") panDirection = -1;
      else if(cmd=="start_pr") panDirection =  1;
      else if(cmd=="start_tu") tiltDirection =  1;
      else if(cmd=="start_td") tiltDirection = -1;
      
      // CONTINUE MOVEMENT (للحركة المستمرة)
      else if(cmd=="pl") panDirection = -1;
      else if(cmd=="pr") panDirection =  1;
      else if(cmd=="tu") tiltDirection =  1;
      else if(cmd=="td") tiltDirection = -1;
      
      // STOP COMMAND - إيقاف الحركة
      else if(cmd=="stop"){
        panDirection = 0;
        tiltDirection = 0;
      }
      
      else if(cmd=="pc"){ 
        panAngle = 90; tiltAngle = 90;
        panDirection = 0; tiltDirection = 0;
        writeServo(PAN_PIN, panAngle);
        writeServo(TILT_PIN, tiltAngle);
      }
    }

    server.send(200,"text/plain","OK");
  });

  server.begin();
  Serial.println("Server started");
}

void loop() {
  server.handleClient();

  // ===== CONTINUOUS SERVO MOVEMENT =====
  if(millis() - lastServoUpdate >= continuousDelay){
    lastServoUpdate = millis();

    if(panDirection != 0){
      panAngle += panDirection;
      panAngle = constrain(panAngle, 0, 180);
      writeServo(PAN_PIN, panAngle);
    }

    if(tiltDirection != 0){
      tiltAngle += tiltDirection;
      tiltAngle = constrain(tiltAngle, 0, 180);
      writeServo(TILT_PIN, tiltAngle);
    }
  }
}
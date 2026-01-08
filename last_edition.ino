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

// ================== HTML محسن ==================
const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Robot Control</title>
<style>
body{font-family:Arial;text-align:center;background:#222;color:#0f0;margin-top:20px;padding:10px}
button{width:100px;height:60px;font-size:18px;margin:6px;border-radius:10px;background:#0f0;color:#000;border:none;cursor:pointer;user-select:none}
button:active{background:#0c0}
.slider{width:90%;height:25px;margin:15px}
h1{font-size:24px;margin:10px}
h2{font-size:20px;margin:15px 0 10px}
h3{font-size:18px;margin:10px}
.container{max-width:500px;margin:0 auto}
@media (max-width: 600px) {
  button{width:80px;height:50px;font-size:16px}
  h1{font-size:20px}
}
</style>
</head>

<body>
<div class="container">
<h1>Robot Control</h1>

<h2>🚗 Movement</h2>
<button ontouchstart="sendMove('f')" ontouchend="sendStop()" onmousedown="sendMove('f')" onmouseup="sendStop()">↑</button><br>
<button ontouchstart="sendMove('l')" ontouchend="sendStop()" onmousedown="sendMove('l')" onmouseup="sendStop()">←</button>
<button onclick="sendStop()">■</button>
<button ontouchstart="sendMove('r')" ontouchend="sendStop()" onmousedown="sendMove('r')" onmouseup="sendStop()">→</button><br>
<button ontouchstart="sendMove('b')" ontouchend="sendStop()" onmousedown="sendMove('b')" onmouseup="sendStop()">↓</button>

<h2>🎥 Pan Tilt</h2>
<button ontouchstart="startServoMove('pl')" ontouchend="stopServoMove()" onmousedown="startServoMove('pl')" onmouseup="stopServoMove()">◀ Pan</button>
<button ontouchstart="startServoMove('pr')" ontouchend="stopServoMove()" onmousedown="startServoMove('pr')" onmouseup="stopServoMove()">Pan ▶</button><br>
<button ontouchstart="startServoMove('tu')" ontouchend="stopServoMove()" onmousedown="startServoMove('tu')" onmouseup="stopServoMove()">▲ Tilt</button>
<button ontouchstart="startServoMove('td')" ontouchend="stopServoMove()" onmousedown="startServoMove('td')" onmouseup="stopServoMove()">Tilt ▼</button><br>
<button onclick="centerServo()">Center</button>

<br><br>
<h3>Motor Speed</h3>
<input type="range" min="80" max="255" value="180" class="slider" oninput="sendSpeed(this.value)">

<script>
// متغيرات للتحكم في الحركة
let isMoving = false;
let activeMove = '';
let activeServoMove = '';
let moveTimeout = null;

// إرسال أمر تحريك الموتور
function sendMove(direction) {
  if (isMoving && activeMove === direction) return;
  
  // إيقاف أي حركة سابقة
  sendStop();
  
  isMoving = true;
  activeMove = direction;
  fetch('/cmd?move=' + direction);
}

// إيقاف الموتور
function sendStop() {
  if (!isMoving) return;
  
  isMoving = false;
  activeMove = '';
  fetch('/cmd?move=s');
  
  // إلغاء أي timeout
  if (moveTimeout) {
    clearTimeout(moveTimeout);
    moveTimeout = null;
  }
}

// بدء حركة السيرفو
function startServoMove(direction) {
  if (activeServoMove === direction) return;
  
  // إيقاف أي حركة سابقة
  if (activeServoMove) {
    fetch('/cmd?move=stop_servo');
  }
  
  activeServoMove = direction;
  fetch('/cmd?move=' + direction);
}

// إيقاف حركة السيرفو
function stopServoMove() {
  if (!activeServoMove) return;
  
  fetch('/cmd?move=stop_servo');
  activeServoMove = '';
}

// تثبيت السيرفو في المنتصف
function centerServo() {
  stopServoMove();
  fetch('/cmd?move=pc');
}

// إرسال السرعة
function sendSpeed(value) {
  fetch('/cmd?move=v' + value);
}

// منع السلوك الافتراضي للعناصر
function preventDefault(e) {
  e.preventDefault();
  e.stopPropagation();
  return false;
}

// منع التمرير عند لمس الشاشة
document.addEventListener('touchmove', function(e) {
  if (e.target.tagName === 'BUTTON') {
    e.preventDefault();
  }
}, { passive: false });

// إيقاف الحركة عند ترك الصفحة
document.addEventListener('visibilitychange', function() {
  if (document.hidden) {
    sendStop();
    stopServoMove();
  }
});

window.addEventListener('blur', function() {
  sendStop();
  stopServoMove();
});

// منع القائمة السياقية
document.addEventListener('contextmenu', preventDefault);
</script>

</div>
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
  Serial.println("Starting Robot Control...");

  // MOTOR PINS
  pinMode(R_PWM1, OUTPUT); pinMode(L_PWM1, OUTPUT);
  pinMode(R_EN1, OUTPUT);  pinMode(L_EN1, OUTPUT);
  pinMode(R_PWM2, OUTPUT); pinMode(L_PWM2, OUTPUT);
  pinMode(R_EN2, OUTPUT);  pinMode(L_EN2, OUTPUT);

  digitalWrite(R_EN1, HIGH); digitalWrite(L_EN1, HIGH);
  digitalWrite(R_EN2, HIGH); digitalWrite(L_EN2, HIGH);

  motorStop();
  Serial.println("Motor pins initialized");

  // PAN TILT LEDC
  ledcAttach(PAN_PIN, 50, 16);
  ledcAttach(TILT_PIN, 50, 16);

  writeServo(PAN_PIN, panAngle);
  writeServo(TILT_PIN, tiltAngle);
  Serial.println("Servo pins initialized");

  // WIFI
  WiFi.config(local_IP, gateway, subnet);
  Serial.print("Connecting to WiFi ");
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) { 
    delay(500); 
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nFailed to connect to WiFi!");
  }

  server.on("/", [](){ 
    Serial.println("Serving HTML page");
    server.send(200, "text/html", htmlPage); 
  });

  server.on("/cmd", [](){
    if(server.hasArg("move")){
      String cmd = server.arg("move");
      Serial.print("Received command: ");
      Serial.println(cmd);

      // MOTOR COMMANDS
      if (cmd == "f") {
        motorForward();
        Serial.println("Forward");
      }
      else if (cmd == "b") {
        motorBackward();
        Serial.println("Backward");
      }
      else if (cmd == "l") {
        motorLeft();
        Serial.println("Left");
      }
      else if (cmd == "r") {
        motorRight();
        Serial.println("Right");
      }
      else if (cmd == "s") {
        motorStop();
        Serial.println("Stop motors");
      }
      else if (cmd.startsWith("v")){
        speed = constrain(cmd.substring(1).toInt(), 80, 255);
        Serial.print("Speed set to: ");
        Serial.println(speed);
      }

      // PAN TILT COMMANDS
      else if(cmd == "pl") {
        panDirection = -1;
        tiltDirection = 0;
        Serial.println("Pan left");
      }
      else if(cmd == "pr") {
        panDirection = 1;
        tiltDirection = 0;
        Serial.println("Pan right");
      }
      else if(cmd == "tu") {
        tiltDirection = 1;
        panDirection = 0;
        Serial.println("Tilt up");
      }
      else if(cmd == "td") {
        tiltDirection = -1;
        panDirection = 0;
        Serial.println("Tilt down");
      }
      else if(cmd == "stop_servo") {
        panDirection = 0;
        tiltDirection = 0;
        Serial.println("Stop servo");
      }
      else if(cmd == "pc") { 
        panAngle = 90; 
        tiltAngle = 90;
        panDirection = 0; 
        tiltDirection = 0;
        writeServo(PAN_PIN, panAngle);
        writeServo(TILT_PIN, tiltAngle);
        Serial.println("Center servos");
      }
      else {
        Serial.print("Unknown command: ");
        Serial.println(cmd);
      }
    }

    server.send(200, "text/plain", "OK");
  });

  // Handle 404 errors
  server.onNotFound([](){
    Serial.println("404 - Page not found");
    server.send(404, "text/plain", "Page not found");
  });

  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();

  // ===== CONTINUOUS SERVO MOVEMENT =====
  unsigned long currentMillis = millis();
  
  if(currentMillis - lastServoUpdate >= continuousDelay){
    lastServoUpdate = currentMillis;

    // تحديث حركة Pan
    if(panDirection != 0){
      int newPanAngle = panAngle + panDirection;
      if(newPanAngle >= 0 && newPanAngle <= 180){
        panAngle = newPanAngle;
        writeServo(PAN_PIN, panAngle);
      } else {
        // إذا وصل إلى الحد الأقصى، توقف تلقائياً
        panDirection = 0;
      }
    }

    // تحديث حركة Tilt
    if(tiltDirection != 0){
      int newTiltAngle = tiltAngle + tiltDirection;
      if(newTiltAngle >= 0 && newTiltAngle <= 180){
        tiltAngle = newTiltAngle;
        writeServo(TILT_PIN, tiltAngle);
      } else {
        // إذا وصل إلى الحد الأقصى، توقف تلقائياً
        tiltDirection = 0;
      }
    }
  }
}

#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>

// بيانات شبكة الواي فاي
const char* ssid = "WE_5DCAF0";
const char* password = "32044461";

IPAddress local_IP(192, 168, 1, 60);   // IP ثابت للـ ESP32
IPAddress gateway(192, 168, 1, 1);     // البوابة
IPAddress subnet(255, 255, 255, 0);    // قناع الشبكة الفرعية

// سيرفر الويب
WebServer server(80);

// بنات درايفر المواتير (L298N)
#define ENA     13
#define IN1     12
#define IN2     14
#define ENB     27
#define IN3     26
#define IN4     25

// بنات السيرفو
#define PAN_PIN 15
#define TILT_PIN 4

// بن حساس رطوبة التربة الجديد
#define SOIL_MOISTURE_PIN 34 // GPIO 34 لمدخل حساس رطوبة التربة

Servo panServo;
Servo tiltServo;
int panPos = 90;  // وضع البان الأولي
int tiltPos = 90; // وضع التلت الأولي

// HTML page with pan/tilt sliders (نفس الـ HTML القديم)
const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Robot Controller</title>
  <style>
    body { font-family: Arial; text-align: center; background-color: #f0f0f0; }
    h1 { color: #333; }
    button {
      width: 100px;
      height: 60px;
      font-size: 18px;
      margin: 10px;
      border-radius: 10px;
    }
    input {
      font-size: 16px;
      padding: 5px;
      width: 250px;
      border-radius: 8px;
    }
    .controls, .servo-controls {
      margin: 20px auto;
    }
    iframe, img {
      margin-top: 20px;
      width: 320px;
      height: 240px;
      border: 2px solid #333;
      border-radius: 10px;
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
<body>
  <h1>🚗 متحكم روبوت ESP32</h1>

  <div class="controls">
    <button onclick="sendCommand('forward')">↑</button><br>
    <button onclick="sendCommand('left')">←</button>
    <button onclick="sendCommand('stop')">⏹</button>
    <button onclick="sendCommand('right')">→</button><br>
    <button onclick="sendCommand('backward')">↓</button>
  </div>

  <div class="servo-controls">
    <h2>🎯 التحكم في السيرفو</h2>
    <div class="slidecontainer">
      <p>بان (Pan):</p>
      <input type="range" min="0" max="180" value="90" class="slider" id="Pan" oninput="sendCommand('Pan,'+this.value)">
    </div>
    <div class="slidecontainer">
      <p>تلت (Tilt):</p>
      <input type="range" min="0" max="180" value="90" class="slider" id="Tilt" oninput="sendCommand('Tilt,'+this.value)">
    </div>
  </div>

  <h2>📷 بث الكاميرا المباشر</h2>

  <div>
    <input type="text" id="camIp" placeholder="أدخل IP الـ ESP32-CAM (مثال: 192.168.1.100)">
    <button onclick="updateCamStream()">📡 تحميل البث</button>
  </div>

  <img id="camStream" src="" alt="بث الكاميرا"/>

  <script>
    function sendCommand(cmd) {
      fetch("/cmd?move=" + cmd)
        .then(response => {
          if (!response.ok) {
            alert("خطأ في إرسال الأمر: " + cmd);
          }
        });
    }

    function updateCamStream() {
      const ip = document.getElementById('camIp').value.trim();
      if (ip !== "") {
        const camStream = document.getElementById('camStream');
        camStream.src = "http://" + ip + ":81/stream";
      } else {
        alert("من فضلك أدخل عنوان IP صالح!");
      }
    }
  </script>
</body>
</html>
)rawliteral";

// معالج الأوامر
void handleCommand(String cmd) {
  if (cmd == "forward") {
    digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW); analogWrite(ENA, 100);
    digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW); analogWrite(ENB, 100);
  } else if (cmd == "backward") {
    digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH); analogWrite(ENA, 100);
    digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH); analogWrite(ENB, 100);
  } else if (cmd == "left") {
    digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH); analogWrite(ENA, 150);
    digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW); analogWrite(ENB, 150);
  } else if (cmd == "right") {
    digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW); analogWrite(ENA, 150);
    digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH); analogWrite(ENB, 150);
  } else if (cmd == "stop") {
    digitalWrite(IN1, LOW); digitalWrite(IN2, LOW); analogWrite(ENA, 0);
    digitalWrite(IN3, LOW); digitalWrite(IN4, LOW); analogWrite(ENB, 0);
  } else if (cmd.startsWith("Pan")) {
    int commaIndex = cmd.indexOf(',');
    if (commaIndex != -1) {
      int value = cmd.substring(commaIndex + 1).toInt();
      panServo.write(value);
      panPos = value;
    }
  } else if (cmd.startsWith("Tilt")) {
    int commaIndex = cmd.indexOf(',');
    if (commaIndex != -1) {
      int value = cmd.substring(commaIndex + 1).toInt();
      tiltServo.write(value);
      tiltPos = value;
    }
  }
}

void setup() {
  Serial.begin(115200); // لاستقبال أوامر UART من الـ ESP32-CAM وطباعة قراءات الحساس

  // بنات درايفر المواتير
  pinMode(ENA, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(ENB, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  // بنات السيرفو
  panServo.attach(PAN_PIN);
  tiltServo.attach(TILT_PIN);
  panServo.write(panPos);
  tiltServo.write(tiltPos);

  // لا نحتاج لتعريف pinMode لـ SOIL_MOISTURE_PIN
  // لأن analogRead() بتظبطه كـ INPUT تلقائيًا

  // WiFi + Static IP
  if (!WiFi.config(local_IP, gateway, subnet)) {
    Serial.println("❌ فشل تهيئة IP الثابت");
  }

  WiFi.begin(ssid, password);
  Serial.print("جاري الاتصال بالواي فاي");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\n✅ تم الاتصال بالواي فاي. عنوان IP: ");
  Serial.println(WiFi.localIP());

  // مسارات سيرفر الويب
  server.on("/", []() {
    server.send(200, "text/html", htmlPage);
  });

  server.on("/cmd", []() {
    if (server.hasArg("move")) {
      String cmd = server.arg("move");
      handleCommand(cmd);
    }
    server.send(200, "text/plain", "OK");
  });

  server.begin();
  Serial.println("تم بدء سيرفر HTTP");
}

void loop() {
  server.handleClient();

  // معالجة أوامر UART من الـ ESP32-CAM
  if (Serial.available()) {
    String data = Serial.readStringUntil('\n');
    Serial.println("تم الاستلام: " + data);
    handleCommand(data);
  }

  // قراءة وعرض قيمة حساس رطوبة التربة
  int soilMoistureValue = analogRead(SOIL_MOISTURE_PIN);
  Serial.print("Soil Moisture: ");
  Serial.println(soilMoistureValue);

  delay(1000); // تأخير ثانية واحدة قبل قراءة الحساس مرة تانية
}

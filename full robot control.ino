#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <ESP32Servo.h>

const char* ssid = "WE_5DCAF0";
const char* password = "32044461";

IPAddress local_IP(192, 168, 1, 60);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

AsyncWebServer server(80);

#define ENA     13
#define IN1     12
#define IN2     14
#define ENB     27
#define IN3     26
#define IN4     25

#define PAN_PIN 15
#define TILT_PIN 4
#define SOIL_MOISTURE_PIN 34

Servo panServo;
Servo tiltServo;
int panPos = 90;
int tiltPos = 90;

const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Robot Controller</title>
  <style>
    body { font-family: Arial; text-align: center; background-color: #f0f0f0; }
    button {
      width: 100px; height: 60px; font-size: 18px; margin: 10px; border-radius: 10px;
    }
    input {
      font-size: 16px; padding: 5px; width: 250px; border-radius: 8px;
    }
    .controls, .servo-controls { margin: 20px auto; }
    img {
      margin-top: 20px; width: 320px; height: 240px; border: 2px solid #333; border-radius: 10px;
    }
    .slider {
      width: 100%; height: 20px; background: #d3d3d3; border-radius: 5px;
    }
    .slider::-webkit-slider-thumb, .slider::-moz-range-thumb {
      width: 40px; height: 40px; background: red; border-radius: 50%; cursor: pointer;
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
    <p>بان (Pan): <input type="range" min="0" max="180" value="90" class="slider" id="Pan" oninput="debounce('Pan,'+this.value)"></p>
    <p>تلت (Tilt): <input type="range" min="0" max="180" value="90" class="slider" id="Tilt" oninput="debounce('Tilt,'+this.value)"></p>
  </div>

  <h2>📷 بث الكاميرا</h2>
  <input type="text" id="camIp" placeholder="أدخل IP الـ ESP32-CAM">
  <button onclick="updateCamStream()">📡 تحميل البث</button>
  <img id="camStream" src="" alt="بث الكاميرا"/>

  <script>
    function sendCommand(cmd) {
      fetch("/cmd?move=" + cmd);
    }
    function updateCamStream() {
      const ip = document.getElementById('camIp').value.trim();
      document.getElementById('camStream').src = ip ? `http://${ip}:81/stream` : "";
    }
    let timeout;
    function debounce(cmd) {
      clearTimeout(timeout);
      timeout = setTimeout(() => sendCommand(cmd), 150);
    }
  </script>
</body>
</html>
)rawliteral";

void handleCommand(String cmd) {
  cmd.trim();
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
    int value = cmd.substring(cmd.indexOf(',') + 1).toInt();
    panServo.write(value);
    panPos = value;
  } else if (cmd.startsWith("Tilt")) {
    int value = cmd.substring(cmd.indexOf(',') + 1).toInt();
    tiltServo.write(value);
    tiltPos = value;
  }
}

TaskHandle_t soilTask;
void readSoilMoisture(void *parameter) {
  while (true) {
    int soilMoistureValue = analogRead(SOIL_MOISTURE_PIN);
    Serial.printf("Soil Moisture: %d\n", soilMoistureValue);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(ENA, OUTPUT); pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(ENB, OUTPUT); pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);

  panServo.attach(PAN_PIN); panServo.write(panPos);
  tiltServo.attach(TILT_PIN); tiltServo.write(tiltPos);

  if (!WiFi.config(local_IP, gateway, subnet)) Serial.println("❌ فشل تهيئة IP الثابت");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\n✅ تم الاتصال بالواي فاي. IP: " + WiFi.localIP().toString());

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", htmlPage);
  });

  server.on("/cmd", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("move")) {
      String cmd = request->getParam("move")->value();
      handleCommand(cmd);
    }
    request->send(200, "text/plain", "OK");
  });

  server.begin();
  Serial.println("✅ Web Server started.");

  xTaskCreatePinnedToCore(
    readSoilMoisture, "SoilReadTask", 1000, NULL, 1, &soilTask, 1);
}

void loop() {
  if (Serial.available()) {
    String data = Serial.readStringUntil('\n');
    handleCommand(data);
  }
}

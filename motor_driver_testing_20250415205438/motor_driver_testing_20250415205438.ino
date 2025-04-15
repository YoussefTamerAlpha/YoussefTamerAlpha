#define ENA 9
#define IN1 8
#define IN2 7
#define ENB 10
#define IN3 6
#define IN4 5

void setup() {
  pinMode(ENA, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(ENB, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  stopMotors(); // بدء التشغيل بإيقاف المحركات
}

void loop() {
  // اختبار الحركة الأمامية (50% سرعة)
  analogWrite(ENA, 80);
  analogWrite(ENB, 80);
  moveForward();
  delay(2000);

  // إيقاف كامل للمحركات
  stopMotors();
  delay(2000);

  // اختبار الدوران لليمين (30% سرعة)
  analogWrite(ENA, 80);
  analogWrite(ENB, 80);
  turnRight();
  delay(2000);

  // إيقاف كامل
  stopMotors();
  delay(2000);
}

void moveForward() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void turnRight() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

void stopMotors() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
  analogWrite(ENA, 0);
  analogWrite(ENB, 0);
}
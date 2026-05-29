#define BLYNK_TEMPLATE_ID "TMPL6g-nNe0HC"
#define BLYNK_TEMPLATE_NAME "ADFS System"
#define BLYNK_AUTH_TOKEN "lO-UmB8O_pYwnUcJ8M3I5rsJvjOKsxms"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <DHT.h>
#include <ESP32Servo.h>

// ---- WiFi Credentials ----
#define WIFI_SSID "amma"
#define WIFI_PASS "ammar123"

// ---- Pin Definitions ----
#define LDR_PIN 34
#define DHT_PIN 4
#define DHT_TYPE DHT11
#define TRIG_PIN 5
#define ECHO_PIN 18
#define SERVO_PIN 25

// ---- Thresholds ----
#define LIGHT_BRIGHT 1000
#define LIGHT_DARK 3000
#define TEMP_HOT 32
#define TEMP_COOL 25
#define PERSON_DISTANCE 20
#define OBSTACLE_DISTANCE 5

// ---- Objects ----
DHT dht(DHT_PIN, DHT_TYPE);
Servo facadeServo;
BlynkTimer timer;

int currentPosition = 90;
bool manualOverride = false;
int manualPosition = 90;

// ---- Blynk Switch Handler (V5) ----
BLYNK_WRITE(V5) {
  manualOverride = param.asInt();
  if (manualOverride) {
    Serial.println("Manual Override: ON");
    Blynk.virtualWrite(V4, "MANUAL OVERRIDE - ON");
  } else {
    Serial.println("Manual Override: OFF - Auto mode");
    Blynk.virtualWrite(V4, "AUTO MODE");
  }
}

// ---- Blynk Slider Handler (V6) ----
BLYNK_WRITE(V6) {
  manualPosition = param.asInt();
  if (manualOverride) {
    Serial.print("Manual Position: ");
    Serial.println(manualPosition);
    moveServoTo(manualPosition);
    Blynk.virtualWrite(V4, "MANUAL - Position: " + String(manualPosition));
  }
}

float getDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH);
  return duration * 0.034 / 2;
}

void moveServoTo(int targetPosition) {
  if (currentPosition == targetPosition) return;
  int step = (targetPosition > currentPosition) ? 5 : -5;
  while (currentPosition != targetPosition) {
    currentPosition += step;
    if (step > 0 && currentPosition > targetPosition) currentPosition = targetPosition;
    if (step < 0 && currentPosition < targetPosition) currentPosition = targetPosition;
    facadeServo.write(currentPosition);
    delay(15);
  }
}

void readAndControl() {
  int lightValue = analogRead(LDR_PIN);
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  float distance = getDistance();

  // ---- Print to Serial ----
  Serial.println("============================");
  Serial.print("Light: "); Serial.println(lightValue);
  Serial.print("Temp: "); Serial.print(temperature); Serial.println(" C");
  Serial.print("Humidity: "); Serial.print(humidity); Serial.println(" %");
  Serial.print("Distance: "); Serial.print(distance); Serial.println(" cm");

  // ---- Send to Blynk ----
  Blynk.virtualWrite(V0, lightValue);
  Blynk.virtualWrite(V1, temperature);
  Blynk.virtualWrite(V2, humidity);
  Blynk.virtualWrite(V3, distance);

  // ---- DHT11 Error Check ----
  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("DHT11 Error - Check wiring!");
    Blynk.virtualWrite(V4, "DHT11 Error!");
    return;
  }

  // ---- Skip auto logic if manual override ON ----
  if (manualOverride) {
    Serial.println("STATUS: MANUAL OVERRIDE ACTIVE");
    return;
  }

  // ---- Decision Logic ----

  // Safety first - obstacle too close
  if (distance < OBSTACLE_DISTANCE) {
    Serial.println("STATUS: OBSTACLE DETECTED - Holding!");
    Blynk.virtualWrite(V4, "OBSTACLE DETECTED!");
  }

  // Person approaching - open panels
  else if (distance < PERSON_DISTANCE) {
    Serial.println("STATUS: PERSON DETECTED - Opening!");
    Blynk.virtualWrite(V4, "PERSON DETECTED - Opening!");
    moveServoTo(180);
  }

  // Bright + Hot → Close (block sun and heat)
  else if (lightValue < LIGHT_BRIGHT && temperature > TEMP_HOT) {
    Serial.println("STATUS: BRIGHT & HOT - Closing panels!");
    Blynk.virtualWrite(V4, "BRIGHT & HOT - Closing!");
    moveServoTo(0);
  }

  // Bright + Cold → Open (let warmth in)
  else if (lightValue < LIGHT_BRIGHT && temperature < TEMP_COOL) {
    Serial.println("STATUS: BRIGHT & COLD - Opening panels!");
    Blynk.virtualWrite(V4, "BRIGHT & COLD - Opening!");
    moveServoTo(180);
  }

  // Hot but not bright → Close (block heat)
  else if (temperature > TEMP_HOT) {
    Serial.println("STATUS: HOT - Closing panels!");
    Blynk.virtualWrite(V4, "HOT - Closing!");
    moveServoTo(0);
  }

  // Dark → Open (let light in)
  else if (lightValue > LIGHT_DARK) {
    Serial.println("STATUS: DARK - Opening panels!");
    Blynk.virtualWrite(V4, "DARK - Opening!");
    moveServoTo(180);
  }

  // Moderate everything → Half open
  else {
    Serial.println("STATUS: MODERATE - Half shading");
    Blynk.virtualWrite(V4, "MODERATE - Half shading");
    moveServoTo(90);
  }
}

void setup() {
  Serial.begin(115200);
  dht.begin();

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  facadeServo.attach(SERVO_PIN);
  facadeServo.write(90);

  Serial.println("Connecting to WiFi...");
  Blynk.begin(BLYNK_AUTH_TOKEN, WIFI_SSID, WIFI_PASS);

  timer.setInterval(2000L, readAndControl);

  Serial.println("============================");
  Serial.println("  ADFS System Starting...   ");
  Serial.println("============================");
}

void loop() {
  Blynk.run();
  timer.run();
}
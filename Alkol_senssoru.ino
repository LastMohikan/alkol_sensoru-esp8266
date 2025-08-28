#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <time.h>
#include <PubSubClient.h>
#include <FS.h>

// OLED tanımları
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Wi-Fi bilgileri
const char* ssid     = "";
const char* password = "";

// Twilio WhatsApp bilgileri
const char* fromNumber = "";
const char* toNumber   = "";
const char* accountSID = "";
const char* authToken  = "";

// MQTT bilgileri (örnek broker)
const char* mqtt_server = "test.mosquitto.org";
WiFiClient espClient;
PubSubClient mqttClient(espClient);

// MQ-3 sensör
const int mq3Pin = A0;
const int threshold = 700;
bool mesajGonderildi = false;

// Buzzer ve LED pinleri
const int buzzerPin   = D5;
const int kirmiziLed  = D6;
const int yesilLed    = D7;

// Diğer
WiFiClientSecure client;
unsigned long previousBuzz = 0;
bool buzzerDurumu = false;

void setup() {
  Serial.begin(115200);

  // Pin ayarları
  pinMode(buzzerPin, OUTPUT);
  pinMode(kirmiziLed, OUTPUT);
  pinMode(yesilLed, OUTPUT);
  digitalWrite(buzzerPin, LOW);
  digitalWrite(kirmiziLed, LOW);
  digitalWrite(yesilLed, LOW);

  // OLED başlat
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED bulunamadi!");
    while (true);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println("MQ-3 Baslatiliyor...");
  display.display();

  // Wi-Fi bağlan
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  client.setInsecure();
  configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov"); 
  display.println("Wi-Fi baglandi!");
  display.display();

  // MQTT başlat
  mqttClient.setServer(mqtt_server, 1883);
  reconnectMQTT();

  // SPIFFS başlat
  if (!SPIFFS.begin()) {
    Serial.println("SPIFFS başlatılamadı");
  }
}

void loop() {
  int deger = analogRead(mq3Pin);
  Serial.print("MQ-3: ");
  Serial.println(deger);

  // Saat bilgisi
  time_t now = time(nullptr);
  struct tm* zaman = localtime(&now);
  char zamanStr[20];
  strftime(zamanStr, sizeof(zamanStr), "%d.%m.%Y %H:%M", zaman);

  // OLED güncelle
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("Alkol Degeri: ");
  display.println(deger);
  display.setCursor(0, 20);
  display.print("Saat: ");
  display.println(zamanStr);

  // MQTT verisi gönder
  char mqttMsg[100];
  snprintf(mqttMsg, sizeof(mqttMsg), "{\"deger\":%d, \"zaman\":\"%s\"}", deger, zamanStr);
  mqttClient.publish("esp8266/alkol", mqttMsg);

  // Dosyaya yaz
  File logFile = SPIFFS.open("/alkol_log.txt", "a+");
  if (logFile) {
    logFile.printf("%s -> %d\n", zamanStr, deger);
    logFile.close();
  }

  if (deger > threshold) {
    display.setCursor(0, 40);
    display.println("UYARI: Alkol yuksek!");

    digitalWrite(kirmiziLed, HIGH);
    digitalWrite(yesilLed, LOW);

    if (millis() - previousBuzz >= 500) {
      buzzerDurumu = !buzzerDurumu;
      digitalWrite(buzzerPin, buzzerDurumu ? HIGH : LOW);
      previousBuzz = millis();
    }

    if (!mesajGonderildi) {
      String msg = "MQ-3 alarm!\nDeger: " + String(deger) + "\nSaat: " + String(zamanStr);
      sendWhatsApp(msg);
      mesajGonderildi = true;
    }
  } else {
    digitalWrite(buzzerPin, LOW);
    digitalWrite(kirmiziLed, LOW);
    digitalWrite(yesilLed, HIGH);
    buzzerDurumu = false;

    if (deger < threshold - 50) {
      mesajGonderildi = false;
    }
  }

  display.display();
  mqttClient.loop();
  if (!mqttClient.connected()) reconnectMQTT();
  delay(100);
}

void reconnectMQTT() {
  while (!mqttClient.connected()) {
    if (mqttClient.connect("ESP8266Client")) {
      Serial.println("MQTT baglandi");
    } else {
      delay(1000);
    }
  }
}

void sendWhatsApp(String mesaj) {
  if (!client.connect("api.twilio.com", 443)) {
    Serial.println("Baglanti hatasi!");
    return;
  }

  String postData = "" + mesaj;
  String auth = "";

  client.println("POST /2010-04-01/Accounts/" + String(accountSID) + "/Messages.json HTTP/1.1");
  client.println("Host: api.twilio.com");
  client.println("Authorization: Basic " + auth);
  client.println("Content-Type: application/x-www-form-urlencoded");
  client.print("Content-Length: ");
  client.println(postData.length());
  client.println();
  client.print(postData);

  Serial.println("WhatsApp mesajı gönderildi.");
}

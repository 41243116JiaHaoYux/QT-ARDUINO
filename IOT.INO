#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// WiFi 帳密
const char *ssid = "wenwen 的 S24 Ultra"; // Enter your Wi-Fi name
const char *password = "********";       // Enter Wi-Fi password

// NTP 時間同步變數
const long gmtOffset_sec = 3600;         // 時間偏移量（秒）
const int daylightOffset_sec = 3600;     // 夏令時偏移量（秒）
const char *ntpServer = "pool.ntp.org";  // NTP 伺服器地址

// 硬體設備基本變數
char id[20] = "test-0001";
char wateringTime[10] = "00:00";
char localtimes[10] = "00:00";
char localtimesoff[10] = "00:00"; // 用於關燈
char lightStart[10] = "00:00";
short int duration = 120;
bool irrigation = false; // 是否正在澆水
#define L298N_1 2
#define L298N_2 4
#define Light 5
#define WaterSensor 18

// MQTT 基本資訊
char *mqtt_broker = "broker.hivemq.com";
char *topic = "wenwen/test";
char *mqtt_username = "wenwen";
char *mqtt_password = "test";
int mqtt_port = 1883;

// 初始化 MQTT
WiFiClient espClient;
PubSubClient client(espClient);

// 獲取當前時間（放入字串）
void LocalTime() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("Failed to obtain time");
        return;
    }

    char myHour[3], myMinute[3];
    strftime(myHour, sizeof(myHour), "%H", &timeinfo);
    strftime(myMinute, sizeof(myMinute), "%M", &timeinfo);
    snprintf(localtimes, sizeof(localtimes), "%s:%s", myHour, myMinute);
}

// 計算關燈時間
void LocalTimeoff() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("Failed to obtain time");
        return;
    }

    char myHour[3], myMinute[3];
    strftime(myHour, sizeof(myHour), "%H", &timeinfo);
    strftime(myMinute, sizeof(myMinute), "%M", &timeinfo);

    // 模擬延遲 12 小時的計算（簡化處理）
    int hour = atoi(myHour);
    hour = (hour + 12) % 24;
    snprintf(myHour, sizeof(myHour), "%02d", hour);

    snprintf(localtimesoff, sizeof(localtimesoff), "%s:%s", myHour, myMinute);
}

// 澆水檢查
bool waterCheck() {
    return analogRead(WaterSensor) < 100;
}

// 開燈
void openLight() {
    digitalWrite(Light, HIGH);
}

// 關燈
void offLight() {
    digitalWrite(Light, LOW);
}

// 開啟水泵
void openWater() {
    digitalWrite(L298N_1, HIGH);
}

// 關閉水泵
void offWater() {
    digitalWrite(L298N_1, LOW);
}

// 初始化 Wi-Fi 和 MQTT
void setup() {
    Serial.begin(115200);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.println("連接 Wi-Fi...");
    }
    Serial.println("成功連接!");

    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

    client.setServer(mqtt_broker, mqtt_port);
    while (!client.connected()) {
        String client_id = "esp32-client-" + String(WiFi.macAddress());
        if (client.connect(client_id.c_str(), mqtt_username, mqtt_password)) {
            Serial.println("MQTT 伺服器已連接!");
        } else {
            Serial.print("MQTT 連接失敗, 狀態: ");
            Serial.println(client.state());
            delay(2000);
        }
    }
    client.subscribe(topic);
}

// 主循環邏輯
void loop() {
    client.loop(); // 處理 MQTT 客戶端
    LocalTime();   // 更新當前時間

    // 開燈邏輯
    if (String(localtimes) == String(lightStart)) {
        openLight();
        LocalTimeoff(); // 設定關燈時間
    }

    // 關燈邏輯
    if (String(localtimes) == String(localtimesoff)) {
        offLight();
    }

    // 澆水邏輯
    if (irrigation) {
        if (!waterCheck()) {
            offWater();
            irrigation = false;
            Serial.println("澆水完成，已關閉水泵");
        }
    } else if (String(localtimes) == String(wateringTime)) {
        openWater();
        irrigation = true;
        Serial.println("已開始澆水");
    }

    delay(1000);
}

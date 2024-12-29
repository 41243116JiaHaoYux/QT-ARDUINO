#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// WiFi 帳密
const char *ssid = "wenwen 的 S24 Ultra"; // Enter your Wi-Fi name
const char *password = "********";  // Enter Wi-Fi password

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 28800;//格林威治時間,一格3600,GMT+8就是8*3600=28800
const int   daylightOffset_sec = 3600;
// 硬體設備基本變數
char id[20] = "test-0001";
char wateringTime[10] = "00:00";
char localtimes[10]= "00:00";
char localtimesoff[10]= "00:00";　//用於關燈
char lightStart[10] = "00:00";
short int duration = 120;
bool irrigation = false; // 是否正在澆水
#define L298N_1 2
#define L298N_2 4
#define Light   5
#define WaterSensor   18

// MQTT 基本資訊
char *mqtt_broker = "broker.hivemq.com";
char *topic = "wenwen/test";
char *mqtt_username = "wenwen";
char *mqtt_password = "test";
int mqtt_port = 1883;

// 初始化 MQTT
WiFiClient espClient;
PubSubClient client(espClient);

// 標誌變數，避免處理自己發送的訊息
bool isPublishing = false;

// 獲取當前資料
StaticJsonDocument<512> getCurrentData() {
    StaticJsonDocument<512> returnJson;
    returnJson["id"] = id;
    returnJson["wateringTime"] = wateringTime;
    returnJson["lightStart"] = lightStart;
    returnJson["duration"] = duration;

    return returnJson;
}

// 更新資料
StaticJsonDocument<512> updateData(StaticJsonDocument<512> updateData) {
    StaticJsonDocument<512> returnJson;
    returnJson["id"] = updateData["id"];
    returnJson["command"] = updateData["command"];

    if (updateData.containsKey("wateringTime") && updateData.containsKey("lightStart") && updateData.containsKey("duration")) {
        // 更新全域變數
        strncpy(wateringTime, updateData["wateringTime"], sizeof(wateringTime));
        strncpy(lightStart, updateData["lightStart"], sizeof(lightStart));
        duration = updateData["duration"].as<int>();

        returnJson["status"] = "success";
        returnJson["error"] = "";
    } else {
        returnJson["status"] = "error";
        returnJson["error"] = "未包含 wateringTime lightStart duration";
    }
    return returnJson;
}

// 即時澆水
StaticJsonDocument<512> realTimeIrrigation(StaticJsonDocument<512> updateData){
    StaticJsonDocument<512> returnJson;

    returnJson["id"] = updateData["id"];
    returnJson["command"] = updateData["command"];

    if(irrigation == true){
        returnJson["status"] = "error";
        returnJson["error"] = "當前正在澆水";
    }
else{
irrigation=true;
returnJson["status"] = "sucess";
returnJson["error"] = "";
}
    return returnJson;
}

// 處理未知命令
StaticJsonDocument<512> unknownCommand(StaticJsonDocument<512> updateData){
    StaticJsonDocument<512> returnJson;

    returnJson["id"] = updateData["id"];
    returnJson["command"] = updateData["command"];
    returnJson["status"] = "error";
    returnJson["error"] = "未知的命令";
    return returnJson;
}

// 發送 MQTT 訊息
void publishMessage(String message) {
    isPublishing = true; // 標記為正在發送
    client.publish(topic, message.c_str());
    isPublishing = false; // 發送完畢後重置標誌
}

// 處理 MQTT 訊息回呼
void callback(char *topic, byte *payload, unsigned int length) {
    // 如果正在發送訊息，則跳過處理
    if (isPublishing) {
        Serial.println("跳過自己發送的訊息");
        return;
    }

    Serial.print("接收訊息: ");
    Serial.println(topic);

    // 將 payload 轉換為字串格式
    String message;
    for (int i = 0; i < length; i++) {
        message += (char)payload[i];
    }

    // 嘗試解析 JSON
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, message);

    if (error) {
        Serial.print("Message: ");
        Serial.println(message);
        Serial.print("接收到的並不是 JSON 檔案: ");
        Serial.println(error.c_str());
    } else {
        Serial.println("成功接收 JSON 檔案");

        // 確認 ID
        if (!doc.containsKey("id") || doc["id"] != id) {
            Serial.println("沒獲取到 ID 或 ID 不正確");
            return;
        }

        // 命令處理
        if (doc.containsKey("command")) {
            String command = doc["command"].as<String>();
            StaticJsonDocument<512> result;

            if (command == "getCurrentData") {
                result = getCurrentData();
            } else if (command == "updateData") {
                result = updateData(doc);
            } else if (command == "realTimeIrrigation") {
                result = realTimeIrrigation(doc);
            } else {
                result = unknownCommand(doc);
            }

            // 發送回應
            String response;
            serializeJson(result, response);
            publishMessage(response);
        }
    }
}
//澆水
void openWater(){
    digitalWrite(L298N_1,HIGH);
}
//如果感測到盆底流出水就停止澆水
void offWater(){
    digitalWrite(L298N_1,LOW);
}
//開燈
void openLight(){
    digitalWrite(Light,HIGH);
}
//關燈
void offLight(){
    digitalWrite(Light,LOW);
}
// 初始化 Wi-Fi 和 MQTT
void setup() {
    Serial.begin(115200);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.println("連接 Wi-Fi..");
    }
    Serial.println("成功連接!");

    client.setServer(mqtt_broker, mqtt_port);
    client.setCallback(callback);
    // Init and get the time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  LocalTime();
    while (!client.connected()) {
        String client_id = "esp32-client-";
        client_id += String(WiFi.macAddress());
        Serial.printf("The client %s connects to the public MQTT broker\n", client_id.c_str());
        if (client.connect(client_id.c_str(), mqtt_username, mqtt_password)) {
            Serial.println("Public EMQX MQTT broker connected");
        } else {
            Serial.print("failed with state ");
            Serial.println(client.state());
            delay(2000);
        }
    }
    client.subscribe(topic);
}
//獲取時間(放loop)
void LocalTime(){
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");//參考C++的strftime語法就會知道為甚麼這樣寫
  Serial.print("Day of week: ");
  Serial.println(&timeinfo, "%A");
  Serial.print("Month: ");
  Serial.println(&timeinfo, "%B");
  Serial.print("Day of Month: ");
  Serial.println(&timeinfo, "%d");
  Serial.print("Year: ");
  Serial.println(&timeinfo, "%Y");
  Serial.print("Hour: ");
  Serial.println(&timeinfo, "%H");
  Serial.print("Hour (12 hour format): ");
  Serial.println(&timeinfo, "%I");
  Serial.print("Minute: ");
  Serial.println(&timeinfo, "%M");
  Serial.print("Second: ");
  Serial.println(&timeinfo, "%S");

  Serial.println("TEST variables");//抓取時間資料裡的"小時"存成字串做測試,以便後續應用
  char myHour[2];
  char myMinute[2];
  
  strftime(myHour,3, "%H", &timeinfo);
  strftime(myMinute,3, "%M", &timeinfo);
  localtimes[0]=myHour;
  localtimes[5]=':';
  localtimes[6]=myMimute;
  
 }

//用於關燈
void LocalTimeoff(){
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");//參考C++的strftime語法就會知道為甚麼這樣寫
  Serial.print("Day of week: ");
  Serial.println(&timeinfo, "%A");
  Serial.print("Month: ");
  Serial.println(&timeinfo, "%B");
  Serial.print("Day of Month: ");
  Serial.println(&timeinfo, "%d");
  Serial.print("Year: ");
  Serial.println(&timeinfo, "%Y");
  Serial.print("Hour: ");
  Serial.println(&timeinfo, "%H");
  Serial.print("Hour (12 hour format): ");
  Serial.println(&timeinfo, "%I");
  Serial.print("Minute: ");
  Serial.println(&timeinfo, "%M");
  Serial.print("Second: ");
  Serial.println(&timeinfo, "%S");

  Serial.println("TEST variables");//抓取時間資料裡的"小時"存成字串做測試,以便後續應用
  char myHour[2];
  char myMinute[2];
  
  strftime(myHour,3, "%H", &timeinfo);
  strftime(myMinute,3, "%M", &timeinfo);
  localtimes[0]=myHour;
  localtimes[5]=':';
  localtimes[6]=myMimute;
  myHour[0]+='1'; //12小時後關燈
  myHour[1]+='2';
  localtimesoff[0]=myHour";
  localtimesoff[5]=':';
  localtimesoff[6]=myMimute;
 }

bool waterCheck(){//檢查水是否澆透(澆透則loop停止澆水)
  if(analogRead(WaterSensor)>=100)
    return false;
  else
    return true;
}
void loop() {
    client.loop(); // 處理 MQTT 客戶端
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    LocalTime(); // 更新當前時間
    
    // 開燈判斷
    if (String(localtimes) == String(lightStart)) { 
        openLight();
        configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
        LocalTimeoff(); // 設置關燈時間
    }
    
    // 關燈判斷
    if (String(localtimes) == String(localtimesoff)) { 
        offLight();
    }

    // 澆水邏輯
    if (irrigation) { 
        if (!waterCheck()) { 
            offWater(); // 如果水已澆透，關閉水泵
            irrigation = false; // 更新狀態
            Serial.println("澆水完成，已關閉水泵");
        }
    } else {
        // 如果當前時間等於設定的澆水時間，開啟水泵
        if (String(localtimes) == String(wateringTime)) { 
            openWater();
            irrigation = true; // 更新澆水狀態
            Serial.println("已開始澆水");
        }
    }

    delay(1000); // 防止過於頻繁地輪詢
}

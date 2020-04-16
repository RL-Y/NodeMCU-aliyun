#include <ESP8266WiFi.h>
#include <PubSubClient.h> 
#include <ArduinoJson.h>

#define PRESS_MIN 20      //最小量程是20g
#define PRESS_MAX 2000    //最大量程是2kg
#define VOLTAGE_MIN 100   //最小电压0.1v
#define VOLTAGE_MAX 3300  //最大电压3.3v

int relayPin = 2; //继电器引脚（GPIO2对应nodemcu的D4，本继电器为低电平触发（另有高电平触发））
int sensorPin = A0;   //压力传感器的引脚

#define WIFI_SSID         "XXXXXX"   //WIFI SSID
#define WIFI_PASSWD       "XXXXX"   //WIFI密码

//三元组
#define PRODUCT_KEY       "XXXXX"
#define DEVICE_NAME       "XXXXX"
#define DEVICE_SECRET     "XXXXXXXXXXXXXXXXX"
#define REGION_ID         "cn-shanghai"
 
//线上环境域名和端口号
#define MQTT_SERVER       PRODUCT_KEY ".iot-as-mqtt." REGION_ID ".aliyuncs.com"
#define MQTT_PORT         1883
#define MQTT_USRNAME      DEVICE_NAME "&" PRODUCT_KEY
 
// 生成passwd：加密明文是参数和对应的值（clientId${esp8266}deviceName${deviceName}productKey${productKey}timestamp${1234567890}）按字典顺序拼接,密钥是设备的DeviceSecret
#define CLIENT_ID    "ESP8266|securemode=3,signmethod=hmacsha1,timestamp=1234567890|"
#define MQTT_PASSWD       "XXXXXXXXXXXXXXXXXX"

//topic-发布和订阅
#define ALINK_BODY_FORMAT_POST        "{\"id\":\"123\",\"version\":\"1.0\",\"method\":\"thing.event.property.post\",\"params\":%s}"   //消息体字符串格式和method参数
#define ALINK_TOPIC_PROP_POST         "/sys/" PRODUCT_KEY "/" DEVICE_NAME "/thing/event/property/post"   //发布，设备属性上报
#define ALINK_TOPIC_PROP_SET          "/sys/" PRODUCT_KEY "/" DEVICE_NAME "/thing/service/property/set"  //订阅，设备属性设置

unsigned long lastMs = 0;
WiFiClient espClient;     // 创建WiFiClient实例
PubSubClient  client(espClient);  //创建Client实例
 
//监听云端下发的指令及解析订阅的topic
void callback(char *topic, byte *payload, unsigned int length)
{
    if (strstr(topic, ALINK_TOPIC_PROP_SET)){ 
      Serial.print("Message arrived [");
      Serial.print(topic);
      Serial.print("] ");
      payload[length] = '\0';
      Serial.println((char *)payload);
      
      DynamicJsonDocument doc(100);
      DeserializationError error = deserializeJson(doc, payload);
      if (error){
        Serial.println("parse json failed");
        return;
        }
        
        //将字符串payload转换为json格式的对象
        // {"method":"thing.service.property.set","id":"282860794","params":{"jidianqi":1},"version":"1.0.0"}
        JsonObject setAlinkMsgObj = doc.as<JsonObject>();       
        int val = setAlinkMsgObj["params"]["jidianqi"];
        if (val == HIGH){
          digitalWrite(relayPin, HIGH);
          }
          else if (val == LOW){
            digitalWrite(relayPin, LOW);
            delay(2000);
            digitalWrite(relayPin, HIGH);
            }
     }
}
 
//连接Wifi 
void wifiInit()
{
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWD);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(1000);
        Serial.println("WiFi not Connect");
    }
 
    Serial.println("Connected to AP");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    client.setServer(MQTT_SERVER, MQTT_PORT);   //连接WiFi之后，连接MQTT服务器
    client.setCallback(callback);   // 设置回调监听云端下发的指令
}
 
//连接Mqtt 
void mqttCheckConnect()
{
    while (!client.connected())
    {
        Serial.println("Connecting to MQTT Server ...");
        if (client.connect(CLIENT_ID, MQTT_USRNAME, MQTT_PASSWD))
        {
            Serial.println("MQTT Connected!");
            client.subscribe(ALINK_TOPIC_PROP_SET); // 订阅属性设置Topic
            Serial.println("subscribe done");
        }
        else
        {
            Serial.print("MQTT Connect err:");
            Serial.println(client.state());
            delay(5000);
        }
    }
}
 
//上报数据
void mqttIntervalPost()
{
    char param[32];
    char jsonBuf[128];
    long Fdata = getPressValue(sensorPin);
    
    //sprintf是格式化字符串
    sprintf(param, "{\"jidianqi\":%d,\"zhongliang\":%d}",digitalRead(relayPin),Fdata);  //产品标识符,数据上传    
    sprintf(jsonBuf, ALINK_BODY_FORMAT_POST, param);
    
    Serial.println(jsonBuf);
    boolean a_POST = client.publish(ALINK_TOPIC_PROP_POST, jsonBuf);//上报属性Topic数据
    Serial.print("publish:0失败;1成功;");
    Serial.println(a_POST);
}

void setup() 
{
    pinMode(relayPin, OUTPUT);   //继电器输出模式
    digitalWrite(relayPin,HIGH); //初始化继电器高电平
    Serial.begin(115200);
    Serial.println("Demo Start");
    wifiInit();
}

void loop()
{
    if (millis() - lastMs >= 5000)
    {
        lastMs = millis();
        mqttCheckConnect(); // MQTT上云
        mqttIntervalPost();//上报消息心跳周期
    }
    client.loop();
}

//FSR薄膜压力传感器（转换）
long getPressValue(int pin)
{
  long PRESS_AO = 0;            //重量
  int VOLTAGE_AO = 0;           //电压
  int value = analogRead(pin);  //AD

  VOLTAGE_AO = map(value, 0, 1023, 0, 3300);
  if(VOLTAGE_AO < VOLTAGE_MIN)
  {
    PRESS_AO = 0;
  }
  else if(VOLTAGE_AO > VOLTAGE_MAX)
  {
    PRESS_AO = PRESS_MAX;
  }
  else
  {
    PRESS_AO = map(VOLTAGE_AO, VOLTAGE_MIN, VOLTAGE_MAX, PRESS_MIN, PRESS_MAX);
  }
  return PRESS_AO;
}

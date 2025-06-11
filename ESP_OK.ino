#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <mcp2515.h>
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>
#include <WiFi.h>

// WiFi AP SSID and password
#define WIFI_SSID "4 chi em"
#define WIFI_PASSWORD "daytro6868"

// InfluxDB configuration
#define INFLUXDB_URL "https://us-east-1-1.aws.cloud2.influxdata.com"
#define INFLUXDB_TOKEN "-lhE67rj108Jl99XLZHk5gXc9g2_HUPqx_DsAFyWuxMEWQuV7YZRkyuTHWAUyRSAMrabFwfFI3KJBP7DzesLhQ=="
#define INFLUXDB_ORG "2261cb1c5f48f043"
#define INFLUXDB_BUCKET "tudeptrai07"
#define TZ_INFO "UTC7"

// Declare InfluxDB client instance with preconfigured InfluxCloud certificate
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);

// Declare Data point
Point sensor("wifi_status");

// =================== Cấu hình chân cho ESP32-C3 ===================
#define TRIG_PIN 2
#define ECHO_PIN 3
#define SPI_SCK   6
#define SPI_MOSI  7
#define SPI_MISO  4
#define SPI_CS    5

// =================== MCP2515 ===================
MCP2515* mcp2515;

// =================== Biến toàn cục ===================
volatile char receivedText[9] = "";
volatile int distance_cm = -1;
volatile bool canSendSuccess = false;
volatile int consecutiveFailCount = 0;
volatile int consecutiveSuccessCount = 0;
volatile int totalSuccess = 0;
volatile int totalFail = 0;
volatile int accuracy = 0;

const int FAIL_THRESHOLD = 3;
const int SUCCESS_THRESHOLD = 2;

// Mutex for protecting shared resources
SemaphoreHandle_t xMutex;

// =================== Đọc khoảng cách siêu âm ===================
void readDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(5);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  xSemaphoreTake(xMutex, portMAX_DELAY);
  if (duration > 0) {
    distance_cm = duration * 0.034 / 2;
  } else {
    distance_cm = -1;
  }
  xSemaphoreGive(xMutex);
}

// =================== Task đo siêu âm ===================
void vTaskUltrasonic(void *pvParameters) {
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(100);

  while (1) {
    readDistance();
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
}

// =================== Task nhận CAN ===================
void vTaskReceiveCAN(void *pvParameters) {
  struct can_frame recvFrame;
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(100);

  while (1) {
    if (mcp2515->readMessage(&recvFrame) == MCP2515::ERROR_OK) {
      if (recvFrame.can_id == 0x038 && recvFrame.can_dlc <= 8) {
        xSemaphoreTake(xMutex, portMAX_DELAY);
        memcpy((void*)receivedText, recvFrame.data, recvFrame.can_dlc);
        receivedText[recvFrame.can_dlc] = '\0';
        xSemaphoreGive(xMutex);

        Serial.print("Received CAN data: ");
        Serial.println((const char*)receivedText);
      }
    }
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
}

// =================== Task gửi CAN ===================
void vTaskSendCAN(void *pvParameters) {
  struct can_frame canMsg;
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(100);

  while (1) {
    xSemaphoreTake(xMutex, portMAX_DELAY);
    int dist = distance_cm;
    xSemaphoreGive(xMutex);

    canMsg.can_id = 0x036;
    canMsg.can_dlc = 2;
    canMsg.data[0] = (dist >> 8) & 0xFF;
    canMsg.data[1] = dist & 0xFF;

    Serial.print("Distance: ");
    Serial.print(dist);
    Serial.println(" cm");

    int ret = mcp2515->sendMessage(&canMsg);

    xSemaphoreTake(xMutex, portMAX_DELAY);
    if (ret == MCP2515::ERROR_OK) {
      canSendSuccess = true;
      consecutiveFailCount = 0;
      consecutiveSuccessCount++;
      totalSuccess++;
      Serial.println("CAN Send OK!");

      // In thống kê
      int totalCount = totalSuccess + totalFail;
      int percentSuccess = (totalCount > 0) ? (totalSuccess * 100 / totalCount) : 0;
      accuracy = percentSuccess;
      Serial.print("Success Rate: ");
      Serial.print(percentSuccess);
      Serial.print("% (");
      Serial.print(totalSuccess);
      Serial.print("/");
      Serial.print(totalCount);
      Serial.println(")");
    } else {
      canSendSuccess = false;
      consecutiveSuccessCount = 0;
      consecutiveFailCount++;
      totalFail++;
      int totalCount = totalSuccess + totalFail;
      int percentSuccess = (totalCount > 0) ? (totalSuccess * 100 / totalCount) : 0;
      accuracy = percentSuccess;

      if (consecutiveFailCount > 10) consecutiveFailCount = 10;
      Serial.print("CAN Send FAILED! (Consecutive fails: ");
      Serial.print(consecutiveFailCount);
      Serial.println(")");
    }
    xSemaphoreGive(xMutex);

    vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
}

// =================== Task gửi dữ liệu lên InfluxDB ===================
void InfluxDB_Task(void *pvParameters) {
  // Add tags for InfluxDB
  sensor.addTag("device", "ESP32-C3");
  sensor.addTag("sensor", "ultrasonic");

  while (true) {
    int curDistance;
    int acc;
    xSemaphoreTake(xMutex, portMAX_DELAY);
    curDistance = distance_cm;
    acc = accuracy;
    xSemaphoreGive(xMutex);

    sensor.clearFields();
    sensor.addField("distance", curDistance);
    sensor.addField("Accuracy", acc);

    if (WiFi.status() == WL_CONNECTED) {
      if (!client.writePoint(sensor)) {
        Serial.print("InfluxDB write failed: ");
        Serial.println(client.getLastErrorMessage());
      } else {
        Serial.println("Data sent to InfluxDB");
      }
    } else {
      Serial.println("WiFi disconnected, skipping InfluxDB write");
    }

    vTaskDelay(pdMS_TO_TICKS(1000)); // Gửi mỗi giây
  }
}

// =================== Setup ===================
void setup() {
  Serial.begin(115200);
  delay(100);

  // Initialize mutex
  xMutex = xSemaphoreCreateMutex();
  if (xMutex == NULL) {
    Serial.println("Mutex creation failed!");
    while (1);
  }

  // Initialize SPI
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, SPI_CS);

  // Initialize WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(100);
  }
  Serial.println("\nWiFi connected");

  // Time sync for InfluxDB
  timeSync(TZ_INFO, "pool.ntp.org", "time.nist.gov");

  // Connect to InfluxDB
  if (client.validateConnection()) {
    Serial.print("Connected to InfluxDB: ");
    Serial.println(client.getServerUrl());
  } else {
    Serial.print("InfluxDB connection failed: ");
    Serial.println(client.getLastErrorMessage());
  }

  // Initialize MCP2515
  mcp2515 = new MCP2515(SPI_CS);
  mcp2515->reset();
  mcp2515->setBitrate(CAN_125KBPS, MCP_8MHZ);
  mcp2515->setNormalMode();

  Serial.println("ESP32-C3 CAN System Started");
  Serial.println("Tasks: Ultrasonic, SendCAN, ReceiveCAN, InfluxDB");

  // Create FreeRTOS tasks
  xTaskCreate(vTaskUltrasonic, "Ultrasonic", 2048, NULL, 2, NULL);
  xTaskCreate(vTaskSendCAN, "SendCAN", 2048, NULL, 2, NULL);
  xTaskCreate(vTaskReceiveCAN, "RecvCAN", 2048, NULL, 1, NULL);
  // xTaskCreate(InfluxDB_Task, "InfluxDB_Task", 4096, NULL, 1, NULL);
}

// =================== Loop (không dùng trong FreeRTOS) ===================
void loop() {
  // Không dùng trong FreeRTOS
}
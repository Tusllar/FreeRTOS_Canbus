#include <MapleFreeRTOS900.h>  // Hoặc FreeRTOS.h tùy vào hệ STM32
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SPI.h>
#include <mcp2515.h>

// Khai báo chân
#define SPI_CS PA4   // Chip Select cho MCP2515

// OLED
#define OLED_RESET -1
Adafruit_SSD1306 display(128, 64, &Wire, OLED_RESET);

// MCP2515
MCP2515* mcp2515;
// L298N Motor1 Driver pins
#define ENA PA0   // Enable A (PWM speed control)
#define IN1 PA1   // Input 1
#define IN2 PA2   // Input 2

// L298N Motor2 Driver pins
#define ENB PA8   // Enable B (PWM speed control)
#define IN3 PA9   // Input 3
#define IN4 PA10  // Input 4


// Biến toàn cục
volatile int distance_cm = -1;
volatile bool canConnected = false;
volatile int totalReceived = 0;
volatile int totalFailed = 0;
volatile bool motorstatus1 = false;
volatile bool motorstatus2 = false;
volatile bool canSend = false;

// THÊM VÀO: Khai báo biến Mutex ở phạm vi toàn cục
SemaphoreHandle_t xMutex;

// ==================== Motor Control ====================
void motor1Forward() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  analogWrite(ENA, 255);   // PWM từ 0–255
  motorstatus1 = true;     // SỬA LOGIC
}

void motor1Forward1() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  analogWrite(ENA, 150);   // PWM từ 0–255
  motorstatus1 = true;     // SỬA LOGIC
}
void motor1Stop() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  analogWrite(ENA, 180);
  motorstatus1 = false;    // SỬA LOGIC
}

void motor2Forward() {
  digitalWrite(IN4, HIGH);
  digitalWrite(IN3, LOW);
  analogWrite(ENB, 255);   // PWM từ 0–255
  motorstatus2 = true;     // SỬA LOGIC
}
void motor2Forward1() {
  digitalWrite(IN4, HIGH);
  digitalWrite(IN3, LOW);
  analogWrite(ENB, 150);   // PWM từ 0–255
  motorstatus2 = true;     // SỬA LOGIC
}

void motor2Stop() {
  digitalWrite(IN4, LOW);
  digitalWrite(IN3, LOW);
  analogWrite(ENB, 180);
  motorstatus2 = false;    // SỬA LOGIC
}
// ==================== Vẽ thanh khoảng cách ====================
void drawDistanceBar(int dist) {
  const int maxDist = 100;
  const int barWidthMax = 70;
  int displayDist = (dist > maxDist) ? maxDist : dist;
  int barLength = map(displayDist, 0, maxDist, 0, barWidthMax);

  // Vẽ khung thanh
  display.drawRect(55, 17, barWidthMax + 2, 12, WHITE);
  // Vẽ thanh thực tế
  if (dist >= 0) {
    display.fillRect(56, 18, barLength, 10, WHITE);
  }
}
// ==================== Điều khiển động cơ theo khoảng cách ====================
void vTaskMotorControl(void *pvParameters) {
    // Motor 1
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(ENA, OUTPUT);
      // Motor 2
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(ENB, OUTPUT);

  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(100); // Cập nhật điều khiển mỗi 100ms

  while (1) {
    int dist = distance_cm;
    // SỬA LOGIC: Dừng khi khoảng cách lớn hoặc không có dữ liệu
    if (dist > 30 || dist < 0) { 
        // motor2Stop();
        motor2Forward();
        motor1Forward();
        // motor1Stop();
      } else if(dist > 15 ) {
        motor2Forward1();
        motor1Forward1();
      } else {
        // motor2Forward();
        motor1Stop();
        motor2Stop();
        // motor1Forward();
      }
    
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
}
// ==================== Nhận CAN ====================
void vTaskReceiveCAN(void *pvParameters) {
  struct can_frame recvFrame;
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(50); // Kiểm tra mỗi 50ms

  while (1) {
    int ret = mcp2515->readMessage(&recvFrame);
    
    if (ret == MCP2515::ERROR_OK) {
      canConnected = true;
      totalReceived++;
      
      // Kiểm tra ID của tin nhắn khoảng cách
      if (recvFrame.can_id == 0x036 && recvFrame.can_dlc == 2) {
        // Ghép 2 byte thành khoảng cách
        distance_cm = (recvFrame.data[0] << 8) | recvFrame.data[1];
        
        Serial.print("Received distance: ");
        Serial.print(distance_cm);
        Serial.println(" cm");
      }
    } else if (ret == MCP2515::ERROR_NOMSG) {
      // Không có tin nhắn - bình thường
    } else {
      totalFailed++;
      Serial.println("CAN Read Error!");
    }
    
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
}

// ==================== Hiển thị OLED ====================
void vTaskDisplay(void *pvParameters) {
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setTextColor(WHITE);

  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(100); // Cập nhật màn hình mỗi 100ms

  while (1) {
    int dist = distance_cm;
    
    display.clearDisplay();

    // Tiêu đề
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("CAN Distance Monitor");
    
    // Đường kẻ phân cách
    display.drawLine(0, 10, 127, 10, WHITE);

    // Hiển thị khoảng cách
    display.setTextSize(2);
    display.setCursor(0, 15);
    if (dist >= 0) {
      display.print(dist);
      display.setTextSize(1);
      display.setCursor(40, 22);
      display.print("cm");
    } else {
      display.setTextSize(1);
      display.setCursor(0, 20);
      display.print("No Data");
    }

    // Vẽ thanh khoảng cách
    if (dist >= 0) {
      drawDistanceBar(dist);
    }

    // Trạng thái CAN
    display.setTextSize(1);
    display.setCursor(0, 35);
    display.print("CAN:");
    if (canConnected) {
      display.print("Con..");
    } else {
      display.print("Dis..");
    }

    // Thống kê
    display.setCursor(0, 45);
    display.print("RX:");
    display.print(totalReceived);

    // Status Motor1
    display.setCursor(60, 35);
    display.print("Motor 1:");
    if (motorstatus1) {
      display.print("ON");
    } else {
      display.print("OFF");
    }

    // Status Motor2
    display.setCursor(60, 45);
    display.print("Motor 2:");
    if (motorstatus2) {
      display.print("ON");
    } else {
      display.print("OFF");
    }

    display.setCursor(0, 55);
    display.print("Send:");
    if( canSend ){
      display.print("Ok!");
    } else {
      display.print("False?");
    }


    display.display();
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
}

// ==================== Kiểm tra kết nối CAN ====================
void vTaskCANStatus(void *pvParameters) {
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(1000); // Kiểm tra mỗi 1 giây

  int lastReceivedCount = 0;
  
  while (1) {
    // Kiểm tra xem có nhận được dữ liệu mới không
    if (totalReceived == lastReceivedCount) {
      canConnected = false;
    }
    lastReceivedCount = totalReceived;
    
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
}

// ==================== Setup ====================
void setup() {
  Serial.begin(115200);
  Wire.begin();
  SPI.begin();

  // Khởi tạo MCP2515
  mcp2515 = new MCP2515(SPI_CS);
  mcp2515->reset();
  mcp2515->setBitrate(CAN_125KBPS, MCP_8MHZ);
  mcp2515->setNormalMode();

  Serial.println("CAN Distance Monitor Started");

  // Tạo các task
  xTaskCreate(vTaskReceiveCAN, "ReceiveCAN", 256, NULL, 2, NULL);
  xTaskCreate(vTaskDisplay, "Display", 512, NULL, 1, NULL);
  xTaskCreate(vTaskCANStatus, "CANStatus", 128, NULL, 1, NULL);
  xTaskCreate(vTaskMotorControl, "MotorCtrl", 256, NULL, 1, NULL);

  // Bắt đầu scheduler
  vTaskStartScheduler();
}

void loop() {
  // Không sử dụng trong FreeRTOS
}
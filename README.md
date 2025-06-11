# FreeRTOS_Canbus
# Giao Tiếp CAN Bus Giữa ESP32-C3 và STM32F103C8T6 Ứng Dụng Đo Khoảng Cách và Điều Khiển 2 Động Cơ

## 📌 Mô tả dự án

Dự án này triển khai hệ thống **giao tiếp CAN Bus** giữa hai vi điều khiển khác nền tảng:

- **ESP32-C3** (chạy FreeRTOS) đảm nhận vai trò **Master**, thực hiện đo khoảng cách bằng cảm biến **SRF05** và hiển thị lên oled **OLED**.
- **STM32F103C8T6** (chạy FreeRTOS) là **Slave**, nhận dữ liệu khoảng cách qua **CAN Bus** và điều khiển **2 động cơ DC** qua **L298N**.

Cả hai vi điều khiển sử dụng **FreeRTOS** để xử lý đa luồng, sử dụng **Semaphore** để đồng bộ hóa các tác vụ quan trọng như đọc cảm biến, gửi/nhận CAN, và điều khiển động cơ.

---

## 🎯 Mục tiêu chính

- Giao tiếp CAN Bus ổn định giữa **ESP32-C3** và **STM32F103C8T6**.
- Đo khoảng cách bằng **SRF05** với độ chính xác cao.
- Hiển thị khoảng cách bằng **OLED** và rate **CANBUS** để dễ dàng kiểm tra.
- Điều khiển **2 động cơ DC** thông qua **L298N**, dựa vào dữ liệu khoảng cách nhận được.
- Quản lý đồng bộ các tiến trình bằng **FreeRTOS + Semaphore**.
- Phản hồi kịp thời khi khoảng cách < 20cm → dừng động cơ.

---

## 🧰 Phần cứng sử dụng

| Thiết bị | Mô tả |
|---------|-------|
| ESP32-C3 DevKit | Master đo khoảng cách và gửi dữ liệu |
| STM32F103C8T6 | Slave điều khiển động cơ |
| SRF05 hoặc HC-SR04 | Cảm biến siêu âm đo khoảng cách |
| OLED | Màn hình hiển thị khoảng cách và rate canbus |
| L298N | Mạch điều khiển 2 động cơ DC |
| Động cơ DC | Điều khiển bởi STM32 |
| SN65HVD230 / MCP2551 | Transceiver CAN Bus cho cả hai board |
| Breadboard, dây nối, trở pull-up | Linh kiện phụ trợ |

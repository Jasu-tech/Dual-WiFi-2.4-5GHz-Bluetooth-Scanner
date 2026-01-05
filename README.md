# WIFIBLE - Dual WiFi 2.4-5GHz & Bluetooth Scanner

WiFi 2.4 - 5 GHz + Bluetooth Scanner whit RTL8720DN BW16 - TOUCH VERSION!

Arduino_GFX + Hardware SPI (40MHz) + Native U8g2 Fonts + English UI + Touch 
--------------------------------------------------------------------------------------
![WiFi_BLE_Scanner_BW16_01](https://github.com/user-attachments/assets/2d5e1a6c-1596-4670-88f8-5ae5c2b7a880)
 ![WiFi_BLE_Scanner_BW16_02](https://github.com/user-attachments/assets/6bb29080-014e-47c0-a19d-84aad29400c5)
 --------------------------------------------------------------------------------------
 WIFIBLE is...
  - ULTRA FAST: 50-100x faster than Software SPI (display only)!
  - Hardware SPI @ 40MHz for Display
  - Software SPI for Touch (separate pins, no conflicts!)
  - Arduino_GFX_Library (optimation to AmebaD)
  - Native U8g2 font support
  - XPT2046 Touch Screen (resistive, manual Software SPI)

![WiFi_BLE_Scanner_BW16_04](https://github.com/user-attachments/assets/0d5b624b-4cc7-49b4-84a7-966727bdec98)

Information of WiFi's: SSID, MAC, Channell, RSSI, Encryption and Type.
--------------------------------------------------------------------------------------
Information of BLE's: Name, MAC, RSSI, Manufacturer and Services.
--------------------------------------------------------------------------------------
All you need is...

- 1pc RTL8720DN BW16
- 1pc ST7789 Touch Screen TFT Display
- Breadboar, some wires, tin, etc.
- Upload the code!
--------------------------------------------------------------------------------------
  Wiring: ST7789 Touch Screen TFT Display (HARDWARE SPI):
  
 * VCC  -> 3.3V
 *   GND  -> GND
 *   SCK  -> PA14 (pin 19) - Hardware SPI1
 *   MOSI -> PA12 (pin 20) - Hardware SPI1
 *   CS   -> PA27 (pin 2)  - Display Chip Select
 *   DC   -> PA25 (pin 7)  - Data/Command
 *   RST  -> PA26 (pin 8)  - Display Reset
 *   BLK  -> PA30 (pin 3)  - Backlight (lookup the J1 in back of you TFT for VCC 3.3V or 5V)

   Touch Screen (SOFTWARE SPI - separate pins!):
 *   T_VCC -> 3.3V
 *   T_GND -> GND
 *   T_CLK -> PB2  (pin 5)  - Software SPI Clock
 *   T_DIN -> PB3  (pin 6)  - Software SPI MOSI
 *   T_DO  -> PB1  (pin 4)  - Software SPI MISO
 *   T_CS  -> PA8  (pin 1)  - Touch Chip Select
 *   T_IRQ -> (NOT in use)
   
   Display rotation: 180° (pins at TOP!). Touch calibration: 90° rotated vs display.
 
REQUIRED LIBRARIES:
- GFX Library for Arduino (by moononournation)
- U8g2 (by oliver)
- WiFi (built-in AmebaD)
- BLEDevice (built-in AmebaD)
- SPI (built-in)
- NO need for XPT2046_Touchscreen library!

Copyright (c) 2026 Jasu-tech.
This code is licensed under a MIT License.

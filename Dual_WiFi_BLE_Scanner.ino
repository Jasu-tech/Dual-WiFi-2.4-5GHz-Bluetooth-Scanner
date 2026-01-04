/*
 * Dual WiFi 2.4 & 5GHz + Bluetooth Scanner whit RTL8720DN BW16 - SOFTWARE SPI TOUCH VERSION!
 * Arduino_GFX + Hardware SPI (40MHz) + Native U8g2 Fonts + English UI + XPT2046 Touch
 *
 * Copyright (c) 2026 Jasu-tech
 * This code is licensed under a MIT License.
 *
 * ULTRA FAST: 50-100x faster than Software SPI (display only)!
 * - Hardware SPI @ 40MHz for Display
 * - Software SPI for Touch (separate pins, no conflicts!)
 * - Arduino_GFX_Library (optimation to AmebaD)
 * - Native U8g2 font support
 * - XPT2046 Touch Screen (resistive, manual Software SPI)
 * * Wiring:
 * ST7789 Touch Screen TFT Display (HARDWARE SPI):
 * VCC  -> 3.3V
 * GND  -> GND
 * SCK  -> PA14 (pin 19) - Hardware SPI1
 * MOSI -> PA12 (pin 20) - Hardware SPI1
 * CS   -> PA27 (pin 2)  - Display Chip Select
 * DC   -> PA25 (pin 7)  - Data/Command
 * RST  -> PA26 (pin 8)  - Display Reset
 * BLK  -> PA30 (pin 3)  - Backlight (lookup the J1 in back of you TFT for VCC 3.3V or 5V)
 * XPT2046 Touch Screen (SOFTWARE SPI - separate pins!):
 * T_VCC -> 3.3V
 * T_GND -> GND
 * T_CLK -> PB2  (pin 5)  - Software SPI Clock
 * T_DIN -> PB3  (pin 6)  - Software SPI MOSI
 * T_DO  -> PB1  (pin 4)  - Software SPI MISO
 * T_CS  -> PA8  (pin 1)  - Touch Chip Select
 * T_IRQ -> (NOT in use)
 * * Display rotation: 180° (pins at TOP!)
 * Touch calibration: 90° rotated vs display
 *
 * REQUIRED LIBRARIES:
 * - GFX Library for Arduino (by moononournation)
 * - U8g2 (by oliver)
 * - WiFi (built-in AmebaD)
 * - BLEDevice (built-in AmebaD)
 * - SPI (built-in)
 * HOX: DON'T need XPT2046_Touchscreen library!
 */

#include <WiFi.h>
#include <BLEDevice.h>
#include <SPI.h>
#include <U8g2lib.h>
#include <Arduino_GFX_Library.h>

// RTL8720DN low-level WiFi SDK for channel information
#include <wifi_conf.h>
#include <wifi_structures.h>
#include <wifi_constants.h>

// Display pins - HARDWARE SPI1!
#define TFT_CS    2   // PA27 - Display Chip Select
#define TFT_DC    7   // PA25 - Data/Command
#define TFT_RST   8   // PA26 - Display Reset
#define TFT_BLK   3   // PA30 - Backlight

// XPT2046 Touch Screen pins - SOFTWARE SPI!
#define TOUCH_CLK  5  // PB2 - Software SPI Clock
#define TOUCH_DIN  6  // PB3 - Software SPI MOSI
#define TOUCH_DO   4  // PB1 - Software SPI MISO
#define TOUCH_CS   1  // PA8 - Touch Chip Select

// Display settings
#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 320
#define HEADER_HEIGHT 28
#define ROW_HEIGHT    38
#define MAX_VISIBLE_ROWS 7

// View modes
enum ViewMode {
  VIEW_MENU = 0,
  VIEW_WIFI_LIST = 1,
  VIEW_WIFI_DETAIL = 2,
  VIEW_BLE_LIST = 3,
  VIEW_BLE_DETAIL = 4
};

ViewMode currentView = VIEW_MENU;
int menuSelection = 0;

// WiFi band scanning flags
bool isScan24GHz = false;
bool isScan5GHz = false;

// Display colors (RGB565)
#define COLOR_BLACK     0x0000
#define COLOR_WHITE     0xFFFF
#define COLOR_RED       0xF800
#define COLOR_GREEN     0x07E0
#define COLOR_BLUE      0x001F
#define COLOR_CYAN      0x07FF
#define COLOR_MAGENTA   0xF81F
#define COLOR_YELLOW    0xFFE0
#define COLOR_DARKGRAY  0x2104
#define COLOR_SHADOW    0x18C3
#define COLOR_ORANGE    0x4208
#define COLOR_PURPLE    0x8010

// Bluetooth manufacturer names (Company ID -> Name mapping)
String getManufacturerName(uint16_t companyID) {
  switch(companyID) {
    case 0x004C: return "Apple";
    case 0x0006: return "Microsoft";
    case 0x0075: return "Samsung";
    case 0x00E0: return "Google";
    case 0x0087: return "Garmin";
    case 0x0157: return "Nokia";
    case 0x0059: return "Nordic Semi";
    case 0x000F: return "Broadcom";
    case 0x000D: return "Texas Instruments";
    case 0x0131: return "Pebble";
    case 0x0099: return "Huawei";
    case 0x02E5: return "Xiaomi";
    case 0x01FF: return "LG Electronics";
    case 0x0171: return "Fitbit";
    case 0x006B: return "Sony";
    case 0x004B: return "Cypress Semi";
    case 0x008C: return "Qualcomm";
    case 0x0057: return "HTC";
    case 0x01E0: return "Amazon";
    case 0x00D7: return "Polar";
    case 0x0022: return "NXP Semi";
    case 0x0046: return "Mediatek";
    case 0x0225: return "OnePlus";
    case 0x0272: return "Bose";
    default: return "";  // Unknown manufacturer
  }
}

// WiFi network structure with channel info and MAC address
struct NetworkInfo {
  String ssid;
  int32_t rssi;
  uint8_t encryptionType;
  uint8_t channel;     // Channel number (1-14 for 2.4GHz, 36+ for 5GHz)
  uint8_t bssid[6];    // MAC address (BSSID)
  uint8_t bssType;     // BSS type (0=Infrastructure, 1=Ad-hoc)
};

NetworkInfo networks[50];
int networkCount = 0;
bool wifiScanInProgress = false;
bool wifiScanComplete = false;

// WiFi scan callback handler
rtw_result_t wifiScanResultHandler(rtw_scan_handler_result_t *malloced_scan_result) {
  rtw_scan_result_t *record;
  
  if (malloced_scan_result->scan_complete != RTW_TRUE) {
    record = &malloced_scan_result->ap_details;
    record->SSID.val[record->SSID.len] = 0;  // Null terminate SSID
    
    if (networkCount < 50) {
      uint8_t channel = record->channel;
      bool is24GHz = (channel >= 1 && channel <= 14);
      bool is5GHz = (channel >= 36);
      
      // Filter by band if requested
      if (isScan24GHz && !is24GHz) return RTW_SUCCESS;
      if (isScan5GHz && !is5GHz) return RTW_SUCCESS;
      
      // Store network info
      networks[networkCount].ssid = String((char*)record->SSID.val);
      networks[networkCount].rssi = record->signal_strength;
      networks[networkCount].channel = channel;
      
      // Store MAC address (BSSID)
      memcpy(networks[networkCount].bssid, record->BSSID.octet, 6);
      
      // Store BSS type
      networks[networkCount].bssType = record->bss_type;
      
      // --- MUUTETTU OSA: Tunnistetaan salaus bittien perusteella ---
      uint32_t sec = record->security;
      
      if (sec == RTW_SECURITY_OPEN) {
        networks[networkCount].encryptionType = 0;
      } 
      // WPA3 bitti (0x00400000)
      else if (sec & 0x00400000) {
        networks[networkCount].encryptionType = 4;
      }
      // WPA2 bittimaski (0x00200000 on tyypillinen WPA2 AES bitti)
      else if (sec & 0x00200000) {
        networks[networkCount].encryptionType = 3;
      }
      // Fallback vanhoihin vakioihin jos bitit eivät osuneet
      else {
        switch (sec) {
          case RTW_SECURITY_WEP_PSK:
          case RTW_SECURITY_WEP_SHARED:
            networks[networkCount].encryptionType = 1;
            break;
          case RTW_SECURITY_WPA_TKIP_PSK:
          case RTW_SECURITY_WPA_AES_PSK:
            networks[networkCount].encryptionType = 2;
            break;
          case RTW_SECURITY_WPA2_AES_PSK:
          case RTW_SECURITY_WPA2_TKIP_PSK:
          case RTW_SECURITY_WPA2_MIXED_PSK:
            networks[networkCount].encryptionType = 3;
            break;
          default:
            networks[networkCount].encryptionType = 3; // Oletus WPA2
        }
      }
      // --- KORJAUS PÄÄTTYY ---
      
      networkCount++;
    }
  } else {
    // Scan complete
    wifiScanComplete = true;
    wifiScanInProgress = false;
  }
  
  return RTW_SUCCESS;
}

// BLE device structure with extended info
struct BLEDeviceInfo {
  String name;
  String address;
  int8_t rssi;
  bool hasName;
  uint16_t appearance;       // Device appearance/type
  uint16_t manufacturerID;   // Company ID
  String manufacturerData;   // Manufacturer specific data
  String serviceUUIDs;       // Service UUIDs (comma-separated)
  bool hasAppearance;
  bool hasManufacturerData;
  bool hasServices;
};

BLEDeviceInfo bleDevices[50];
int bleDeviceCount = 0;
bool bleScanning = false;

// Navigation
int selectedIndex = 0;
int scrollOffset = 0;

// Touch handling
unsigned long lastTouchTime = 0;
const unsigned long TOUCH_DEBOUNCE = 250;
int lastTouchX = -1, lastTouchY = -1;

// Touch calibration values for PORTRAIT mode (240x320 with 180° rotation)
#define TS_MINX 233    // Min touchX (left edge)
#define TS_MINY 247    // Min touchY (bottom edge)
#define TS_MAXX 1770   // Max touchX (right edge)
#define TS_MAXY 1814   // Max touchY (top edge)

// Hardware SPI setup for BW16
Arduino_DataBus *bus = new Arduino_HWSPI(TFT_DC, TFT_CS);
Arduino_GFX *tft = new Arduino_ST7789(bus, TFT_RST, 2 /* rotation 180° */, true /* IPS */,
                                      SCREEN_WIDTH, SCREEN_HEIGHT,
                                      0, 0, 0, 0);

// BLE Scan Callback for Ameba API
BLEAdvertData foundDevice;

void bleScanCallback(T_LE_CB_DATA* p_data) {
  if (bleDeviceCount >= 50) return;
  
  foundDevice.parseScanInfo(p_data);
  
  String address = foundDevice.getAddr().str();
  
  // Check if device already exists
  for (int i = 0; i < bleDeviceCount; i++) {
    if (bleDevices[i].address == address) {
      // Update RSSI
      bleDevices[i].rssi = foundDevice.getRSSI();
      return;
    }
  }
  
  // Add new device
  bleDevices[bleDeviceCount].address = address;
  bleDevices[bleDeviceCount].rssi = foundDevice.getRSSI();
  
  if (foundDevice.hasName()) {
    bleDevices[bleDeviceCount].name = foundDevice.getName();
    bleDevices[bleDeviceCount].hasName = true;
  } else {
    bleDevices[bleDeviceCount].name = "[Unknown]";
    bleDevices[bleDeviceCount].hasName = false;
  }
  
  // Get appearance (device type)
  bleDevices[bleDeviceCount].appearance = foundDevice.getAppearance();
  bleDevices[bleDeviceCount].hasAppearance = (bleDevices[bleDeviceCount].appearance != 0);
  
  // Get manufacturer data
  uint16_t mfgID = foundDevice.getManufacturer();
  uint8_t mfgLen = foundDevice.getManufacturerDataLength();
  if (mfgID != 0 || mfgLen > 0) {
    bleDevices[bleDeviceCount].manufacturerID = mfgID;
    char mfgStr[64];
    snprintf(mfgStr, sizeof(mfgStr), "ID:0x%04X Len:%d", mfgID, mfgLen);
    bleDevices[bleDeviceCount].manufacturerData = String(mfgStr);
    bleDevices[bleDeviceCount].hasManufacturerData = true;
  } else {
    bleDevices[bleDeviceCount].manufacturerID = 0;
    bleDevices[bleDeviceCount].hasManufacturerData = false;
  }
  
  // Get service UUIDs
  uint8_t serviceCount = foundDevice.getServiceCount();
  if (serviceCount > 0) {
    String uuids = "";
    BLEUUID* serviceList = foundDevice.getServiceList();
    for (uint8_t i = 0; i < serviceCount && i < 5; i++) {  // Max 5 UUIDs to save space
      if (i > 0) uuids += ", ";
      uuids += serviceList[i].str();
    }
    bleDevices[bleDeviceCount].serviceUUIDs = uuids;
    bleDevices[bleDeviceCount].hasServices = true;
  } else {
    bleDevices[bleDeviceCount].hasServices = false;
  }
  
  bleDeviceCount++;
}

// ========== XPT2046 SOFTWARE SPI FUNCTIONS ==========

void touchSPIBegin() {
  pinMode(TOUCH_CS, OUTPUT);
  pinMode(TOUCH_CLK, OUTPUT);
  pinMode(TOUCH_DIN, OUTPUT);
  pinMode(TOUCH_DO, INPUT);
  
  digitalWrite(TOUCH_CS, HIGH);
  digitalWrite(TOUCH_CLK, LOW);
}

uint16_t touchSPITransfer(uint8_t data) {
  uint16_t result = 0;
  
  for (int i = 7; i >= 0; i--) {
    digitalWrite(TOUCH_DIN, (data >> i) & 0x01);
    digitalWrite(TOUCH_CLK, HIGH);
    delayMicroseconds(1);
    digitalWrite(TOUCH_CLK, LOW);
    delayMicroseconds(1);
  }
  
  // Read 12-bit response
  for (int i = 11; i >= 0; i--) {
    digitalWrite(TOUCH_CLK, HIGH);
    delayMicroseconds(1);
    result |= (digitalRead(TOUCH_DO) << i);
    digitalWrite(TOUCH_CLK, LOW);
    delayMicroseconds(1);
  }
  
  return result;
}

uint16_t touchReadRaw(uint8_t command) {
  digitalWrite(TOUCH_CS, LOW);
  delayMicroseconds(10);
  
  uint16_t data = touchSPITransfer(command);
  
  digitalWrite(TOUCH_CS, HIGH);
  delayMicroseconds(10);
  
  return data;
}

bool touchPressed() {
  // Read Z1 pressure - LOW threshold for corner/edge detection
  uint16_t z1 = touchReadRaw(0xB1); // Z1 command
  return (z1 > 80);  // Low threshold to catch edge/corner presses
}

void touchGetRawXY(int& x, int& y) {
  x = touchReadRaw(0xD0); // X command
  y = touchReadRaw(0x90); // Y command
}

// Map touch coordinates to screen coordinates for PORTRAIT mode (240x320)
void mapTouchToScreen(int touchX, int touchY, int& screenX, int& screenY) {
  screenX = map(touchX, TS_MINX, TS_MAXX, 0, SCREEN_WIDTH - 1);   // touchX → screenX (NOT inverted)
  screenY = map(touchY, TS_MINY, TS_MAXY, SCREEN_HEIGHT - 1, 0);  // touchY → screenY (inverted)
  
  // Constrain to screen bounds
  screenX = constrain(screenX, 0, SCREEN_WIDTH - 1);
  screenY = constrain(screenY, 0, SCREEN_HEIGHT - 1);
}

// Handle touch input
void handleTouchInput() {
  if (!touchPressed()) return;
  
  unsigned long now = millis();
  if (now - lastTouchTime < TOUCH_DEBOUNCE) return;
  
  int touchX, touchY;
  touchGetRawXY(touchX, touchY);
  
  int screenX, screenY;
  mapTouchToScreen(touchX, touchY, screenX, screenY);
  
  // DEBUG: Print raw and mapped values
  Serial.print("RAW: touchX=");
  Serial.print(touchX);
  Serial.print(" touchY=");
  Serial.print(touchY);
  Serial.print(" -> SCREEN: X=");
  Serial.print(screenX);
  Serial.print(" Y=");
  Serial.println(screenY);
  
  lastTouchTime = now;
  lastTouchX = screenX;
  lastTouchY = screenY;
  
  // Check button areas - MUST MATCH DRAWN POSITIONS!
  bool isBackButton = (screenX < 80 && screenY < 50);    // Left-top corner - BACK button (Y: 0-50)
  bool isUpButton = (screenX >= 170 && screenY >= 0 && screenY < 40);      // Right side - UP button (Y: 0-40)
  bool isDownButton = (screenX >= 170 && screenY >= 280 && screenY <= 320); // Right side - DOWN button (Y: 280-320)
  
  // Back button - CHECK FIRST! (NOT in menu view)
  if (isBackButton && currentView != VIEW_MENU) {
    currentView = VIEW_MENU;
    menuSelection = 0;
    drawMenu();
    return;
  }
  
  // UP/DOWN buttons - ONLY work in list views
  if ((isUpButton || isDownButton) && (currentView == VIEW_WIFI_LIST || currentView == VIEW_BLE_LIST)) {
    if (isUpButton && selectedIndex > 0) {
      selectedIndex--;
      if (selectedIndex < scrollOffset) scrollOffset--;
      if (currentView == VIEW_WIFI_LIST) drawWiFiList();
      else drawBLEList();
    }
    else if (isDownButton) {
      if (currentView == VIEW_WIFI_LIST && selectedIndex < networkCount - 1) {
        selectedIndex++;
        if (selectedIndex >= scrollOffset + MAX_VISIBLE_ROWS) scrollOffset++;
        drawWiFiList();
      }
      if (currentView == VIEW_BLE_LIST && selectedIndex < bleDeviceCount - 1) {
        selectedIndex++;
        if (selectedIndex >= scrollOffset + MAX_VISIBLE_ROWS) scrollOffset++;
        drawBLEList();
      }
    }
    return;
  }
  
  // Menu navigation
  if (currentView == VIEW_MENU) {
    if (screenY >= 50 && screenY < 140) {
      isScan24GHz = true;
      isScan5GHz = false;
      startWiFiScan();
      return;
    }
    if (screenY >= 140 && screenY < 230) {
      isScan24GHz = false;
      isScan5GHz = true;
      startWiFiScan();
      return;
    }
    if (screenY >= 230 && screenY <= 320) {
      startBLEScan();
      return;
    }
  }
  
  // Detail views
  else if (currentView == VIEW_WIFI_DETAIL) {
    currentView = VIEW_WIFI_LIST;
    drawWiFiList();
  }
  else if (currentView == VIEW_BLE_DETAIL) {
    currentView = VIEW_BLE_LIST;
    drawBLEList();
  }
  else if (currentView == VIEW_WIFI_LIST && screenX < 200 && screenY >= 50 && screenY <= 280) {
    currentView = VIEW_WIFI_DETAIL;
    drawWiFiDetail();
  }
  else if (currentView == VIEW_BLE_LIST && screenX < 200 && screenY >= 50 && screenY <= 280) {
    currentView = VIEW_BLE_DETAIL;
    drawBLEDetail();
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Dual WiFi+BLE Scanner - SOFTWARE SPI TOUCH VERSION!");
  
  tft->begin(40000000);
  tft->invertDisplay(true);
  tft->fillScreen(COLOR_BLACK);
  tft->setFont(u8g2_font_helvR14_tr);
  tft->setTextColor(COLOR_CYAN);
  tft->setCursor(30, 160);
  tft->print("Starting...");
  delay(500);
  
  touchSPIBegin();
  
  Serial.println("Initializing WiFi...");
  WiFi.status();
  delay(1000);
  
  drawMenu();
}

void loop() {
  handleTouchInput();
  delay(10);
}

void drawMenu() {
  tft->fillScreen(COLOR_BLACK);
  
  tft->fillRect(0, 0, SCREEN_WIDTH, 50, COLOR_BLUE);
  tft->setFont(u8g2_font_helvR14_tr);
  tft->setTextColor(COLOR_WHITE);
  tft->setCursor(35, 20);
  tft->print("WiFi + BLE");
  
  tft->setFont(u8g2_font_helvR08_tr);
  tft->setTextColor(COLOR_CYAN);
  tft->setCursor(15, 35);
  tft->print("2.4GHz | 5GHz | BLE");
  
  tft->fillRect(0, 50, SCREEN_WIDTH, 90, COLOR_DARKGRAY);
  tft->drawRect(0, 50, SCREEN_WIDTH, 90, COLOR_CYAN);
  tft->setFont(u8g2_font_helvR14_tr);
  tft->setTextColor(COLOR_CYAN);
  tft->setCursor(45, 75);
  tft->print("2.4 GHz");
  
  tft->setFont(u8g2_font_helvR08_tr);
  tft->setTextColor(COLOR_WHITE);
  tft->setCursor(30, 95);
  tft->print("WiFi networks");
  tft->setCursor(20, 110);
  tft->print("Channells 1-13");
  tft->setCursor(15, 125);
  tft->print("Better penetration");
  
  tft->fillRect(0, 140, SCREEN_WIDTH, 90, COLOR_DARKGRAY);
  tft->drawRect(0, 140, SCREEN_WIDTH, 90, COLOR_CYAN);
  tft->setFont(u8g2_font_helvR14_tr);
  tft->setTextColor(COLOR_CYAN);
  tft->setCursor(50, 165);
  tft->print("5 GHz");
  
  tft->setFont(u8g2_font_helvR08_tr);
  tft->setTextColor(COLOR_WHITE);
  tft->setCursor(30, 185);
  tft->print("WiFi networks");
  tft->setCursor(25, 200);
  tft->print("Channells 36+");
  tft->setCursor(20, 215);
  tft->print("Better speed");
  
  tft->fillRect(0, 230, SCREEN_WIDTH, 90, COLOR_DARKGRAY);
  tft->drawRect(0, 230, SCREEN_WIDTH, 90, COLOR_MAGENTA);
  tft->setFont(u8g2_font_helvR14_tr);
  tft->setTextColor(COLOR_MAGENTA);
  tft->setCursor(50, 255);
  tft->print("Bluetooth");
  
  tft->setFont(u8g2_font_helvR08_tr);
  tft->setTextColor(COLOR_WHITE);
  tft->setCursor(35, 275);
  tft->print("BLE Units");
  tft->setCursor(30, 290);
  tft->print("Touch to search");
}

void drawWiFiList() {
  tft->fillScreen(COLOR_BLACK);
  
  tft->fillRect(0, 0, SCREEN_WIDTH, 40, COLOR_BLUE);
  tft->setFont(u8g2_font_helvR14_tr);
  tft->setTextColor(COLOR_WHITE);
  tft->setCursor(25, 20);
  tft->print("* WiFi Networks");
  
  tft->setFont(u8g2_font_helvR08_tr);
  tft->setTextColor(COLOR_CYAN);
  tft->setCursor(20, 32);
  tft->print("Networks: ");
  tft->print(networkCount);
  
  tft->fillRect(170, 0, 70, 40, COLOR_BLUE);
  tft->drawRect(170, 0, 70, 40, COLOR_CYAN);
  tft->setFont(u8g2_font_helvR14_tr);
  tft->setTextColor(COLOR_YELLOW);
  tft->setCursor(198, 22);
  tft->print("^");
  
  int y = 50;
  for (int i = scrollOffset; i < networkCount && i < scrollOffset + MAX_VISIBLE_ROWS; i++) {
    if (i == selectedIndex) {
      tft->fillRect(0, y - 2, 195, ROW_HEIGHT, COLOR_DARKGRAY);
      tft->drawRect(0, y - 2, 195, ROW_HEIGHT, COLOR_CYAN);
      tft->setTextColor(COLOR_YELLOW);
    } else {
      tft->setTextColor(COLOR_WHITE);
    }
    
    tft->setFont(u8g2_font_helvR10_tr);
    tft->setCursor(10, y + 12);
    String ssid = networks[i].ssid;
    if (ssid.length() > 18) ssid = ssid.substring(0, 15) + "...";
    tft->print(ssid);
    
    tft->setFont(u8g2_font_helvR08_tr);
    tft->setCursor(10, y + 23);
    char mac[18];
    sprintf(mac, "%02X:%02X:%02X:%02X:%02X:%02X", 
            networks[i].bssid[0], networks[i].bssid[1], networks[i].bssid[2],
            networks[i].bssid[3], networks[i].bssid[4], networks[i].bssid[5]);
    tft->print(mac);
    
    tft->setCursor(10, y + 34);
    tft->print("Ch:");
    tft->print(networks[i].channel);
    tft->print(" | ");
    tft->print(networks[i].rssi);
    tft->print("dBm");
    
    y += ROW_HEIGHT;
  }
  
  tft->fillRect(170, 280, 70, 40, COLOR_BLUE);
  tft->drawRect(170, 280, 70, 40, COLOR_CYAN);
  tft->setFont(u8g2_font_helvR14_tr);
  tft->setTextColor(COLOR_YELLOW);
  tft->setCursor(198, 295);
  tft->print("v");
}

void drawWiFiDetail() {
  if (selectedIndex >= networkCount) return;
  
  tft->fillScreen(COLOR_BLACK);
  
  tft->fillRect(0, 0, SCREEN_WIDTH, 40, COLOR_BLUE);
  tft->setFont(u8g2_font_helvR12_tr);
  tft->setTextColor(COLOR_CYAN);
  tft->setCursor(20, 20);
  tft->print("Information");
  
  int y = 60;
  tft->setFont(u8g2_font_helvR10_tr);
  tft->setTextColor(COLOR_WHITE);
  
  tft->setCursor(20, y);
  tft->print("SSID:");
  tft->setCursor(20, y + 18);
  tft->setTextColor(COLOR_YELLOW);
  tft->print(networks[selectedIndex].ssid);
  
  y += 40;
  tft->setTextColor(COLOR_WHITE);
  tft->setCursor(20, y);
  tft->print("MAC:");
  tft->setCursor(20, y + 18);
  tft->setTextColor(COLOR_CYAN);
  char mac[18];
  sprintf(mac, "%02X:%02X:%02X:%02X:%02X:%02X", 
          networks[selectedIndex].bssid[0], networks[selectedIndex].bssid[1], 
          networks[selectedIndex].bssid[2], networks[selectedIndex].bssid[3], 
          networks[selectedIndex].bssid[4], networks[selectedIndex].bssid[5]);
  tft->print(mac);
  
  y += 40;
  tft->setTextColor(COLOR_WHITE);
  tft->setCursor(20, y);
  tft->print("Channell: ");
  tft->setTextColor(COLOR_YELLOW);
  tft->print(networks[selectedIndex].channel);
  
  tft->setTextColor(COLOR_WHITE);
  tft->print(" (");
  if (networks[selectedIndex].channel >= 1 && networks[selectedIndex].channel <= 14) {
    tft->print("2.4GHz");
  } else if (networks[selectedIndex].channel >= 36) {
    tft->print("5GHz");
  }
  tft->print(")");
  
  y += 30;
  tft->setTextColor(COLOR_WHITE);
  tft->setCursor(20, y);
  tft->print("RSSI: ");
  tft->setTextColor(COLOR_CYAN);
  tft->print(networks[selectedIndex].rssi);
  tft->print(" dBm");
  
  y += 30;
  tft->setTextColor(COLOR_WHITE);
  tft->setCursor(20, y);
  tft->print("Encryption: ");
  tft->setTextColor(COLOR_GREEN);
  
  switch (networks[selectedIndex].encryptionType) {
    case 0: tft->print("Avoin"); break;
    case 1: tft->print("WEP"); break;
    case 2: tft->print("WPA"); break;
    case 3: tft->print("WPA2"); break;
    case 4: tft->print("WPA3-SAE"); break; 
    default: tft->print("Unknown");
  }
  
  y += 30;
  tft->setTextColor(COLOR_WHITE);
  tft->setCursor(20, y);
  tft->print("Type: ");
  tft->setTextColor(COLOR_YELLOW);
  if (networks[selectedIndex].bssType == 0) {
    tft->print("Infrastructure");
  } else if (networks[selectedIndex].bssType == 1) {
    tft->print("Ad-hoc");
  } else {
    tft->print("Unknown");
  }
}

void startWiFiScan() {
  tft->fillScreen(COLOR_BLACK);
  tft->fillRect(0, 0, SCREEN_WIDTH, 40, COLOR_BLUE);
  tft->setFont(u8g2_font_helvR14_tr);
  tft->setTextColor(COLOR_WHITE);
  
  if (isScan24GHz) {
    tft->setCursor(30, 20);
    tft->print("WiFi 2.4GHz");
  } else if (isScan5GHz) {
    tft->setCursor(35, 20);
    tft->print("WiFi 5GHz");
  } else {
    tft->setCursor(45, 20);
    tft->print("WiFi");
  }
  
  tft->setFont(u8g2_font_helvR10_tr);
  tft->setTextColor(COLOR_CYAN);
  tft->setCursor(50, 100);
  tft->print("Scanning...");
  
  networkCount = 0;
  scrollOffset = 0;
  wifiScanComplete = false;
  wifiScanInProgress = true;
  
  if (wifi_scan_networks(wifiScanResultHandler, NULL) == RTW_SUCCESS) {
    uint8_t attempts = 20;
    while (!wifiScanComplete && attempts > 0) {
      delay(250);
      attempts--;
    }
  }
  
  isScan24GHz = false;
  isScan5GHz = false;
  wifiScanInProgress = false;
  
  delay(500);
  currentView = VIEW_WIFI_LIST;
  selectedIndex = 0;
  drawWiFiList();
}

void startBLEScan() {
  tft->fillScreen(COLOR_BLACK);
  tft->fillRect(0, 0, SCREEN_WIDTH, 40, COLOR_MAGENTA);
  tft->setFont(u8g2_font_helvR14_tr);
  tft->setTextColor(COLOR_WHITE);
  tft->setCursor(70, 20);
  tft->print("Bluetooth");
  
  tft->setFont(u8g2_font_helvR10_tr);
  tft->setTextColor(COLOR_MAGENTA);
  tft->setCursor(70, 100);
  tft->print("Scanning...");
  
  bleDeviceCount = 0;
  scrollOffset = 0;
  bleScanning = true;
  
  BLE.init();
  BLE.configScan()->setScanMode(GAP_SCAN_MODE_ACTIVE);
  BLE.configScan()->setScanInterval(500);
  BLE.configScan()->setScanWindow(250);
  BLE.configScan()->updateScanParams();
  
  BLE.setScanCallback(bleScanCallback);
  BLE.beginCentral(0);
  BLE.configScan()->startScan(10000);
  
  delay(2000);
  
  BLE.end();
  
  bleScanning = false;
  
  delay(500);
  currentView = VIEW_BLE_LIST;
  selectedIndex = 0;
  drawBLEList();
}

void drawBLEList() {
  tft->fillScreen(COLOR_BLACK);
  tft->fillRect(0, 0, SCREEN_WIDTH, 40, COLOR_MAGENTA);
  tft->setFont(u8g2_font_helvR14_tr);
  tft->setTextColor(COLOR_WHITE);
  tft->setCursor(25, 20);
  tft->print("* BLE Units");
  
  tft->setFont(u8g2_font_helvR08_tr);
  tft->setTextColor(COLOR_CYAN);
  tft->setCursor(20, 32);
  tft->print("Units: ");
  tft->print(bleDeviceCount);
  
  tft->fillRect(170, 0, 70, 40, COLOR_MAGENTA);
  tft->drawRect(170, 0, 70, 40, COLOR_CYAN);
  tft->setFont(u8g2_font_helvR14_tr);
  tft->setTextColor(COLOR_YELLOW);
  tft->setCursor(198, 22);
  tft->print("^");
  
  int y = 50;
  for (int i = scrollOffset; i < bleDeviceCount && i < scrollOffset + MAX_VISIBLE_ROWS; i++) {
    if (i == selectedIndex) {
      tft->fillRect(0, y - 2, 195, ROW_HEIGHT, COLOR_DARKGRAY);
      tft->drawRect(0, y - 2, 195, ROW_HEIGHT, COLOR_MAGENTA);
      tft->setTextColor(COLOR_MAGENTA);
    } else {
      tft->setTextColor(COLOR_WHITE);
    }
    
    tft->setFont(u8g2_font_helvR10_tr);
    tft->setCursor(10, y + 15);
    String name = bleDevices[i].name;
    if (name.length() > 18) name = name.substring(0, 15) + "...";
    tft->print(name);
    
    tft->setFont(u8g2_font_helvR08_tr);
    tft->setCursor(10, y + 28);
    tft->print("RSSI: ");
    tft->print(bleDevices[i].rssi);
    y += ROW_HEIGHT;
  }
  
  tft->fillRect(170, 280, 70, 40, COLOR_MAGENTA);
  tft->drawRect(170, 280, 70, 40, COLOR_CYAN);
  tft->setFont(u8g2_font_helvR14_tr);
  tft->setTextColor(COLOR_YELLOW);
  tft->setCursor(198, 295);
  tft->print("v");
}

void drawBLEDetail() {
  if (selectedIndex >= bleDeviceCount) return;
  tft->fillScreen(COLOR_BLACK);
  tft->fillRect(0, 0, SCREEN_WIDTH, 40, COLOR_MAGENTA);
  tft->setFont(u8g2_font_helvR12_tr);
  tft->setTextColor(COLOR_CYAN);
  tft->setCursor(20, 20);
  tft->print("Information");
  int y = 60;
  tft->setFont(u8g2_font_helvR10_tr);
  tft->setTextColor(COLOR_WHITE);
  tft->setCursor(20, y);
  tft->print("Name:");
  tft->setCursor(20, y + 18);
  tft->setTextColor(COLOR_YELLOW);
  tft->print(bleDevices[selectedIndex].name);
  y += 50;
  tft->setTextColor(COLOR_WHITE);
  tft->setCursor(20, y);
  tft->print("MAC:");
  tft->setCursor(20, y + 18);
  tft->setTextColor(COLOR_CYAN);
  tft->print(bleDevices[selectedIndex].address);
  y += 40;
  tft->setTextColor(COLOR_WHITE);
  tft->setCursor(20, y);
  tft->print("RSSI: ");
  tft->setTextColor(COLOR_GREEN);
  tft->print(bleDevices[selectedIndex].rssi);
  tft->print(" dBm");
  
  if (bleDevices[selectedIndex].hasAppearance) {
    y += 30;
    tft->setTextColor(COLOR_WHITE);
    tft->setCursor(20, y);
    tft->print("Type: ");
    tft->setTextColor(COLOR_YELLOW);
    tft->print("0x");
    tft->print(bleDevices[selectedIndex].appearance, HEX);
  }
  
  if (bleDevices[selectedIndex].hasManufacturerData) {
    y += 30;
    tft->setTextColor(COLOR_WHITE);
    tft->setCursor(20, y);
    tft->print("Manufacturer:");
    String mfgName = getManufacturerName(bleDevices[selectedIndex].manufacturerID);
    if (mfgName.length() > 0) {
      tft->setCursor(20, y + 15);
      tft->setTextColor(COLOR_YELLOW);
      tft->print(mfgName);
      tft->setCursor(20, y + 30);
      tft->setTextColor(COLOR_CYAN);
      tft->setFont(u8g2_font_helvR08_tr);
      tft->print(bleDevices[selectedIndex].manufacturerData);
      tft->setFont(u8g2_font_helvR10_tr);
      y += 30;
    } else {
      tft->setCursor(20, y + 15);
      tft->setTextColor(COLOR_CYAN);
      tft->setFont(u8g2_font_helvR08_tr);
      tft->print(bleDevices[selectedIndex].manufacturerData);
      tft->setFont(u8g2_font_helvR10_tr);
      y += 15;
    }
  }
  
  if (bleDevices[selectedIndex].hasServices) {
    y += 30;
    tft->setTextColor(COLOR_WHITE);
    tft->setCursor(20, y);
    tft->print("Services:");
    tft->setCursor(20, y + 15);
    tft->setTextColor(COLOR_MAGENTA);
    tft->setFont(u8g2_font_helvR08_tr);
    String uuids = bleDevices[selectedIndex].serviceUUIDs;
    if (uuids.length() > 25) {
      tft->print(uuids.substring(0, 25));
      tft->setCursor(20, y + 27);
      tft->print(uuids.substring(25));
    } else {
      tft->print(uuids);
    }
    tft->setFont(u8g2_font_helvR10_tr);
  }
}

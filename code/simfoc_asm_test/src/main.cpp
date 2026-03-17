#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>

// ============================================================================
//  AS5600 Magnetic Encoder Test
//  STM32F103C8 + AS5600 (I2C1) + SSD1306 OLED (Software I2C)
// ============================================================================

// ----- AS5600 Definitions -----
#define AS5600_ADDR        0x36  // 7-bit I2C address

// Register addresses
#define REG_RAW_ANGLE_H    0x0C
#define REG_RAW_ANGLE_L    0x0D
#define REG_ANGLE_H        0x0E
#define REG_ANGLE_L        0x0F
#define REG_STATUS         0x0B
#define REG_AGC            0x1A
#define REG_MAGNITUDE_H    0x1B
#define REG_MAGNITUDE_L    0x1C

// Status register bits
#define STATUS_MH          0x08  // Magnet too strong
#define STATUS_ML          0x10  // Magnet too weak
#define STATUS_MD          0x20  // Magnet detected

// ----- OLED (Software I2C on PB3=SDA, PB4=SCL) -----
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, /* SCL=*/ PB4, /* SDA=*/ PB3, /* reset=*/ U8X8_PIN_NONE);

// ----- Update interval -----
#define UPDATE_INTERVAL_MS 100
static uint32_t last_update = 0;

// ============================================================================
//  AS5600 I2C Read Functions
// ============================================================================

// Read a single byte from AS5600 register
static uint8_t as5600_read_byte(uint8_t reg) {
    Wire.beginTransmission(AS5600_ADDR);
    Wire.write(reg);
    Wire.endTransmission(false);
    Wire.requestFrom((uint8_t)AS5600_ADDR, (uint8_t)1);
    return Wire.available() ? Wire.read() : 0;
}

// Read a 12-bit value from two consecutive registers (high byte first)
static uint16_t as5600_read_12bit(uint8_t reg_h) {
    Wire.beginTransmission(AS5600_ADDR);
    Wire.write(reg_h);
    Wire.endTransmission(false);
    Wire.requestFrom((uint8_t)AS5600_ADDR, (uint8_t)2);
    if (Wire.available() < 2) return 0;
    uint8_t h = Wire.read();
    uint8_t l = Wire.read();
    return ((uint16_t)(h & 0x0F) << 8) | l;
}

// ============================================================================
//  Setup
// ============================================================================

void setup() {
    // Serial on USART3 (PB10/PB11)
    Serial.begin(115200);
    delay(200);
    Serial.println(F("=================================="));
    Serial.println(F("  AS5600 Encoder Test"));
    Serial.println(F("  I2C1: PB6(SCL) / PB7(SDA)"));
    Serial.println(F("  OLED:  PB4(SCL) / PB3(SDA)"));
    Serial.println(F("=================================="));

    // Hardware I2C1 for AS5600 (PB6=SCL, PB7=SDA by default)
    Wire.begin();

    // Verify AS5600 is present
    Wire.beginTransmission(AS5600_ADDR);
    uint8_t err = Wire.endTransmission();
    if (err == 0) {
        Serial.println(F("AS5600 detected on I2C1."));
    } else {
        Serial.print(F("AS5600 NOT found! I2C error: "));
        Serial.println(err);
    }

    // Initialize OLED (software I2C)
    u8g2.begin();
    u8g2.setFont(u8g2_font_6x13_tf);

    // Show splash screen
    u8g2.clearBuffer();
    u8g2.drawStr(16, 28, "AS5600 Test");
    u8g2.drawStr(20, 48, "Starting...");
    u8g2.sendBuffer();
    delay(500);

    last_update = millis();
    Serial.println(F("Setup complete. Reading..."));
    Serial.println(F("RAW\tANGLE\tDEG\tSTATUS\tAGC\tMAG"));
}

// ============================================================================
//  Main Loop
// ============================================================================

void loop() {
    uint32_t now = millis();
    if (now - last_update < UPDATE_INTERVAL_MS) return;
    last_update = now;

    // ----- Read all AS5600 data -----
    uint16_t raw_angle = as5600_read_12bit(REG_RAW_ANGLE_H);
    uint16_t angle     = as5600_read_12bit(REG_ANGLE_H);
    uint8_t  status    = as5600_read_byte(REG_STATUS);
    uint8_t  agc       = as5600_read_byte(REG_AGC);
    uint16_t magnitude = as5600_read_12bit(REG_MAGNITUDE_H);

    // ----- Convert to degrees -----
    float raw_deg   = (float)raw_angle * 360.0f / 4096.0f;
    float angle_deg = (float)angle * 360.0f / 4096.0f;

    // ----- Decode magnet status -----
    const char *mag_status;
    if (!(status & STATUS_MD)) {
        mag_status = "NO MAGNET";
    } else if (status & STATUS_MH) {
        mag_status = "TOO STRONG";
    } else if (status & STATUS_ML) {
        mag_status = "TOO WEAK";
    } else {
        mag_status = "OK";
    }

    // ----- Serial output -----
    Serial.print(raw_angle);  Serial.print('\t');
    Serial.print(angle);      Serial.print('\t');
    Serial.print(raw_deg, 1); Serial.print('\t');
    Serial.print(mag_status); Serial.print('\t');
    Serial.print(agc);        Serial.print('\t');
    Serial.println(magnitude);

    // ----- OLED display -----
    char buf[22];

    u8g2.clearBuffer();

    // Line 1: Title
    u8g2.setFont(u8g2_font_6x13_tf);
    u8g2.drawStr(0, 12, "AS5600 Encoder Test");

    // Line 2: Raw angle value
    snprintf(buf, sizeof(buf), "RAW: %4u / 4095", raw_angle);
    u8g2.drawStr(0, 26, buf);

    // Line 3: Angle in degrees (filtered)
    // Use dtostrf for float formatting on AVR/STM32
    char deg_str[8];
    dtostrf(angle_deg, 6, 1, deg_str);
    snprintf(buf, sizeof(buf), "DEG:%s", deg_str);
    u8g2.drawStr(0, 40, buf);

    // Line 4: Magnet status + AGC
    snprintf(buf, sizeof(buf), "MAG:%s AGC:%u", mag_status, agc);
    u8g2.drawStr(0, 54, buf);

    u8g2.sendBuffer();
}

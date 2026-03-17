#include <Arduino.h>
#include <U8g2lib.h>

// ============================================================================
//  STM32F103C8 Basic Upload Test - LED Blink + OLED
//  Verify: JLink upload, 8MHz HSE crystal, GPIO, OLED
//  LED on PC13 (HIGH = ON), blink 1s interval
//  OLED on PB3(SDA) / PB4(SCL), software I2C
//
//  Clock check:
//    72 MHz = HSE 8MHz working (8 * 9 PLL)
//    64 MHz = HSI internal RC  (8 / 2 * 16 PLL)
// ============================================================================

#define PIN_LED PC13

// OLED SSD1306 128x64, software I2C
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, /* SCL=*/ PB4, /* SDA=*/ PB3, /* reset=*/ U8X8_PIN_NONE);

static uint32_t blink_count = 0;
static bool led_state = false;

void setup() {
    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, LOW);

    // Initialize OLED
    u8g2.begin();
    u8g2.setFont(u8g2_font_6x13_tf);

    // Splash screen
    u8g2.clearBuffer();
    u8g2.drawStr(20, 28, "LED Blink Test");
    u8g2.drawStr(24, 48, "Starting...");
    u8g2.sendBuffer();
    delay(500);
}

void loop() {
    // Toggle LED
    led_state = !led_state;
    digitalWrite(PIN_LED, led_state ? HIGH : LOW);
    blink_count++;

    // Update OLED
    char buf[22];
    u8g2.clearBuffer();

    // Line 1: Title
    u8g2.drawStr(12, 12, "Basic Blink Test");

    // Line 2: LED state
    u8g2.drawStr(0, 28, led_state ? "LED: ON " : "LED: OFF");

    // Line 3: Blink count
    snprintf(buf, sizeof(buf), "Count: %lu", blink_count);
    u8g2.drawStr(0, 42, buf);

    // Line 4: System clock (72=HSE ok, 64=HSI)
    snprintf(buf, sizeof(buf), "CLK: %lu MHz", SystemCoreClock / 1000000);
    u8g2.drawStr(0, 56, buf);

    u8g2.sendBuffer();

    delay(1000);
}

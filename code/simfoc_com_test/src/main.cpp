#include <Arduino.h>
#include <U8g2lib.h>

// ============================================================================
//  UART + CAN Communication Test
//  Two STM32F103C8 boards, Master/Slave selected at compile time
//  Press KEY1 to send a random message via both UART and CAN
//  Received messages displayed on OLED
// ============================================================================

// ----- Role (0=Master, 1=Slave) -----
#ifndef NODE_ROLE
#define NODE_ROLE 0
#endif

#if NODE_ROLE == 0
#define ROLE_NAME "MASTER"
#define MY_CAN_TX_ID  0x10
#define MY_CAN_RX_ID  0x20
#else
#define ROLE_NAME "SLAVE"
#define MY_CAN_TX_ID  0x20
#define MY_CAN_RX_ID  0x10
#endif

// ----- Pins -----
#define PIN_LED  PC13
#define PIN_KEY1 PA15

// ----- OLED (Software I2C: PB3=SDA, PB4=SCL) -----
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, /* SCL=*/ PB4, /* SDA=*/ PB3, /* reset=*/ U8X8_PIN_NONE);

// ----- Predefined messages (max 7 chars to fit CAN 8-byte payload) -----
static const char *msg_pool[] = {
    "Hello!",
    "FOC OK",
    "Test12",
    "CAN OK",
    "Ping",
    "STM32",
    "SimFOC",
    "Motor!",
};
#define MSG_POOL_SIZE (sizeof(msg_pool) / sizeof(msg_pool[0]))

// ----- State -----
static char last_tx[10]       = "-";
static char last_uart_rx[10]  = "-";
static char last_can_rx[10]   = "-";
static uint32_t tx_count      = 0;
static uint32_t uart_rx_count = 0;
static uint32_t can_rx_count  = 0;

// UART receive buffer
static char uart_buf[16];
static uint8_t uart_buf_idx = 0;

// Key debounce
static uint32_t last_key_time = 0;
static bool last_key_state    = HIGH;

// OLED refresh
static uint32_t last_oled_time = 0;
static bool oled_dirty = true;

// ============================================================================
//  CAN (STM32 HAL)
// ============================================================================

static CAN_HandleTypeDef hcan;
static volatile bool can_rx_flag = false;
static volatile uint8_t can_rx_data[8];
static volatile uint8_t can_rx_len = 0;

bool can_init(void) {
    // GPIO: PA11=CAN_RX, PA12=CAN_TX
    __HAL_RCC_CAN1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};

    // CAN TX - PA12
    gpio.Pin   = GPIO_PIN_12;
    gpio.Mode  = GPIO_MODE_AF_PP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &gpio);

    // CAN RX - PA11
    gpio.Pin  = GPIO_PIN_11;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &gpio);

    // CAN: 500kbps, APB1=36MHz, 36/(4*(1+13+4))=500k
    hcan.Instance = CAN1;
    hcan.Init.Prescaler          = 4;
    hcan.Init.Mode               = CAN_MODE_NORMAL;
    hcan.Init.SyncJumpWidth      = CAN_SJW_1TQ;
    hcan.Init.TimeSeg1           = CAN_BS1_13TQ;
    hcan.Init.TimeSeg2           = CAN_BS2_4TQ;
    hcan.Init.TimeTriggeredMode  = DISABLE;
    hcan.Init.AutoBusOff         = ENABLE;
    hcan.Init.AutoWakeUp         = ENABLE;
    hcan.Init.AutoRetransmission = ENABLE;
    hcan.Init.ReceiveFifoLocked  = DISABLE;
    hcan.Init.TransmitFifoPriority = DISABLE;

    if (HAL_CAN_Init(&hcan) != HAL_OK) return false;

    // Filter: accept peer's TX ID only (16-bit list mode)
    CAN_FilterTypeDef filter = {0};
    filter.FilterBank           = 0;
    filter.FilterMode           = CAN_FILTERMODE_IDLIST;
    filter.FilterScale          = CAN_FILTERSCALE_16BIT;
    filter.FilterFIFOAssignment = CAN_FILTER_FIFO0;
    filter.FilterActivation     = ENABLE;

    uint16_t peer_id = (uint16_t)(MY_CAN_RX_ID << 5);
    filter.FilterIdHigh     = peer_id;
    filter.FilterIdLow      = peer_id;
    filter.FilterMaskIdHigh = peer_id;
    filter.FilterMaskIdLow  = peer_id;

    if (HAL_CAN_ConfigFilter(&hcan, &filter) != HAL_OK) return false;

    if (HAL_CAN_Start(&hcan) != HAL_OK) return false;

    // Enable FIFO0 RX interrupt
    HAL_CAN_ActivateNotification(&hcan, CAN_IT_RX_FIFO0_MSG_PENDING);
    HAL_NVIC_SetPriority(USB_LP_CAN1_RX0_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(USB_LP_CAN1_RX0_IRQn);

    return true;
}

// CAN ISR
extern "C" void USB_LP_CAN1_RX0_IRQHandler(void) {
    HAL_CAN_IRQHandler(&hcan);
}

extern "C" void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *h) {
    CAN_RxHeaderTypeDef header;
    uint8_t data[8];
    if (HAL_CAN_GetRxMessage(h, CAN_RX_FIFO0, &header, data) != HAL_OK) return;

    for (uint8_t i = 0; i < header.DLC && i < 8; i++) {
        can_rx_data[i] = data[i];
    }
    can_rx_len = header.DLC;
    can_rx_flag = true;
}

bool can_send(const char *msg) {
    CAN_TxHeaderTypeDef header = {0};
    header.StdId = MY_CAN_TX_ID;
    header.IDE   = CAN_ID_STD;
    header.RTR   = CAN_RTR_DATA;

    uint8_t data[8] = {0};
    uint8_t len = 0;
    while (msg[len] && len < 8) {
        data[len] = msg[len];
        len++;
    }
    header.DLC = len;

    uint32_t mailbox;
    if (HAL_CAN_AddTxMessage(&hcan, &header, data, &mailbox) != HAL_OK) {
        return false;
    }

    // Wait for TX complete (timeout 5ms)
    uint32_t start = millis();
    while (HAL_CAN_IsTxMessagePending(&hcan, mailbox)) {
        if (millis() - start > 5) return false;
    }
    return true;
}

// ============================================================================
//  OLED Display
// ============================================================================

void oled_update(void) {
    char buf[22];
    u8g2.clearBuffer();

    // Line 1: Role + counts
    snprintf(buf, sizeof(buf), "%s TX:%lu", ROLE_NAME, tx_count);
    u8g2.drawStr(0, 12, buf);

    // Line 2: Last sent message
    snprintf(buf, sizeof(buf), "TX> %s", last_tx);
    u8g2.drawStr(0, 26, buf);

    // Line 3: Last UART received
    snprintf(buf, sizeof(buf), "UART< %s [%lu]", last_uart_rx, uart_rx_count);
    u8g2.drawStr(0, 42, buf);

    // Line 4: Last CAN received
    snprintf(buf, sizeof(buf), "CAN<  %s [%lu]", last_can_rx, can_rx_count);
    u8g2.drawStr(0, 56, buf);

    u8g2.sendBuffer();
}

// ============================================================================
//  Send a random message via both UART and CAN
// ============================================================================

void send_random_message(void) {
    uint8_t idx = random(MSG_POOL_SIZE);
    const char *msg = msg_pool[idx];

    // Save for display
    strncpy(last_tx, msg, sizeof(last_tx) - 1);
    last_tx[sizeof(last_tx) - 1] = '\0';
    tx_count++;

    // Send via UART (Serial = USART3)
    Serial.println(msg);

    // Send via CAN
    can_send(msg);

    // Blink LED
    digitalWrite(PIN_LED, HIGH);
    delay(50);
    digitalWrite(PIN_LED, LOW);

    oled_dirty = true;
}

// ============================================================================
//  Setup
// ============================================================================

void setup() {
    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, LOW);
    pinMode(PIN_KEY1, INPUT_PULLUP);

    // UART on USART3 (PB10=TX, PB11=RX), cross-connected to peer
    Serial.begin(115200);

    // Seed random with floating ADC
    randomSeed(analogRead(PA0));

    // Init OLED
    u8g2.begin();
    u8g2.setFont(u8g2_font_6x13_tf);
    u8g2.clearBuffer();
    u8g2.drawStr(16, 28, "Comm Test");
    u8g2.drawStr(28, 48, ROLE_NAME);
    u8g2.sendBuffer();
    delay(500);

    // Init CAN
    bool can_ok = can_init();

    // Show init result
    u8g2.clearBuffer();
    u8g2.drawStr(0, 12, ROLE_NAME);
    u8g2.drawStr(0, 28, can_ok ? "CAN: OK" : "CAN: FAIL");
    u8g2.drawStr(0, 42, "UART: 115200");
    u8g2.drawStr(0, 56, "Press KEY1 to send");
    u8g2.sendBuffer();
    delay(1000);

    last_oled_time = millis();
}

// ============================================================================
//  Main Loop
// ============================================================================

void loop() {
    uint32_t now = millis();

    // ----- KEY1: send on press -----
    bool key_state = digitalRead(PIN_KEY1);
    if (key_state == LOW && last_key_state == HIGH) {
        if (now - last_key_time > 200) {
            last_key_time = now;
            send_random_message();
        }
    }
    last_key_state = key_state;

    // ----- UART receive (accumulate until '\n') -----
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') {
            if (uart_buf_idx > 0) {
                uart_buf[uart_buf_idx] = '\0';
                strncpy(last_uart_rx, uart_buf, sizeof(last_uart_rx) - 1);
                last_uart_rx[sizeof(last_uart_rx) - 1] = '\0';
                uart_rx_count++;
                uart_buf_idx = 0;
                oled_dirty = true;
            }
        } else if (uart_buf_idx < sizeof(uart_buf) - 1) {
            uart_buf[uart_buf_idx++] = c;
        }
    }

    // ----- CAN receive -----
    if (can_rx_flag) {
        __disable_irq();
        uint8_t len = can_rx_len;
        char tmp[9];
        for (uint8_t i = 0; i < len && i < 8; i++) {
            tmp[i] = (char)can_rx_data[i];
        }
        tmp[len < 8 ? len : 8] = '\0';
        can_rx_flag = false;
        __enable_irq();

        strncpy(last_can_rx, tmp, sizeof(last_can_rx) - 1);
        last_can_rx[sizeof(last_can_rx) - 1] = '\0';
        can_rx_count++;
        oled_dirty = true;
    }

    // ----- OLED refresh (max 10Hz, only when dirty) -----
    if (oled_dirty && (now - last_oled_time >= 100)) {
        last_oled_time = now;
        oled_dirty = false;
        oled_update();
    }
}

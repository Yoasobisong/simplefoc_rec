#include "can_protocol.h"
#include <Arduino.h>

// STM32 HAL CAN handle
static CAN_HandleTypeDef hcan;

// Receive buffer (filled by ISR, consumed by main loop)
static volatile bool     rx_flag = false;
static volatile CanMessage rx_buf;

// Emergency stop flag (set in ISR for immediate response)
static volatile bool     estop_flag = false;

// Node ID (set during init)
static uint8_t my_node_id = 0;

// ============================================================================
//  CAN Initialization
// ============================================================================

bool can_init(uint8_t node_id) {
    my_node_id = node_id;

    // --- GPIO Init: PA11=CAN_RX, PA12=CAN_TX ---
    __HAL_RCC_CAN1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};

    // CAN TX - PA12, Alternate Push-Pull
    gpio.Pin   = GPIO_PIN_12;
    gpio.Mode  = GPIO_MODE_AF_PP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &gpio);

    // CAN RX - PA11, Input floating
    gpio.Pin  = GPIO_PIN_11;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &gpio);

    // --- CAN Peripheral Init ---
    // APB1 = 36MHz, Baud = 500kbps
    // Prescaler=4, BS1=13TQ, BS2=4TQ -> 36MHz/(4*(1+13+4)) = 500kHz
    hcan.Instance = CAN1;
    hcan.Init.Prescaler = 4;
    hcan.Init.Mode = CAN_MODE_NORMAL;
    hcan.Init.SyncJumpWidth = CAN_SJW_1TQ;
    hcan.Init.TimeSeg1 = CAN_BS1_13TQ;
    hcan.Init.TimeSeg2 = CAN_BS2_4TQ;
    hcan.Init.TimeTriggeredMode = DISABLE;
    hcan.Init.AutoBusOff = ENABLE;
    hcan.Init.AutoWakeUp = ENABLE;
    hcan.Init.AutoRetransmission = ENABLE;
    hcan.Init.ReceiveFifoLocked = DISABLE;
    hcan.Init.TransmitFifoPriority = DISABLE;

    if (HAL_CAN_Init(&hcan) != HAL_OK) {
        return false;
    }

    // --- Filter Config: Accept broadcast(0x000) + this node's IDs ---
    // 16-bit list mode: each filter bank holds 4 IDs
    // Filter bank 0: broadcast + position + config + status for this node
    CAN_FilterTypeDef filter = {0};
    filter.FilterBank = 0;
    filter.FilterMode = CAN_FILTERMODE_IDLIST;
    filter.FilterScale = CAN_FILTERSCALE_16BIT;
    filter.FilterFIFOAssignment = CAN_FILTER_FIFO0;
    filter.FilterActivation = ENABLE;

    // 16-bit ID list: each register holds 2 IDs
    // Format: ID[10:3] in bits[15:8], ID[2:0] in bits[7:5], RTR=0, IDE=0
    #define MAKE_16BIT_FILTER(id) ((uint16_t)((id) << 5))

    uint16_t id_broadcast = MAKE_16BIT_FILTER(CAN_ID_BROADCAST);
    uint16_t id_position  = MAKE_16BIT_FILTER(CAN_ID_POSITION_BASE + node_id);
    uint16_t id_config    = MAKE_16BIT_FILTER(CAN_ID_CONFIG_BASE + node_id);
    uint16_t id_status    = MAKE_16BIT_FILTER(CAN_ID_STATUS_BASE + node_id);

    filter.FilterIdHigh      = id_broadcast;
    filter.FilterIdLow       = id_position;
    filter.FilterMaskIdHigh  = id_config;
    filter.FilterMaskIdLow   = id_status;

    if (HAL_CAN_ConfigFilter(&hcan, &filter) != HAL_OK) {
        return false;
    }

    // --- Start CAN and enable FIFO0 message pending interrupt ---
    if (HAL_CAN_Start(&hcan) != HAL_OK) {
        return false;
    }

    if (HAL_CAN_ActivateNotification(&hcan, CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK) {
        return false;
    }

    // Enable CAN RX0 interrupt in NVIC
    HAL_NVIC_SetPriority(USB_LP_CAN1_RX0_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(USB_LP_CAN1_RX0_IRQn);

    return true;
}

// ============================================================================
//  CAN ISR
// ============================================================================

extern "C" void USB_LP_CAN1_RX0_IRQHandler(void) {
    HAL_CAN_IRQHandler(&hcan);
}

// HAL callback: called when a message arrives in FIFO0
extern "C" void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *h) {
    CAN_RxHeaderTypeDef header;
    uint8_t data[8];

    if (HAL_CAN_GetRxMessage(h, CAN_RX_FIFO0, &header, data) != HAL_OK) {
        return;
    }

    uint32_t id = header.StdId;

    // Handle emergency stop immediately in ISR context
    if (id == CAN_ID_BROADCAST && header.DLC >= 1 && data[0] == CMD_BROADCAST_ESTOP) {
        estop_flag = true;
        return;
    }

    // Copy to buffer for main loop processing
    rx_buf.id  = id;
    rx_buf.len = header.DLC;
    for (uint8_t i = 0; i < header.DLC && i < 8; i++) {
        rx_buf.data[i] = data[i];
    }
    rx_flag = true;
}

// ============================================================================
//  API Implementation
// ============================================================================

bool can_available(void) {
    return rx_flag;
}

void can_get_message(CanMessage &msg) {
    // Disable interrupt briefly to ensure consistent read
    __disable_irq();
    msg.id  = rx_buf.id;
    msg.len = rx_buf.len;
    for (uint8_t i = 0; i < rx_buf.len && i < 8; i++) {
        msg.data[i] = rx_buf.data[i];
    }
    rx_flag = false;
    __enable_irq();
}

bool can_send(const CanMessage &msg) {
    CAN_TxHeaderTypeDef header = {0};
    header.StdId = msg.id;
    header.IDE   = CAN_ID_STD;
    header.RTR   = CAN_RTR_DATA;
    header.DLC   = msg.len;

    uint32_t mailbox;
    if (HAL_CAN_AddTxMessage(&hcan, &header, (uint8_t*)msg.data, &mailbox) != HAL_OK) {
        return false;
    }

    // Wait for transmission to complete (timeout ~5ms)
    uint32_t start = millis();
    while (HAL_CAN_IsTxMessagePending(&hcan, mailbox)) {
        if (millis() - start > 5) return false;
    }
    return true;
}

void can_send_status(uint8_t node_id, float angle_deg, float velocity, uint8_t flags) {
    CanMessage msg;
    msg.id  = CAN_ID_STATUS_BASE + node_id;
    msg.len = 8;

    // Pack: [0..1] angle in 0.01 deg units (int16), [2..3] velocity in 0.01 rad/s (int16), [4] flags
    int16_t angle_raw = (int16_t)(angle_deg * 100.0f);
    int16_t vel_raw   = (int16_t)(velocity * 100.0f);

    msg.data[0] = (uint8_t)(angle_raw & 0xFF);
    msg.data[1] = (uint8_t)((angle_raw >> 8) & 0xFF);
    msg.data[2] = (uint8_t)(vel_raw & 0xFF);
    msg.data[3] = (uint8_t)((vel_raw >> 8) & 0xFF);
    msg.data[4] = flags;
    msg.data[5] = 0;
    msg.data[6] = 0;
    msg.data[7] = 0;

    can_send(msg);
}

bool can_estop_triggered(void) {
    if (estop_flag) {
        estop_flag = false;
        return true;
    }
    return false;
}

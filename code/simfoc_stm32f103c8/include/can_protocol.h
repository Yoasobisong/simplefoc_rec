#ifndef CAN_PROTOCOL_H
#define CAN_PROTOCOL_H

#include <stdint.h>

// ============================================================================
//  CAN Protocol Definition for SimpleFOC Servo Network
// ============================================================================
//
//  CAN ID Layout (standard 11-bit):
//    0x000           Broadcast (e-stop / enable / home)
//    0x100 + nodeId  Position command
//    0x200 + nodeId  Configuration command
//    0x300 + nodeId  Status feedback (sent by node)
//
//  Node ID range: 1..30
//
// ============================================================================

// --- CAN ID Base ---
#define CAN_ID_BROADCAST     0x000
#define CAN_ID_POSITION_BASE 0x100
#define CAN_ID_CONFIG_BASE   0x200
#define CAN_ID_STATUS_BASE   0x300

// --- Broadcast sub-commands (data[0]) ---
#define CMD_BROADCAST_ESTOP   0x00  // Emergency stop all motors
#define CMD_BROADCAST_ENABLE  0x01  // Enable all motors, data[1]=0/1
#define CMD_BROADCAST_HOME    0x02  // All motors go to zero position

// --- Config sub-commands (data[0]) ---
#define CMD_CONFIG_SET_LIMITS 0x01  // Set angle limits: data[1..4]=min, data[5..8] not used (2nd frame)
#define CMD_CONFIG_SAVE       0x02  // Save current config to EEPROM
#define CMD_CONFIG_RESET      0x03  // Reset to factory defaults

// --- Status flags (bitfield in status byte) ---
#define STATUS_FLAG_ENABLED   0x01
#define STATUS_FLAG_ON_TARGET 0x02
#define STATUS_FLAG_ERROR     0x80

// --- Timing ---
#define CAN_STATUS_INTERVAL_MS 100  // Status report interval

// ============================================================================
//  CAN Message Structure
// ============================================================================

struct CanMessage {
    uint32_t id;        // CAN ID (standard 11-bit)
    uint8_t  len;       // Data length (0..8)
    uint8_t  data[8];   // Payload
};

// ============================================================================
//  API Functions
// ============================================================================

// Initialize CAN peripheral (PA11=RX, PA12=TX, 500kbps)
// Returns true on success.
bool can_init(uint8_t node_id);

// Check if a new message is available (call from main loop)
bool can_available(void);

// Get the received message (valid after can_available() returns true)
void can_get_message(CanMessage &msg);

// Send a CAN message. Returns true on success.
bool can_send(const CanMessage &msg);

// Send status feedback frame
// angle_deg: current angle in degrees
// velocity: current velocity in rad/s
// flags: STATUS_FLAG_xxx bitfield
void can_send_status(uint8_t node_id, float angle_deg, float velocity, uint8_t flags);

// Check if emergency stop was triggered (set in ISR, cleared by this call)
bool can_estop_triggered(void);

#endif // CAN_PROTOCOL_H

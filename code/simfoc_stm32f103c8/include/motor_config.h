#ifndef MOTOR_CONFIG_H
#define MOTOR_CONFIG_H

#include <stdint.h>

// EEPROM storage layout (10 bytes total):
//   [0]     magic       (1 byte)  = 0xA5
//   [1..4]  angle_min   (4 bytes) float, degrees
//   [5..8]  angle_max   (4 bytes) float, degrees
//   [9]     checksum    (1 byte)  XOR of bytes [0..8]

#define CONFIG_MAGIC      0xA5
#define CONFIG_EEPROM_ADDR 0   // start address in EEPROM

// Default angle limits for GM2208 (degrees)
#define DEFAULT_ANGLE_MIN  0.0f
#define DEFAULT_ANGLE_MAX  290.0f

struct MotorConfig {
    float angle_min;   // minimum angle limit (degrees)
    float angle_max;   // maximum angle limit (degrees)
};

// Load config from EEPROM. Returns true if valid config found.
bool config_load(MotorConfig &cfg);

// Save config to EEPROM.
void config_save(const MotorConfig &cfg);

// Reset config to factory defaults and save.
void config_reset(MotorConfig &cfg);

// Convert degrees to radians
inline float deg2rad(float deg) { return deg * 0.017453293f; }

// Convert radians to degrees
inline float rad2deg(float rad) { return rad * 57.29577951f; }

#endif // MOTOR_CONFIG_H

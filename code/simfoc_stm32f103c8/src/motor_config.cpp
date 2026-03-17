#include "motor_config.h"
#include <EEPROM.h>

// Compute XOR checksum over a byte array
static uint8_t compute_checksum(const uint8_t *data, uint8_t len) {
    uint8_t cs = 0;
    for (uint8_t i = 0; i < len; i++) {
        cs ^= data[i];
    }
    return cs;
}

bool config_load(MotorConfig &cfg) {
    uint8_t buf[10];

    // Read raw bytes from EEPROM
    for (uint8_t i = 0; i < 10; i++) {
        buf[i] = EEPROM.read(CONFIG_EEPROM_ADDR + i);
    }

    // Verify magic byte
    if (buf[0] != CONFIG_MAGIC) {
        cfg.angle_min = DEFAULT_ANGLE_MIN;
        cfg.angle_max = DEFAULT_ANGLE_MAX;
        return false;
    }

    // Verify checksum (XOR of bytes 0..8)
    uint8_t cs = compute_checksum(buf, 9);
    if (cs != buf[9]) {
        cfg.angle_min = DEFAULT_ANGLE_MIN;
        cfg.angle_max = DEFAULT_ANGLE_MAX;
        return false;
    }

    // Extract float values
    memcpy(&cfg.angle_min, &buf[1], 4);
    memcpy(&cfg.angle_max, &buf[5], 4);

    return true;
}

void config_save(const MotorConfig &cfg) {
    uint8_t buf[10];

    buf[0] = CONFIG_MAGIC;
    memcpy(&buf[1], &cfg.angle_min, 4);
    memcpy(&buf[5], &cfg.angle_max, 4);
    buf[9] = compute_checksum(buf, 9);

    // Write to EEPROM (only writes changed bytes)
    for (uint8_t i = 0; i < 10; i++) {
        EEPROM.write(CONFIG_EEPROM_ADDR + i, buf[i]);
    }
}

void config_reset(MotorConfig &cfg) {
    cfg.angle_min = DEFAULT_ANGLE_MIN;
    cfg.angle_max = DEFAULT_ANGLE_MAX;
    config_save(cfg);
}

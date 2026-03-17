#include <Arduino.h>
#include <SimpleFOC.h>
#include "can_protocol.h"
#include "motor_config.h"

// ============================================================================
//  SimpleFOC CAN Servo Controller
//  STM32F103C8 + DRV8313 + AS5600 + GM2208
// ============================================================================

// ----- Motor -----
// GM2208 gimbal motor, 7 pole pairs
BLDCMotor motor = BLDCMotor(7);

// ----- Driver -----
// DRV8313: 3-PWM mode, enable pin on PB15
BLDCDriver3PWM driver = BLDCDriver3PWM(PA8, PA9, PA10, PB15);

// ----- Sensor -----
// AS5600 magnetic encoder on I2C1 (PB6/PB7)
MagneticSensorI2C sensor = MagneticSensorI2C(AS5600_I2C);

// ----- Commander (debug only) -----
#ifdef DEBUG_MODE
Commander command = Commander(Serial);
void doTarget(char *cmd) { command.scalar(&motor.target, cmd); }
void doMotor(char *cmd) { command.motor(&motor, cmd); }
#endif

// ----- Pin Definitions -----
#define PIN_LED PC13
#define PIN_KEY1 PA15

// ----- CAN Node ID -----
#ifndef CAN_NODE_ID
#define CAN_NODE_ID 1
#endif

// ----- Motor Config -----
MotorConfig motor_cfg;

// ----- State -----
static bool motor_enabled = true;
static uint32_t last_status_time = 0;

// ============================================================================
//  CAN Message Handler
// ============================================================================

void handle_can_message(const CanMessage &msg)
{
  uint8_t node_id = CAN_NODE_ID;

  // --- Broadcast commands ---
  if (msg.id == CAN_ID_BROADCAST && msg.len >= 1)
  {
    switch (msg.data[0])
    {
    case CMD_BROADCAST_ENABLE:
      if (msg.len >= 2)
      {
        motor_enabled = (msg.data[1] != 0);
        if (!motor_enabled)
        {
          motor.disable();
        }
        else
        {
          motor.enable();
        }
      }
      break;
    case CMD_BROADCAST_HOME:
      motor.target = deg2rad(motor_cfg.angle_min);
      break;
    }
    return;
  }

  // --- Position command: 0x100 + nodeId ---
  if (msg.id == (CAN_ID_POSITION_BASE + node_id) && msg.len >= 4)
  {
    // data[0..3] = target angle in degrees (float)
    float target_deg;
    memcpy(&target_deg, msg.data, 4);

    // Clamp to configured limits
    if (target_deg < motor_cfg.angle_min)
      target_deg = motor_cfg.angle_min;
    if (target_deg > motor_cfg.angle_max)
      target_deg = motor_cfg.angle_max;

    motor.target = deg2rad(target_deg);

#ifdef DEBUG_MODE
    Serial.print(F("CAN pos: "));
    Serial.print(target_deg, 1);
    Serial.println(F(" deg"));
#endif
    return;
  }

  // --- Config command: 0x200 + nodeId ---
  if (msg.id == (CAN_ID_CONFIG_BASE + node_id) && msg.len >= 1)
  {
    switch (msg.data[0])
    {
    case CMD_CONFIG_SET_LIMITS:
      // data[1..4] = angle_min (float), need 2 frames for both
      // For simplicity: data[1..4]=min, second frame or use 2 commands
      if (msg.len >= 5)
      {
        float val;
        memcpy(&val, &msg.data[1], 4);
        // data[5] selects which limit: 0=min, 1=max
        if (msg.len >= 6 && msg.data[5] == 1)
        {
          motor_cfg.angle_max = val;
        }
        else
        {
          motor_cfg.angle_min = val;
        }
#ifdef DEBUG_MODE
        Serial.print(F("CAN limit set: min="));
        Serial.print(motor_cfg.angle_min, 1);
        Serial.print(F(" max="));
        Serial.println(motor_cfg.angle_max, 1);
#endif
      }
      break;
    case CMD_CONFIG_SAVE:
      config_save(motor_cfg);
#ifdef DEBUG_MODE
      Serial.println(F("CAN: config saved"));
#endif
      break;
    case CMD_CONFIG_RESET:
      config_reset(motor_cfg);
#ifdef DEBUG_MODE
      Serial.println(F("CAN: config reset"));
#endif
      break;
    }
    return;
  }
}

// ============================================================================
//  Setup
// ============================================================================

void setup()
{
#ifdef DEBUG_MODE
  // Serial on USART3 (PB10/PB11)
  Serial.begin(115200);
  delay(500);
  Serial.println(F("=========================================="));
  Serial.println(F("  SimpleFOC CAN Servo - DEBUG MODE"));
  Serial.println(F("  Node ID: " __XSTRING(CAN_NODE_ID)));
  Serial.println(F("=========================================="));
#endif

  // --- GPIO Init ---
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW); // LED ON (active low)
  pinMode(PIN_KEY1, INPUT_PULLUP);

  // --- Load config from EEPROM ---
  if (config_load(motor_cfg))
  {
#ifdef DEBUG_MODE
    Serial.print(F("Config loaded: "));
    Serial.print(motor_cfg.angle_min, 1);
    Serial.print(F(" - "));
    Serial.print(motor_cfg.angle_max, 1);
    Serial.println(F(" deg"));
#endif
  }
  else
  {
#ifdef DEBUG_MODE
    Serial.println(F("Config: using defaults (0-300 deg)"));
#endif
  }

  // ===== 1. Sensor Setup =====
  Wire.begin();
  sensor.init();
#ifdef DEBUG_MODE
  Serial.print(F("Sensor angle: "));
  Serial.println(sensor.getAngle(), 4);
#endif

  // ===== 2. Driver Setup =====
  driver.voltage_power_supply = 12;
  driver.voltage_limit = 6;
  if (driver.init() == 0)
  {
#ifdef DEBUG_MODE
    Serial.println(F("ERROR: Driver init failed!"));
#endif
    while (1)
    {
      // Blink LED fast to indicate error
      digitalWrite(PIN_LED, !digitalRead(PIN_LED));
      delay(200);
    }
  }
#ifdef DEBUG_MODE
  Serial.println(F("Driver initialized."));
#endif

  // ===== 3. Motor Setup =====
  motor.linkSensor(&sensor);
  motor.linkDriver(&driver);

  motor.foc_modulation = FOCModulationType::SpaceVectorPWM;
  motor.controller = MotionControlType::angle;

  // Velocity PID (inner loop)
  motor.PID_velocity.P = 0.2f;
  motor.PID_velocity.I = 20.0f;
  motor.PID_velocity.D = 0.001f;
  motor.LPF_velocity.Tf = 0.01f;

  // Position P controller (outer loop)
  motor.P_angle.P = 20.0f;

  // Safety limits
  motor.voltage_limit = 6;
  motor.velocity_limit = 20;

#ifdef DEBUG_MODE
  motor.useMonitoring(Serial);
  motor.monitor_downsample = 100;
#endif

  motor.init();

  // ===== 4. FOC Init =====
  motor.initFOC();

  // ===== 5. CAN Init =====
  if (can_init(CAN_NODE_ID))
  {
#ifdef DEBUG_MODE
    Serial.println(F("CAN initialized (500kbps)."));
#endif
  }
  else
  {
#ifdef DEBUG_MODE
    Serial.println(F("ERROR: CAN init failed!"));
#endif
  }

// ===== 6. Commander Setup (debug only) =====
#ifdef DEBUG_MODE
  command.add('T', doTarget, "target angle (rad)");
  command.add('M', doMotor, "motor config");
#endif

  // Start at current position
  motor.target = sensor.getAngle();

  digitalWrite(PIN_LED, HIGH); // LED OFF = init complete

#ifdef DEBUG_MODE
  Serial.println(F("=========================================="));
  Serial.println(F("  READY! Serial: T3.14 / M"));
  Serial.println(F("  CAN: position/config/status active"));
  Serial.println(F("=========================================="));
#endif

  last_status_time = millis();
}

// ============================================================================
//  Key handling
// ============================================================================

static uint32_t last_key_time = 0;
static bool last_key_state = HIGH;
static const float preset_angles[] = {0, 1.5708f, 3.1416f, 4.7124f};
static uint8_t preset_index = 0;

// ============================================================================
//  Main Loop
// ============================================================================

void loop()
{
  // ===== FOC Core (must run as fast as possible) =====
  motor.loopFOC();
  motor.move();

  // ===== Emergency Stop Check =====
  if (can_estop_triggered())
  {
    motor.disable();
    motor_enabled = false;
#ifdef DEBUG_MODE
    Serial.println(F("!!! EMERGENCY STOP !!!"));
#endif
    // Blink LED rapidly
    digitalWrite(PIN_LED, LOW);
  }

  // ===== CAN Message Processing =====
  if (can_available())
  {
    CanMessage msg;
    can_get_message(msg);
    handle_can_message(msg);
  }

  // ===== Periodic Status Report =====
  uint32_t now = millis();
  if (now - last_status_time >= CAN_STATUS_INTERVAL_MS)
  {
    last_status_time = now;

    float angle_deg = rad2deg(sensor.getAngle());
    float velocity = motor.shaft_velocity;
    uint8_t flags = 0;

    if (motor_enabled)
      flags |= STATUS_FLAG_ENABLED;
    // On-target: within 2 degrees
    float target_deg = rad2deg(motor.target);
    if (fabsf(angle_deg - target_deg) < 2.0f)
    {
      flags |= STATUS_FLAG_ON_TARGET;
    }

    can_send_status(CAN_NODE_ID, angle_deg, velocity, flags);
  }

// ===== Serial Commander (debug only) =====
#ifdef DEBUG_MODE
  command.run();
#endif

  // ===== Key1 Handler =====
  bool key_state = digitalRead(PIN_KEY1);
  if (key_state == LOW && last_key_state == HIGH)
  {
    if (now - last_key_time > 200)
    {
      last_key_time = now;
      preset_index = (preset_index + 1) % 4;
      motor.target = preset_angles[preset_index];

#ifdef DEBUG_MODE
      Serial.print(F("KEY1 -> "));
      Serial.print(preset_angles[preset_index] * 180.0f / PI, 0);
      Serial.println(F(" deg"));
#endif

      digitalWrite(PIN_LED, LOW);
      delay(50);
      digitalWrite(PIN_LED, HIGH);
    }
  }
  last_key_state = key_state;
}

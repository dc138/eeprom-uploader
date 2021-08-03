#include <Arduino.h>

constexpr uint8_t version = 0x02;

typedef struct state_flags_t {
  bool receiving = false;
  bool sending   = false;
  bool high      = false;
  bool low       = false;
} state_flags_t;

state_flags_t state_flags;

void setup() {
  Serial.begin(9600);
  Serial.write(0x01);
  Serial.write(version);
}

void loop() {
  if (Serial.available() > 1) {
    uint8_t data_high = 0x00;
    uint8_t data_low  = 0x00;

    Serial.readBytes(&data_high, 1);
    Serial.readBytes(&data_low, 1);
  }
}

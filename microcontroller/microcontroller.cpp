#include <Arduino.h>

#include "eeprom.hpp"

constexpr uint8_t version = 0x02;

typedef struct state_flags_t {
  bool receiving_data  = false;
  bool receiving_flags = false;
  bool sending         = false;
  bool high            = false;
  bool low             = false;

  uint8_t recv_size           = 0x00;
  uint8_t recv_buff_pos       = 0x00;
  uint8_t recv_buff_high[256] = {};
  uint8_t recv_buff_low[256]  = {};

  uint8_t send_size           = 0x00;
  uint8_t send_buff_pos       = 0x00;
  uint8_t send_buff_high[256] = {};
  uint8_t send_buff_low[256]  = {};
} state_flags_t;

EEPROM        eeprom(14, 13, 9, 10, 12, 2, 3, 11, 17, 16, 15, 8, 7, 6, 5, 4);
state_flags_t state;

void setup() {
  eeprom.init();

  Serial.begin(9600);
  Serial.write(0x01);
  Serial.write(version);
}

void panic() {
  cli();

  while (true) {
    __asm__("nop\n\t");
  }
}

void loop() {
  if (state.sending) {
    uint8_t i = 0x00;

    do {
      if (!state.low) {
        state.send_buff_high[i] = eeprom.read_high();
      }

      if (!state.high) {
        state.send_buff_low[i] = eeprom.read_low();
      }

      eeprom.next();
    } while (i++ < 0xFF);

    Serial.write(0x08);
    Serial.write(0xFF);

    i = 0;

    do {
      Serial.write(state.send_buff_high[i]);
      Serial.write(state.send_buff_low[i]);
    } while (i++ < 0xFF);

    state.sending = false;
  }

  if (Serial.available() > 1) {
    uint8_t data_high = 0x00;
    uint8_t data_low  = 0x00;

    Serial.readBytes(&data_high, 1);
    Serial.readBytes(&data_low, 1);

    if (state.receiving_data) {
      state.recv_buff_high[state.recv_buff_pos] = data_high;
      state.recv_buff_low[state.recv_buff_pos]  = data_low;

      if (state.recv_size == 0) {
        state.recv_buff_pos  = 0;
        state.receiving_data = false;

        uint8_t i        = 0x00;
        uint8_t attempts = 0x00;
        uint8_t errors   = 0x00;

        do {
          if (!state.low) {
            do {
              eeprom.write_high(state.recv_buff_high[i]);

            } while ((attempts++ < 10) && (eeprom.read_high() != state.recv_buff_high[i]));

            if (eeprom.read_high() != state.recv_buff_high[i]) {
              Serial.write(0x04);
              Serial.write(0x00);
              errors++;
            }
          }

          attempts = 0;

          if (!state.high) {
            do {
              eeprom.write_low(state.recv_buff_low[i]);

            } while ((attempts++ < 10) && (eeprom.read_low() != state.recv_buff_low[i]));

            if (eeprom.read_low() != state.recv_buff_low[i]) {
              Serial.write(0x04);
              Serial.write(0xFF);
              errors++;
            }
          }

          eeprom.next();
        } while (i++ < 0xFF);  // TODO: This may vary

        Serial.write(0x07);
        Serial.write(errors);

      } else {
        state.recv_size--;
        state.recv_buff_pos++;
      }

    } else if (state.receiving_flags) {
      switch (state.recv_buff_pos) {
        case 0x00:
          state.high = data_high;
          state.low  = data_low;

          state.recv_size--;
          state.recv_buff_pos++;
          break;

        case 0x01:
          state.sending = data_high;

          state.recv_size     = 0;
          state.recv_buff_pos = 0;

          Serial.write(0x05);
          Serial.write(0x01);

          state.receiving_flags = false;
          break;

        default:
          Serial.write(0x03);
          Serial.write(0x02);

          panic();

          state.recv_size     = 0;
          state.recv_buff_pos = 0;
          break;
      }

    } else {
      if (data_high == 0x02) {
        state.receiving_flags = true;
        state.recv_size       = data_low;

      } else if (data_high == 0x06) {
        state.receiving_data = true;
        state.recv_size      = data_low;

      } else {
        Serial.write(0x03);
        Serial.write(0x01);

        panic();
      }
    }
  }
}

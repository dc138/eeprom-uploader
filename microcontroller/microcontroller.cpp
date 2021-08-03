#include <Arduino.h>

constexpr uint8_t version = 0x02;

typedef struct state_flags_t {
  bool receiving_data  = false;
  bool receiving_flags = false;
  bool sending         = false;
  bool high            = false;
  bool low             = false;
  int  rec_size        = -1;
} state_flags_t;

state_flags_t state;

uint8_t rec_buff_high[256];
uint8_t rec_buff_low[256];
uint8_t rec_buff_pos = 0x00;

void setup() {
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
    // TODO: Send data
  }

  if (Serial.available() > 1) {
    uint8_t data_high = 0x00;
    uint8_t data_low  = 0x00;

    Serial.readBytes(&data_high, 1);
    Serial.readBytes(&data_low, 1);

    if (state.receiving_data && state.rec_size >= 0) {
      // TODO: Receive data

    } else if (state.receiving_flags && state.rec_size >= 0) {
      switch (rec_buff_pos) {
        case 0x00:
          state.high = data_high;
          state.low  = data_low;

          state.rec_size--;
          rec_buff_pos++;
          break;

        case 0x01:
          state.sending        = data_high;
          state.receiving_data = data_low;

          state.rec_size = -1;
          rec_buff_pos   = 0;

          if (state.receiving_data) state.rec_size = 0xFF;

          Serial.write(0x05);
          Serial.write(0x01);

          state.receiving_flags = false;

          break;

        default:
          Serial.write(0x03);
          Serial.write(0x02);

          panic();

          state.rec_size = 0;
          rec_buff_pos   = 0;
          break;
      }

    } else {
      if (data_high == 0x02) {
        state.receiving_flags = true;
        state.rec_size        = data_low;

      } else {
        Serial.write(0x03);
        Serial.write(0x01);

        panic();
      }
    }
  }
}

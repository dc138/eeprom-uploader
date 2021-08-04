#include <libserial/SerialPort.h>
namespace ls = LibSerial;

#include <fmt/color.h>
#include <fmt/core.h>

#include <iomanip>
#include <iostream>
#include <stypox/argparser.hpp>
namespace sp = stypox;

constexpr uint8_t version = 0x02;

typedef struct state_flags_t {
  bool    receiving      = false;
  bool    sending        = false;
  bool    high           = false;
  bool    low            = false;
  bool    ready          = false;
  bool    setup          = true;
  bool    handshake      = false;
  bool    debug          = false;
  bool    waiting        = false;
  uint8_t rec_size       = 0x00;
  uint8_t rec_buffer_pos = 0x00;
  uint8_t rec_buffer_high[256];
  uint8_t rec_buffer_low[256];
} state_flags_t;

typedef struct args_t {
  std::string port         = "";
  std::string send_file    = "";
  std::string receive_file = "";
  bool        help         = false;
  bool        high         = false;
  bool        low          = false;
} args_t;

state_flags_t state;
args_t        args;

int main(int argc, const char* argv[]) {
  sp::ArgParser parser {
      std::make_tuple(sp::HelpSection("\nAvailable options:"),
                      sp::SwitchOption {"help", args.help, sp::args("-?", "--help"), "Show help options"},
                      sp::SwitchOption {"high", args.high, sp::args("-h", "--high"), "Use high mode"},
                      sp::SwitchOption {"low", args.low, sp::args("-l", "--low"), "Use low mode"},
                      sp::Option {"port", args.port, sp::args("-p", "--port"), "Port to use", true},
                      sp::Option {"rfile", args.receive_file, sp::args("-r", "--receive"), "File to receive into"},
                      sp::Option {"sfile", args.send_file, sp::args("-s", "--send"), "File to send"}),
      "Very Simple Architecture EEPROM Programmer\n"};

  try {
    parser.parse(argc, argv);
  } catch (std::runtime_error& err) {
    std::cout << err.what() << std::endl;
    exit(1);
  }

  if (args.help) {
    std::cout << parser.help();
    exit(0);
  }

  try {
    parser.validate();
  } catch (std::runtime_error& err) {
    std::cout << err.what() << std::endl;
    exit(1);
  }

  if (args.port.starts_with('=')) {
    args.port.erase(args.port.begin());
  }

  fmt::print("[INF] Using version {:#x}\n", version);

  ls::SerialPort port {};

  try {
    port.Open(args.port);
  } catch (std::runtime_error& err) {
    std::cout << err.what() << std::endl;
    exit(4);
  }

  port.SetBaudRate(ls::BaudRate::BAUD_9600);
  port.SetCharacterSize(ls::CharacterSize::CHAR_SIZE_8);
  port.SetFlowControl(ls::FlowControl::FLOW_CONTROL_NONE);
  port.SetParity(ls::Parity::PARITY_NONE);

  fmt::print("[INF] Port opened\n[INF] Waiting for controller\n");

  // TODO: Check files
  // TODO: Validate send / receive

  do {
    if (state.sending) {
      // TODO: Send real data
      fmt::print("[INF] Sending data to controller\n");
      uint8_t i = 0;

      port.WriteByte((uint8_t)0x06);
      port.WriteByte((uint8_t)0xFF);
      fmt::print(fmt::fg(fmt::terminal_color::blue), "[OUT] {:#x} {:#x}\n", 0x06, 0xFF);

      do {
        port.WriteByte((uint8_t)i);
        port.WriteByte((uint8_t)(0xFF - i));
        fmt::print(fmt::fg(fmt::terminal_color::blue), "[OUT] {:#x} {:#x}\n", 0x00, 0x00);
      } while (i++ < 0xFF);

      state.sending = false;
      state.waiting = true;
    }

    if (port.GetNumberOfBytesAvailable() > 1) {
      uint8_t data_high = 0x00;
      uint8_t data_low  = 0x00;

      port.ReadByte(data_high);
      port.ReadByte(data_low);

      fmt::print(fmt::fg(fmt::terminal_color::bright_green), "[REC] {:#x} {:#x}\n", data_high, data_low);

      if (state.receiving) {
        // TODO: Write data to file

        state.rec_buffer_high[state.rec_buffer_pos] = data_high;
        state.rec_buffer_low[state.rec_buffer_pos]  = data_low;

        if (state.rec_size == 0) {
          state.rec_buffer_pos = 0;
          state.receiving      = false;

          fmt::print("[INF] Done receiving data\n");

        } else {
          state.rec_size--;
          state.rec_buffer_pos++;
        }

      } else {
        if (data_high == 0x01) {
          if (data_low != version) {
            fmt::print(fmt::fg(fmt::terminal_color::red),
                       "[ERR] We are using version {:#x} but controller is on version {:#x}\n",
                       version,
                       data_low);
            exit(2);
          }

          state.handshake = true;
          state.setup     = false;

          fmt::print("[INF] Performing initial handshake\n");

          port.WriteByte((uint8_t)0x02);
          port.WriteByte((uint8_t)0x01);
          fmt::print(fmt::fg(fmt::terminal_color::blue), "[OUT] {:#x} {:#x}\n", 0x02, 0x02);

          port.WriteByte((uint8_t)args.high);
          port.WriteByte((uint8_t)args.low);
          fmt::print(fmt::fg(fmt::terminal_color::blue), "[OUT] {:#x} {:#x}\n", args.high, args.low);

          port.WriteByte((uint8_t)(args.receive_file.empty() ? 0x00 : 0x01));
          port.WriteByte((uint8_t)(args.send_file.empty() ? 0x00 : 0x01));
          fmt::print(fmt::fg(fmt::terminal_color::blue),
                     "[OUT] {:#x} {:#x}\n",
                     args.receive_file.empty() ? 0x00 : 0x01,
                     args.send_file.empty() ? 0x00 : 0x01);

        } else if (data_high == 0x03) {
          fmt::print(fmt::fg(fmt::terminal_color::red),
                     "[ERR] Received abort packed with parameter {:#x}, aborting...\n",
                     data_low);
          break;

        } else if (data_high == 0x04) {
          fmt::print(
              fmt::fg(fmt::terminal_color::yellow), "[DBG] Received debug packet with parameter {:#x}\n", data_low);

        } else if (data_high == 0x05) {
          state.handshake = false;

          if (!args.receive_file.empty()) {
            state.waiting = true;

          } else if (!args.send_file.empty()) {
            state.sending = true;

          } else {
            fmt::print("[INF] Nothing to do\n");
          }

        } else if (data_high == 0x07) {
          state.waiting = false;
          fmt::print("[INF] Done sending data, controller succesfully wrote {:#x} words\n", data_low);

        } else if (data_high == 0x08) {
          state.receiving = true;
          state.waiting   = false;
          state.rec_size  = data_low;

          fmt::print("[INF] Receiving {:#x} words of data\n", data_low);

        } else {
          fmt::print(fmt::fg(fmt::terminal_color::red), "[ERR] Received unknown data packet, aborting...\n");
          break;
        }
      }
    }
  } while (state.receiving || state.ready || state.setup || state.handshake || state.sending || state.debug ||
           state.waiting);

  std::cout << "[INF] Connection ended\n";

  port.Close();
  std::cout << "[INF] Closed port\n";

  return 0;
}

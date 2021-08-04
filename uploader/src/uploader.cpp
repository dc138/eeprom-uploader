// Serial port
#include <libserial/SerialPort.h>
namespace ls = LibSerial;

// Formatting
#include <fmt/color.h>
#include <fmt/core.h>

// STL
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <thread>

using namespace std::chrono_literals;

// Argument parser
#include <stypox/argparser.hpp>
namespace sp = stypox;

constexpr uint8_t version = 0x02;

typedef struct state_t {
  bool receiving = false;
  bool sending   = false;
  bool high      = false;
  bool low       = false;
  bool ready     = false;
  bool setup     = true;
  bool handshake = false;
  bool debug     = false;
  bool waiting   = false;

  uint8_t recv_size             = 0x00;
  uint8_t recv_buffer_pos       = 0x00;
  uint8_t recv_buffer_high[256] = {};
  uint8_t recv_buffer_low[256]  = {};

  uint8_t send_size             = 0x00;
  uint8_t send_buffer_pos       = 0x00;
  uint8_t send_buffer_high[256] = {};
  uint8_t send_buffer_low[256]  = {};
} state_t;

typedef struct args_t {
  std::string port         = "";
  std::string send_file    = "";
  std::string receive_file = "";
  bool        help         = false;
  bool        high         = false;
  bool        low          = false;
  bool        overwrite    = false;
} args_t;

state_t state;
args_t  args;

void send_word(ls::SerialPort& port, uint8_t data_high, uint8_t data_low);

int main(int argc, const char* argv[]) {
  sp::ArgParser parser {
      std::make_tuple(
          sp::HelpSection("\nAvailable options:"),
          sp::SwitchOption {"help", args.help, sp::args("-?", "--help"), "Show help options"},
          sp::SwitchOption {"high", args.high, sp::args("-h", "--high"), "Use high mode"},
          sp::SwitchOption {"low", args.low, sp::args("-l", "--low"), "Use low mode"},
          sp::SwitchOption {"overwrite", args.overwrite, sp::args("-o", "--overwrite"), "Overwrite output file"},
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

  if (args.send_file.starts_with('=')) {
    args.send_file.erase(args.send_file.begin());
  }

  if (args.receive_file.starts_with('=')) {
    args.receive_file.erase(args.receive_file.begin());
  }

  fmt::print("[INF] Using version {:#x}\n", version);

  if (args.high && args.low) {
    fmt::print(fmt::fg(fmt::terminal_color::red),
               "[ERR] Using both high and low mode is the same as not using any of them\n");
    exit(5);
  }

  if (!args.send_file.empty() && !args.receive_file.empty()) {
    fmt::print(fmt::fg(fmt::terminal_color::red), "[ERR] Cannot send and receive in the same session\n");
    exit(6);
  }

  std::ofstream recvf;
  std::ifstream sendf;

  try {
    if (!args.send_file.empty()) {
      if (!std::filesystem::exists(args.send_file)) {
        fmt::print("[INF] File {} doesn't exist\n", args.send_file);
        exit(7);
      }

      sendf.open(args.send_file, std::ofstream::binary);
      fmt::print("[INF] Opened {}\n", args.send_file);

      if (args.high || args.low) {
        if (std::filesystem::file_size(args.send_file) != 256) {
          fmt::print(fmt::fg(fmt::terminal_color::red),
                     "[ERR] Using single byte mode, but {} isn't exactly 256 bytes long\n",
                     args.send_file);
          exit(9);
        }

      } else {
        if (std::filesystem::file_size(args.send_file) != 512) {
          fmt::print(fmt::fg(fmt::terminal_color::red),
                     "[ERR] Using single byte mode, but {} isn't exactly 512 bytes long\n",
                     args.send_file);
          exit(9);
        }
      }

      fmt::print("[INF] Reading {}...\n", args.send_file);
      uint8_t i = 0;

      do {
        if (!args.low) {
          sendf.read((char*)(state.send_buffer_high + i), 1);
        }

        if (!args.high) {
          sendf.read((char*)(state.send_buffer_low) + i, 1);
        }

        /*fmt::print(fmt::fg(fmt::terminal_color::yellow),
                   "[DBG] Send buffer {:#x} : {:#x} {:#x}\n",
                   i,
                   state.send_buffer_high[i],
                   state.send_buffer_low[i]);*/
      } while (i++ < 0xFF);
    }

    if (!args.receive_file.empty()) {
      if (std::filesystem::exists(args.receive_file)) {
        if (!args.overwrite) {
          fmt::print("[INF] File {} already exists and --overwrite is not set, refusing to continue\n",
                     args.receive_file);
          exit(7);

        } else {
          std::filesystem::remove(args.receive_file);
        }
      }

      recvf.open(args.receive_file, std::ifstream::binary);
      fmt::print("[INF] Opened {}\n", args.receive_file);
    }
  } catch (std::filesystem::filesystem_error& err) {
    fmt::print(fmt::fg(fmt::terminal_color::red), "[ERR] Filesystem error thrown: {}\n", err.what());
    exit(8);
  }

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

  do {
    if (state.sending) {
      fmt::print("[INF] Sending data to controller\n");
      send_word(port, 0x06, 0xFF);

      uint8_t i = 0;

      do {
        send_word(port, state.send_buffer_high[i], state.send_buffer_low[i]);
      } while (i++ < 0xFF);

      fmt::print("[INF] Waiting for controller to write data\n");
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
        state.recv_buffer_high[state.recv_buffer_pos] = data_high;
        state.recv_buffer_low[state.recv_buffer_pos]  = data_low;

        if (state.recv_size == 0) {
          state.recv_buffer_pos = 0;
          state.receiving       = false;

          fmt::print("[INF] Done receiving data, writting to {}\n", args.receive_file);
          uint8_t i = 0;

          do {
            if (!args.low) recvf.put((uint8_t)(state.recv_buffer_high[i]));
            if (!args.high) recvf.put((uint8_t)(state.recv_buffer_low[i]));
            /* fmt::print(fmt::fg(fmt::terminal_color::yellow),
                       "[DBG] Writting {:#x} {:#x} to {}\n",
                       state.rec_buffer_high[i],
                       state.rec_buffer_low[i],
                       args.receive_file);*/
          } while (i++ < 0xFF);

          recvf.flush();

        } else {
          state.recv_size--;
          state.recv_buffer_pos++;
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

          send_word(port, 0x02, 0x01);
          send_word(port, args.high, args.low);
          send_word(port, args.receive_file.empty() ? 0x00 : 0x01, args.send_file.empty() ? 0x00 : 0x01);

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
            fmt::print("[INF] Waiting for controller to read and send data\n");
            state.waiting = true;

          } else if (!args.send_file.empty()) {
            state.sending = true;

          } else {
            fmt::print("[INF] Nothing to do\n");
          }

        } else if (data_high == 0x07) {
          state.waiting = false;
          fmt::print("[INF] Controller wrote data with {:#x} errors\n", data_low);

        } else if (data_high == 0x08) {
          state.receiving = true;
          state.waiting   = false;
          state.recv_size = data_low;

          fmt::print("[INF] Receiving {:#x} words of data\n", data_low);

        } else {
          fmt::print(fmt::fg(fmt::terminal_color::red), "[ERR] Received unknown data packet, aborting...\n");
          break;
        }
      }
    }
  } while (state.receiving || state.ready || state.setup || state.handshake || state.sending || state.debug ||
           state.waiting);

  fmt::print("[INF] Connection ended\n");

  fmt::print("[INF] Closing port\n");
  port.Close();

  if (sendf.is_open()) {
    sendf.close();
    fmt::print("[INF] Closed {}\n", args.receive_file);
  }

  if (recvf.is_open()) {
    recvf.close();
    fmt::print("[INF] Closed {}\n", args.send_file);
  }

  return 0;
}

void send_word(ls::SerialPort& port, uint8_t data_high, uint8_t data_low) {
  port.WriteByte((uint8_t)data_high);
  port.WriteByte((uint8_t)data_low);

  fmt::print(fmt::fg(fmt::terminal_color::blue), "[OUT] {:#x} {:#x}\n", data_high, data_low);
}

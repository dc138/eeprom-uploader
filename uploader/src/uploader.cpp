#include <libserial/SerialPort.h>
namespace ls = LibSerial;

#include <iostream>
#include <stypox/argparser.hpp>
namespace sp = stypox;

constexpr uint8_t version = 0x02;

typedef struct state_flags_t {
  bool receiving = false;
  bool high      = false;
  bool low       = false;
  bool ready     = false;
  bool setup     = true;
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
                      sp::SwitchOption {"low", args.high, sp::args("-l", "--low"), "Use low mode"},
                      sp::Option {"port", args.port, sp::args("-p", "--port"), "Port to use", true},
                      sp::Option {"rfile", args.port, sp::args("-r", "--receive"), "File to receive into"},
                      sp::Option {"sfile", args.port, sp::args("-s", "--send"), "File to send"}),
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

  std::cout << "[INFO] Using version " << std::hex << (int)version << "\n";

  ls::SerialPort port(args.port);

  port.SetBaudRate(ls::BaudRate::BAUD_9600);
  port.SetCharacterSize(ls::CharacterSize::CHAR_SIZE_8);
  port.SetFlowControl(ls::FlowControl::FLOW_CONTROL_NONE);
  port.SetParity(ls::Parity::PARITY_NONE);

  std::cout << "[INFO] Port opened\n";

  do {
    if (port.GetNumberOfBytesAvailable() > 1) {
      uint8_t data_high = 0x00;
      uint8_t data_low  = 0x00;

      port.ReadByte(data_high);
      port.ReadByte(data_low);

      if (state.receiving) {
        //
      } else {
        if (data_high == 0x01) {
          std::cout << "[INFO] Controller is ready\n";

          if (data_low != version) {
            std::cout << "[ERROR] We are on version " << std::hex << (int)version << " but controller is using version "
                      << (int)data_low << ", aborting...\n";
            exit(2);
          }

          std::cout << "[INFO] We are ready\n";
          state.ready = true;
          state.setup = false;
        } else {
          std::cout << "[ERROR] Received unknown data packet: " << std::hex << (int)data_high << " " << (int)data_low
                    << "\n";
          exit(3);
        }
      }
    }
  } while (state.receiving || state.ready || state.setup);

  std::cout << "[INFO] Connection ended\n";

  port.Close();
  std::cout << "[INFO] Closed port";

  return 0;
}

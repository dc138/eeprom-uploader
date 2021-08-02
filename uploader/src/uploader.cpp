#include <libserialport.h>

#include <iostream>
#include <stypox/argparser.hpp>
namespace sp = stypox;

typedef struct state_flags_t {
  bool receiving = false;
  bool sending   = false;
  bool high      = false;
  bool low       = false;
} state_flags_t;

typedef struct args_t {
  std::string port         = "";
  std::string send_file    = "";
  std::string receive_file = "";
  bool        help         = false;
  bool        high         = false;
  bool        low          = false;
} args_t;

state_flags_t state_flags;
args_t        args;

void check(sp_return res) {
  if (res != sp_return::SP_OK) {
    std::cout << "Error: " << res << "\n";
    exit(1);
  }
}
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
    return 1;
  }

  if (args.help) {
    std::cout << parser.help();
    return 0;
  }

  try {
    parser.validate();
  } catch (std::runtime_error& err) {
    std::cout << err.what() << std::endl;
    return 1;
  }

  if (args.port.starts_with('=')) {
    args.port.erase(args.port.begin());
  }

  struct sp_port* port;

  std::cout << "Looking for port " << args.port << "\n";
  check(sp_get_port_by_name(args.port.data(), &port));

  std::cout << "Opening port\n";
  check(sp_open(port, SP_MODE_READ_WRITE));

  return 0;
}

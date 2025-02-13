#include <iostream>
#include <string>
#include <vector>

// Helper function to check if a command is a builtin
bool is_builtin(const std::string& cmd) {
  const std::vector<std::string> builtins = {"echo", "exit", "type"};
  for (const auto& builtin : builtins) {
    if (cmd == builtin) {
      return true;
    }
  }
  return false;
}

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  std::string input;
  
  // Main REPL loop
  while (true) {
    std::cout << "$ ";  // Print prompt
    
    // Read input
    if (!std::getline(std::cin, input)) {
      // Exit if we hit EOF (Ctrl+D) or encounter an error
      break;
    }
    
    // Check for exit command
    if (input == "exit 0") {
      return 0;  // Exit with status code 0
    }
    
    // Check for echo command
    if (input.substr(0, 5) == "echo ") {
      // Print everything after "echo "
      std::cout << input.substr(5) << std::endl;
      continue;
    }
    
    // Check for type command
    if (input.substr(0, 5) == "type ") {
      std::string cmd = input.substr(5);  // Get the command to check
      if (is_builtin(cmd)) {
        std::cout << cmd << " is a shell builtin" << std::endl;
      } else {
        std::cout << cmd << ": not found" << std::endl;
      }
      continue;
    }
    
    // For now, all other commands are invalid
    std::cout << input << ": command not found" << std::endl;
  }

  return 0;
}

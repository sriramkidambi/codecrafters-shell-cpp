#include <iostream>
#include <string>

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
    
    // For now, all other commands are invalid
    std::cout << input << ": command not found" << std::endl;
  }

  return 0;
}

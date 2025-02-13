#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <filesystem>
#include <sstream>

namespace fs = std::filesystem;

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

// Helper function to split PATH into directories
std::vector<std::string> get_path_dirs() {
  std::vector<std::string> dirs;
  const char* path = std::getenv("PATH");
  if (!path) return dirs;
  
  std::string path_str(path);
  std::stringstream ss(path_str);
  std::string dir;
  
  while (std::getline(ss, dir, ':')) {
    if (!dir.empty()) {
      dirs.push_back(dir);
    }
  }
  return dirs;
}

// Helper function to find executable in PATH
std::string find_in_path(const std::string& cmd) {
  auto dirs = get_path_dirs();
  for (const auto& dir : dirs) {
    fs::path cmd_path = fs::path(dir) / cmd;
    if (fs::exists(cmd_path) && fs::is_regular_file(cmd_path)) {
      return cmd_path.string();
    }
  }
  return "";
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
      
      // First check if it's a builtin
      if (is_builtin(cmd)) {
        std::cout << cmd << " is a shell builtin" << std::endl;
      } else {
        // If not a builtin, search in PATH
        std::string cmd_path = find_in_path(cmd);
        if (!cmd_path.empty()) {
          std::cout << cmd << " is " << cmd_path << std::endl;
        } else {
          std::cout << cmd << ": not found" << std::endl;
        }
      }
      continue;
    }
    
    // For now, all other commands are invalid
    std::cout << input << ": command not found" << std::endl;
  }

  return 0;
}

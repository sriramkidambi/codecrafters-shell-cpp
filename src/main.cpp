#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <unistd.h>
#include <sys/wait.h>
#include <cstring>
#include <cerrno>

namespace fs = std::filesystem;

// Helper function to check if a command is a builtin
bool is_builtin(const std::string& cmd) {
  const std::vector<std::string> builtins = {"echo", "exit", "type", "pwd", "cd"};
  for (const auto& builtin : builtins) {
    if (cmd == builtin) {
      return true;
    }
  }
  return false;
}

// Helper function to get home directory
std::string get_home_directory() {
  const char* home = std::getenv("HOME");
  if (!home) {
    return "";
  }
  return std::string(home);
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

// Helper function to split input into tokens
std::vector<std::string> split_input(const std::string& input) {
    std::vector<std::string> tokens;
    size_t i = 0;
    while (i < input.size()) {
        // Skip any leading whitespace
        while (i < input.size() && std::isspace(input[i])) {
            ++i;
        }
        if (i >= input.size())
            break;

        std::string token;
        // Process a token until we hit unescaped whitespace
        while (i < input.size()) {
            if (input[i] == '\\' && i + 1 < input.size()) {
                // Handle backslash escaping outside quotes
                token.push_back(input[i + 1]);
                i += 2;
                continue;
            }
            
            if (input[i] == '\'') {
                // Single quotes - preserve everything literally
                ++i;  // Skip the opening quote
                while (i < input.size() && input[i] != '\'') {
                    token.push_back(input[i]);
                    ++i;
                }
                if (i < input.size()) ++i;  // Skip the closing quote
            } else if (input[i] == '"') {
                // Double quotes - handle backslash escapes
                ++i;  // Skip the opening quote
                while (i < input.size() && input[i] != '"') {
                    if (input[i] == '\\' && i + 1 < input.size()) {
                        char next = input[i + 1];
                        if (next == '\\' || next == '$' || next == '"' || next == '\n') {
                            token.push_back(next);
                            i += 2;
                            continue;
                        }
                    }
                    token.push_back(input[i]);
                    ++i;
                }
                if (i < input.size()) ++i;  // Skip the closing quote
            } else if (std::isspace(input[i])) {
                break;  // End of token
            } else {
                token.push_back(input[i]);
                ++i;
            }
        }
        if (!token.empty()) {
            tokens.push_back(token);
        }
    }
    return tokens;
}

// Helper function to execute external program
void execute_program(const std::string& program_path, const std::vector<std::string>& args) {
  pid_t pid = fork();
  if (pid == 0) {  // Child process
    // Prepare arguments for execv
    std::vector<char*> c_args;
    c_args.push_back(const_cast<char*>(args[0].c_str()));  // Program name
    for (size_t i = 1; i < args.size(); i++) {
      c_args.push_back(const_cast<char*>(args[i].c_str()));
    }
    c_args.push_back(nullptr);  // Null terminator

    execv(program_path.c_str(), c_args.data());
    // If execv returns, there was an error
    std::cerr << "Error executing program" << std::endl;
    exit(1);
  } else if (pid > 0) {  // Parent process
    int status;
    waitpid(pid, &status, 0);
  }
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
    
    // Skip empty input
    if (input.empty()) {
      continue;
    }

    // Split input into tokens
    auto tokens = split_input(input);
    if (tokens.empty()) continue;

    std::string cmd = tokens[0];
    
    // Check for exit command
    if (cmd == "exit" && tokens.size() == 2 && tokens[1] == "0") {
      return 0;  // Exit with status code 0
    }
    
    // Check for echo command
    if (cmd == "echo") {
      // Skip the echo command itself and print remaining tokens with spaces
      for (size_t i = 1; i < tokens.size(); ++i) {
        if (i > 1) std::cout << " ";
        std::cout << tokens[i];
      }
      std::cout << std::endl;
      continue;
    }

    // Check for pwd command
    if (cmd == "pwd") {
      std::cout << fs::current_path().string() << std::endl;
      continue;
    }

    // Check for cd command
    if (cmd == "cd") {
      if (tokens.size() != 2) {
        std::cerr << "cd: wrong number of arguments" << std::endl;
        continue;
      }

      std::string path = tokens[1];
      
      // Handle ~ for home directory
      if (path == "~") {
        std::string home = get_home_directory();
        if (home.empty()) {
          std::cerr << "cd: HOME not set" << std::endl;
          continue;
        }
        path = home;
      }

      if (chdir(path.c_str()) != 0) {
        std::cerr << "cd: " << path << ": No such file or directory" << std::endl;
      }
      continue;
    }
    
    // Check for type command
    if (cmd == "type" && tokens.size() == 2) {
      std::string target = tokens[1];
      
      // First check if it's a builtin
      if (is_builtin(target)) {
        std::cout << target << " is a shell builtin" << std::endl;
      } else {
        // If not a builtin, search in PATH
        std::string cmd_path = find_in_path(target);
        if (!cmd_path.empty()) {
          std::cout << target << " is " << cmd_path << std::endl;
        } else {
          std::cout << target << ": not found" << std::endl;
        }
      }
      continue;
    }
    
    // Try to execute as external program
    std::string program_path = find_in_path(cmd);
    if (!program_path.empty()) {
      execute_program(program_path, tokens);
    } else {
      std::cout << cmd << ": command not found" << std::endl;
    }
  }

  return 0;
}

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
#include <fcntl.h>

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

// Structure to hold command and redirection information
struct Command {
    std::vector<std::string> args;
    std::string stdout_file;
    std::string stderr_file;
    bool has_stdout_redirection = false;
    bool has_stderr_redirection = false;
};

// Helper function to parse command and redirection
Command parse_command(const std::vector<std::string>& tokens) {
    Command cmd;
    
    for (size_t i = 0; i < tokens.size(); ++i) {
        if ((tokens[i] == ">" || tokens[i] == "1>") && i + 1 < tokens.size()) {
            cmd.has_stdout_redirection = true;
            cmd.stdout_file = tokens[i + 1];
            i++; // Skip the file name
        } else if (tokens[i] == "2>" && i + 1 < tokens.size()) {
            cmd.has_stderr_redirection = true;
            cmd.stderr_file = tokens[i + 1];
            i++; // Skip the file name
        } else {
            cmd.args.push_back(tokens[i]);
        }
    }
    
    return cmd;
}

// Helper function to execute external program with possible redirection
void execute_program(const std::string& program_path, const Command& cmd) {
    pid_t pid = fork();
    if (pid == 0) {  // Child process
        // Handle stdout redirection
        if (cmd.has_stdout_redirection) {
            int fd = open(cmd.stdout_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd == -1) {
                std::cerr << "Error opening output file" << std::endl;
                exit(1);
            }
            if (dup2(fd, STDOUT_FILENO) == -1) {
                std::cerr << "Error redirecting output" << std::endl;
                exit(1);
            }
            close(fd);
        }

        // Handle stderr redirection
        if (cmd.has_stderr_redirection) {
            int fd = open(cmd.stderr_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd == -1) {
                std::cerr << "Error opening error output file" << std::endl;
                exit(1);
            }
            if (dup2(fd, STDERR_FILENO) == -1) {
                std::cerr << "Error redirecting error output" << std::endl;
                exit(1);
            }
            close(fd);
        }

        // Prepare arguments for execv
        std::vector<char*> c_args;
        c_args.push_back(const_cast<char*>(cmd.args[0].c_str()));  // Program name
        for (size_t i = 1; i < cmd.args.size(); i++) {
            c_args.push_back(const_cast<char*>(cmd.args[i].c_str()));
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

// Helper function to write output with redirection
void write_output(const std::string& output, const Command& cmd, bool is_error = false) {
    if ((is_error && cmd.has_stderr_redirection) || (!is_error && cmd.has_stdout_redirection)) {
        const std::string& file = is_error ? cmd.stderr_file : cmd.stdout_file;
        int fd = open(file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd != -1) {
            write(fd, output.c_str(), output.length());
            close(fd);
        }
    } else {
        if (is_error) {
            std::cerr << output;
        } else {
            std::cout << output;
        }
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

    // Parse command and redirection
    Command cmd = parse_command(tokens);
    if (cmd.args.empty()) continue;

    std::string command = cmd.args[0];
    
    // Check for exit command
    if (command == "exit" && cmd.args.size() == 2 && cmd.args[1] == "0") {
      return 0;  // Exit with status code 0
    }
    
    // Check for echo command
    if (command == "echo") {
      std::string output;
      for (size_t i = 1; i < cmd.args.size(); ++i) {
        if (i > 1) output += " ";
        output += cmd.args[i];
      }
      output += "\n";
      write_output(output, cmd);
      continue;
    }

    // Check for pwd command
    if (command == "pwd") {
      std::string output = fs::current_path().string() + "\n";
      write_output(output, cmd);
      continue;
    }

    // Check for cd command
    if (command == "cd") {
      if (cmd.args.size() != 2) {
        std::string error = "cd: wrong number of arguments\n";
        write_output(error, cmd, true);
        continue;
      }

      std::string path = cmd.args[1];
      
      // Handle ~ for home directory
      if (path == "~") {
        std::string home = get_home_directory();
        if (home.empty()) {
          std::string error = "cd: HOME not set\n";
          write_output(error, cmd, true);
          continue;
        }
        path = home;
      }

      if (chdir(path.c_str()) != 0) {
        std::string error = "cd: " + path + ": No such file or directory\n";
        write_output(error, cmd, true);
      }
      continue;
    }
    
    // Check for type command
    if (command == "type" && cmd.args.size() == 2) {
      std::string target = cmd.args[1];
      
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
    std::string program_path = find_in_path(command);
    if (!program_path.empty()) {
      execute_program(program_path, cmd);
    } else {
      std::cout << command << ": command not found" << std::endl;
    }
  }

  return 0;
}

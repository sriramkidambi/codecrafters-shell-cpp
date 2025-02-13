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
#include <termios.h>
#include <optional>
#include <numeric>
#include <fstream>
#include <set>

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
    bool append_stdout = false;
    bool append_stderr = false;
};

// Helper function to parse command and redirection
Command parse_command(const std::vector<std::string>& tokens) {
    Command cmd;
    
    for (size_t i = 0; i < tokens.size(); ++i) {
        if ((tokens[i] == ">" || tokens[i] == "1>") && i + 1 < tokens.size()) {
            cmd.has_stdout_redirection = true;
            cmd.stdout_file = tokens[i + 1];
            cmd.append_stdout = false;
            i++; // Skip the file name
        } else if ((tokens[i] == ">>" || tokens[i] == "1>>") && i + 1 < tokens.size()) {
            cmd.has_stdout_redirection = true;
            cmd.stdout_file = tokens[i + 1];
            cmd.append_stdout = true;
            i++; // Skip the file name
        } else if (tokens[i] == "2>" && i + 1 < tokens.size()) {
            cmd.has_stderr_redirection = true;
            cmd.stderr_file = tokens[i + 1];
            cmd.append_stderr = false;
            i++; // Skip the file name
        } else if (tokens[i] == "2>>" && i + 1 < tokens.size()) {
            cmd.has_stderr_redirection = true;
            cmd.stderr_file = tokens[i + 1];
            cmd.append_stderr = true;
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
            int flags = O_WRONLY | O_CREAT | (cmd.append_stdout ? O_APPEND : O_TRUNC);
            int fd = open(cmd.stdout_file.c_str(), flags, 0644);
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
            int flags = O_WRONLY | O_CREAT | (cmd.append_stderr ? O_APPEND : O_TRUNC);
            int fd = open(cmd.stderr_file.c_str(), flags, 0644);
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

// Replace autocomplete function with this
std::vector<std::string> get_possible_completions(const std::string& input) {
    std::vector<std::string> completions;

    // Check built-in commands
    const std::vector<std::string> builtins = {"echo", "exit", "type", "pwd", "cd"};
    for (const auto& builtin : builtins) {
        if (builtin.find(input) == 0) {
            completions.push_back(builtin);
        }
    }

    // Check external executables in PATH
    std::set<std::string> exe_matches;
    auto dirs = get_path_dirs();
    for (const auto& dir : dirs) {
        if (!fs::is_directory(dir)) continue;
        try {
            for (const auto& entry : fs::directory_iterator(dir)) {
                if (entry.is_regular_file() && (::access(entry.path().c_str(), X_OK) == 0)) {
                    std::string filename = entry.path().filename().string();
                    if (filename.find(input) == 0) {
                        exe_matches.insert(filename);
                    }
                }
            }
        } catch (...) {
            // Ignore inaccessible directories
        }
    }

    // Add external executables to completions
    for (const auto& exe : exe_matches) {
        completions.push_back(exe);
    }

    return completions;
}

void enableRawMode() {
    termios term;
    tcgetattr(STDIN_FILENO, &term);
    term.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &term);
}

void disableRawMode() {
    termios term;
    tcgetattr(STDIN_FILENO, &term);
    term.c_lflag |= (ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &term);
}

// Update the input handling to support multi-completion
void readInputWithTabSupport(std::string& input) {
    enableRawMode();
    char c;
    input.clear();
    
    // Track tab completion state
    bool pending_completion = false;
    std::vector<std::string> current_completions;

    while (true) {
        c = getchar();
        if (c == '\n') {
            std::cout << std::endl;
            break;
        } else if (c == '\t') {
            if (input.find(' ') == std::string::npos) { // Only autocomplete command if no spaces
                std::string original = input;
                auto completions = get_possible_completions(original);
                
                if (completions.size() == 1) {
                    input = completions[0] + " ";
                    std::cout << "\r$ " << input << std::flush;
                    pending_completion = false;
                    current_completions.clear();
                } else if (completions.size() > 1) {
                    if (pending_completion) {
                        // Display all matches on second tab
                        std::cout << "\n";
                        for (size_t i = 0; i < completions.size(); ++i) {
                            if (i > 0) std::cout << "  ";
                            std::cout << completions[i];
                        }
                        std::cout << "\n$ " << input << std::flush;
                        pending_completion = false;
                        current_completions.clear();
                    } else {
                        // Store matches and ring bell on first tab
                        current_completions = completions;
                        pending_completion = true;
                        std::cout << '\a' << std::flush;
                    }
                } else {
                    // No matches found
                    std::cout << '\a' << std::flush;
                    pending_completion = false;
                    current_completions.clear();
                }
            }
        } else if (c == 127) { // Backspace
            if (!input.empty()) {
                input.pop_back();
                std::cout << "\b \b";
            }
            pending_completion = false;
            current_completions.clear();
        } else if (c >= 32 && c < 127) { // Printable characters
            input += c;
            std::cout << c;
            pending_completion = false;
            current_completions.clear();
        }
    }
    
    disableRawMode();
}

int main() {
    // Flush after every std::cout / std:cerr
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    while (true) {
        // Save original file descriptors
        int original_stdout = dup(STDOUT_FILENO);
        int original_stderr = dup(STDERR_FILENO);

        std::cout << "$ ";
        std::string input;
        readInputWithTabSupport(input);

        // Exit if we hit EOF (Ctrl+D) or encounter an error
        if (std::cin.eof()) {
            close(original_stdout);
            close(original_stderr);
            break;
        }
        
        // Skip empty input
        if (input.empty()) {
            close(original_stdout);
            close(original_stderr);
            continue;
        }

        // Split input into tokens
        auto tokens = split_input(input);
        if (tokens.empty()) {
            close(original_stdout);
            close(original_stderr);
            continue;
        }

        // Parse command and redirection
        Command cmd = parse_command(tokens);
        if (cmd.args.empty()) {
            close(original_stdout);
            close(original_stderr);
            continue;
        }

        std::string command = cmd.args[0];
        
        // Handle redirections for builtin commands
        if (cmd.has_stdout_redirection) {
            int flags = O_WRONLY | O_CREAT | (cmd.append_stdout ? O_APPEND : O_TRUNC);
            int fd = open(cmd.stdout_file.c_str(), flags, 0644);
            if (fd != -1) {
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }
        }
        if (cmd.has_stderr_redirection) {
            int flags = O_WRONLY | O_CREAT | (cmd.append_stderr ? O_APPEND : O_TRUNC);
            int fd = open(cmd.stderr_file.c_str(), flags, 0644);
            if (fd != -1) {
                dup2(fd, STDERR_FILENO);
                close(fd);
            }
        }
        
        // Check for exit command
        if (command == "exit" && cmd.args.size() == 2 && cmd.args[1] == "0") {
            close(original_stdout);
            close(original_stderr);
            return 0;  // Exit with status code 0
        }
        
        // Check for echo command
        if (command == "echo") {
            for (size_t i = 1; i < cmd.args.size(); ++i) {
                if (i > 1) std::cout << " ";
                std::cout << cmd.args[i];
            }
            std::cout << std::endl;
        }
        // Check for pwd command
        else if (command == "pwd") {
            std::cout << fs::current_path().string() << std::endl;
        }
        // Check for cd command
        else if (command == "cd") {
            if (cmd.args.size() != 2) {
                std::cerr << "cd: wrong number of arguments" << std::endl;
            } else {
                std::string path = cmd.args[1];
                
                // Handle ~ for home directory
                if (path == "~") {
                    std::string home = get_home_directory();
                    if (home.empty()) {
                        std::cerr << "cd: HOME not set" << std::endl;
                    } else {
                        path = home;
                    }
                }

                if (chdir(path.c_str()) != 0) {
                    std::cerr << "cd: " << path << ": No such file or directory" << std::endl;
                }
            }
        }
        // Check for type command
        else if (command == "type" && cmd.args.size() == 2) {
            std::string target = cmd.args[1];
            
            if (is_builtin(target)) {
                std::cout << target << " is a shell builtin" << std::endl;
            } else {
                std::string cmd_path = find_in_path(target);
                if (!cmd_path.empty()) {
                    std::cout << target << " is " << cmd_path << std::endl;
                } else {
                    std::cout << target << ": not found" << std::endl;
                }
            }
        }
        // Try to execute as external program
        else {
            std::string program_path = find_in_path(command);
            if (!program_path.empty()) {
                execute_program(program_path, cmd);
            } else {
                std::cout << command << ": command not found" << std::endl;
            }
        }

        // Restore original file descriptors
        dup2(original_stdout, STDOUT_FILENO);
        dup2(original_stderr, STDERR_FILENO);
        close(original_stdout);
        close(original_stderr);
    }

    return 0;
}

#include <iostream>
#include <string>
#include <unordered_set>

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  std::unordered_set<std::string> builtins;
  builtins.insert("echo");
  builtins.insert("type");
  builtins.insert("exit");
  // TODO: Uncomment the code below to pass the first stage
  while (true) {
    std::cout << "$ ";
    std::string input;
    std::getline(std::cin, input);
    if (input == "exit") {
      break;
    }

    size_t space_pos = input.find(' ');
    std::string first_word = input.substr(0, space_pos);

    if (first_word == "echo") {
      std::cout << input.substr(space_pos + 1) << std::endl;
    } else if (first_word == "type") {
      if (builtins.count(input.substr(5))) {
        std::cout << input.substr(5) << " is a shell builtin" << std::endl;
      } else {
        std::cout << input.substr(5) << ": not found" << std::endl;
      }
    } else {
      std::cout << input << ": command not found" << std::endl;
    }
  }

  return 0;
}

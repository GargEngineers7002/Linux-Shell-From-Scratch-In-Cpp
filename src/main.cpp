#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>
#include <unordered_set>
#include <vector>

std::unordered_set<std::string> builtins = {"echo", "type", "exit"};

std::string check_PATH(std::string command)
{
  const char *path_env = std::getenv("PATH");
  if (!path_env)
    return "";

  std::string s(path_env);
  std::stringstream ss(s);
  std::string segment;

  while (std::getline(ss, segment, ':'))
  {
    std::string full_path = segment + '/' + command;
    if (access(full_path.c_str(), X_OK) == 0)
    {
      return full_path;
    }
  }
  return "";
}

int main()
{
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  // TODO: Uncomment the code below to pass the first stage
  while (true)
  {
    std::cout << "$ ";
    std::string input;
    std::getline(std::cin, input);
    if (input == "exit")
    {
      break;
    }

    std::stringstream ss(input);
    std::string command;
    ss >> command;

    std::string argument;
    std::getline(ss, argument);

    if (!argument.empty() && argument[0] == ' ')
    {
      argument = argument.substr(1);
    }

    if (command == "echo")
    {
      std::cout << argument << std::endl;
    }
    else if (command == "type")
    {
      if (builtins.count(argument))
      {
        std::cout << argument << " is a shell builtin" << std::endl;
      }
      else
      {
        std::string path = check_PATH(argument);
        if (!path.empty())
        {
          std::cout << argument << " is " << path << std::endl;
        }
        else
        {
          std::cout << argument << ": not found" << std::endl;
        }
      }
    }
    else
    {
      std::cout << input << ": command not found" << std::endl;
    }
  }

  return 0;
}

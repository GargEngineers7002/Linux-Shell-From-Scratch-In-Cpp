#include <cstdlib>
#include <iostream>
#include <limits.h>
#include <sstream>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <unordered_set>
#include <vector>

std::unordered_set<std::string> builtins = {"echo", "type", "exit", "pwd",
                                            "cd"};

// PARSE INPUT
std::vector<std::string> parse_input(const std::string &input)
{
  std::vector<std::string> tokens;
  std::string current_token;
  bool in_sq = false; // single quotes
  bool in_dq = false; // double quotes
  bool backslash = false;

  for (char c : input)
  {
    if (backslash)
    {
      if (in_sq)
      {
        current_token += '\\';
        current_token += c;
      }
      else if (in_dq)
      {
        if (c != '\"' && c != '\\')
        {
          current_token += '\\';
        }
        current_token += c;
      }
      else
      {
        current_token += c;
      }
      backslash = false;
    }
    else if (c == '\\')
    {
      backslash = true;
    }
    else if (c == ' ' && !in_sq && !in_dq)
    {
      if (!current_token.empty())
      {
        tokens.push_back(current_token);
        current_token.clear();
      }
    }
    else if (c == '\'' && !in_dq)
    {
      in_sq = !in_sq;
    }
    else if (c == '\"' && !in_sq)
    {
      in_dq = !in_dq;
    }
    else
    {
      current_token += c;
    }
  }

  if (!current_token.empty())
  {
    tokens.push_back(current_token);
  }

  return tokens;
}

// CHECK IF COMMAND IS IN PATH
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

    std::vector<std::string> args = parse_input(input);

    std::string command = args[0];
    std::string arguments;
    for (size_t i = 1; i < args.size(); i++)
    {
      arguments += args[i];
      if (i != args.size() - 1)
      {
        arguments += " ";
      }
    }

    if (command == "echo")
    {
      std::cout << arguments << std::endl;
    }
    else if (command == "pwd")
    {
      char cwd[PATH_MAX];
      getcwd(cwd, PATH_MAX);
      std::cout << cwd << std::endl;
    }
    else if (command == "cd")
    {
      if (args.size() == 1 || args[1] == "~")
      {
        chdir(std::getenv("HOME"));
      }
      else
      {
        if (chdir(arguments.c_str()) == -1)
        {
          std::cout << "cd: " << arguments << ": No such file or directory"
                    << std::endl;
        }
      }
    }
    else if (command == "type")
    {
      if (builtins.count(args[1]))
      {
        std::cout << args[1] << " is a shell builtin" << std::endl;
      }
      else
      {
        std::string path = check_PATH(args[1]);
        if (!path.empty())
        {
          std::cout << args[1] << " is " << path << std::endl;
        }
        else
        {
          std::cout << args[1] << ": not found" << std::endl;
        }
      }
    }
    // EXECUTE COMMANDS LIKE UNIX SHELL AND CAT
    else if (!check_PATH(command).empty())
    {
      std::vector<char *> c_args;
      for (auto &arg : args)
      {
        c_args.push_back(&arg[0]);
      }
      c_args.push_back(nullptr);

      pid_t pid = fork();
      if (pid == 0)
      {
        execvp(command.c_str(), c_args.data());
        exit(1);
      }
      else if (pid > 0)
      {
        int status;
        waitpid(pid, &status, 0);
      }
      else
      {
        std::cerr << "Fork Failed" << std::endl;
      }
    }
    else
    {
      std::cerr << input << ": command not found" << std::endl;
    }
  }

  return 0;
}

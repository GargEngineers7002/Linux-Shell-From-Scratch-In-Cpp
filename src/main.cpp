#include <cstdlib>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <limits.h>
#include <sstream>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <unordered_set>
#include <vector>

#include <readline/history.h>
#include <readline/readline.h>

std::vector<std::string> builtins = {"echo", "type", "exit", "pwd", "cd"};

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

// COMPLETION
char *command_generator(const char *text, int state)
{
  static size_t list_index, len;

  if (!state)
  {
    list_index = 0;
    len = strlen(text);
  }

  while (list_index < builtins.size())
  {
    const std::string &name = builtins[list_index];
    list_index++;

    if (name.compare(0, len, text) == 0)
    {
      return strdup(name.c_str());
    }
  }

  return nullptr;
}
char **custom_completion(const char *text, int start, int end)
{
  if (start == 0)
  {
    return rl_completion_matches(text, command_generator);
  }

  return nullptr;
}
int main()
{
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  std::unordered_set<std::string> builtins_set(builtins.begin(),
                                               builtins.end());

  rl_attempted_completion_function = custom_completion;

  // TODO: Uncomment the code below to pass the first stage
  while (true)
  {
    char *input_c = readline("$ ");
    if (!input_c)
    {
      break;
    }

    std::string input(input_c);
    if (!input.empty())
    {
      add_history(input_c);
    }
    else
    {
      continue;
    }

    free(input_c);

    if (input == "exit")
    {
      break;
    }

    std::vector<std::string> args = parse_input(input);
    if (args.size() == 0)
    {
      continue;
    }

    // REDIRECTION LOGIC
    std::string output_file;
    bool redirect_stdout = false;
    bool append_redirect_stdout = false;
    bool redirect_stderr = false;
    bool append_redirect_stderr = false;

    for (int i = 0; i < args.size(); i++)
    {
      if (args[i] == ">>" || args[i] == "1>>")
      {
        if (i + 1 < args.size())
        {
          output_file = args[i + 1];
          append_redirect_stdout = true;
          args.erase(args.begin() + i, args.begin() + i + 2);
          break;
        }
      }
      else if (args[i] == "2>>")
      {
        if (i + 1 < args.size())
        {
          output_file = args[i + 1];
          append_redirect_stderr = true;
          args.erase(args.begin() + i, args.begin() + i + 2);
          break;
        }
      }
      else if (args[i] == ">" || args[i] == "1>")
      {
        if (i + 1 < args.size())
        {
          output_file = args[i + 1];
          redirect_stdout = true;
          args.erase(args.begin() + i, args.begin() + i + 2);
          break;
        }
      }
      else if (args[i] == "2>")
      {
        if (i + 1 < args.size())
        {
          output_file = args[i + 1];
          redirect_stderr = true;
          args.erase(args.begin() + i, args.begin() + i + 2);
          break;
        }
      }
    }

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
      std::streambuf *original_cout = nullptr;
      std::streambuf *original_cerr = nullptr;
      std::ofstream out_file_stream;
      std::ofstream err_file_stream;

      if (redirect_stdout || append_redirect_stdout)
      {
        auto mode = append_redirect_stdout ? std::ios::app : std::ios::trunc;
        out_file_stream.open(output_file, mode);
        original_cout = std::cout.rdbuf();
        std::cout.rdbuf(out_file_stream.rdbuf());
      }

      if (redirect_stderr || append_redirect_stderr)
      {
        auto mode = append_redirect_stderr ? std::ios::app : std::ios::trunc;
        err_file_stream.open(output_file, mode);
        original_cerr = std::cerr.rdbuf();
        std::cerr.rdbuf(err_file_stream.rdbuf());
      }

      std::cout << arguments << std::endl;

      if (redirect_stdout || append_redirect_stdout)
      {
        std::cout.rdbuf(original_cout);
      }

      if (redirect_stderr || append_redirect_stderr)
      {
        std::cerr.rdbuf(original_cerr);
      }
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
      if (builtins_set.count(args[1]))
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
        bool is_stdout = redirect_stdout || append_redirect_stdout;
        bool is_stderr = redirect_stderr || append_redirect_stderr;

        if (is_stdout || is_stderr)
        {
          int fd;
          if (append_redirect_stdout || append_redirect_stderr)
          {
            fd = open(output_file.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
          }
          else
          {
            fd = open(output_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
          }
          if (fd < 0)
          {
            perror("open");
            exit(1);
          }
          if (is_stdout)
          {
            dup2(fd, STDOUT_FILENO);
          }
          if (is_stderr)
          {
            dup2(fd, STDERR_FILENO);
          }
          close(fd);
        }
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

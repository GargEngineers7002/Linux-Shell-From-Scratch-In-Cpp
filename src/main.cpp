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

#include <dirent.h>
#include <set>

std::vector<std::string> builtins = {"echo", "type", "exit",
                                     "pwd",  "cd",   "history"};

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
  static std::vector<std::string> matches;
  static size_t match_index;

  if (state == 0)
  {
    matches.clear();
    match_index = 0;

    std::string text_str(text);

    std::set<std::string> match_set;

    // SEARCH BUILTINS
    for (const auto &cmd : builtins)
    {
      if (cmd.compare(0, text_str.size(), text_str) == 0)
      {
        match_set.insert(cmd);
      }
    }

    // SEARCH IN PATH
    const char *path_env = std::getenv("PATH");
    if (path_env)
    {
      std::string path_str(path_env);
      std::stringstream ss(path_str);
      std::string dir_path;

      while (getline(ss, dir_path, ':'))
      {
        DIR *dir = opendir(dir_path.c_str());
        if (!dir)
          continue;

        struct dirent *entry;
        while ((entry = readdir(dir)) != nullptr)
        {
          std::string filename = entry->d_name;
          if (filename == "." || filename == "..")
            continue;

          if (filename.compare(0, text_str.size(), text_str) == 0)
          {
            std::string full_path = dir_path + '/' + filename;
            if (access(full_path.c_str(), X_OK) == 0)
            {
              match_set.insert(filename);
            }
          }
        }
        closedir(dir);
      }
    }
    for (const auto &m : match_set)
    {
      matches.push_back(m);
    }
  }

  if (match_index < matches.size())
  {
    return strdup(matches[match_index++].c_str());
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

// PIPE LOGIC
std::vector<std::vector<std::string>>
parse_pipeline(const std::vector<std::string> &args)
{
  std::vector<std::vector<std::string>> commands;
  std::vector<std::string> current_command;

  for (const auto &arg : args)
  {
    if (arg == "|")
    {
      if (!current_command.empty())
      {
        commands.push_back(current_command);
        current_command.clear();
      }
    }
    else
    {
      current_command.push_back(arg);
    }
  }
  if (!current_command.empty())
    commands.push_back(current_command);

  return commands;
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

    if (input_c[0] != '\0')
    {
      add_history(input_c);
    }
    else
    {
      continue;
    }

    std::string input(input_c);

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

    // PIPE LOGIC
    bool has_pipe = false;
    for (const auto &arg : args)
    {
      if (arg == "|")
      {
        has_pipe = true;
        break;
      }
    }

    if (has_pipe)
    {
      std::vector<std::vector<std::string>> commands = parse_pipeline(args);
      int num_cmds = commands.size();

      int prev_pipe_read = -1;

      std::vector<pid_t> pids;

      for (int i = 0; i < num_cmds; i++)
      {
        int pipe_fds[2];

        bool is_last = (i == num_cmds - 1);
        if (!is_last)
        {
          if (pipe(pipe_fds) == -1)
          {
            perror("pipe");
            exit(1);
          }
        }

        pid_t pid = fork();
        if (pid == 0)
        {
          // CHILD PROCESS
          if (prev_pipe_read != -1)
          {
            dup2(prev_pipe_read, STDIN_FILENO);
            close(prev_pipe_read);
          }

          if (!is_last)
          {
            dup2(pipe_fds[1], STDOUT_FILENO);
            close(pipe_fds[1]);
            close(pipe_fds[0]);
          }

          std::vector<std::string> &command = commands[i];
          std::string cmd_name = command[0];

          if (cmd_name == "echo")
          {
            for (size_t j = 1; j < command.size(); j++)
            {
              std::cout << command[j];
              if (j != command.size() - 1)
              {
                std::cout << " ";
              }
            }
            std::cout << std::endl;
            exit(0);
          }
          else if (cmd_name == "pwd")
          {
            char cwd[PATH_MAX];
            getcwd(cwd, PATH_MAX);
            std::cout << cwd << std::endl;
            exit(0);
          }
          else if (cmd_name == "type")
          {
            if (builtins_set.count(command[1]))
            {
              std::cout << command[1] << " is a shell builtin" << std::endl;
            }
            else
            {
              std::string path = check_PATH(command[1]);
              if (!path.empty())
              {
                std::cout << command[1] << " is " << path << std::endl;
              }
              else
              {
                std::cout << command[1] << ": not found" << std::endl;
              }
            }
            exit(0);
          }
          else if (cmd_name == "cd")
          {
            if (command.size() == 1 || command[1] == "~")
            {
              chdir(std::getenv("HOME"));
            }
            else
            {
              if (chdir(command[1].c_str()) == -1)
              {
                std::cout << "cd: " << command[1]
                          << ": No such file or directory" << std::endl;
              }
            }
            exit(0);
          }
          else if (cmd_name == "history")
          {
            HIST_ENTRY **history = history_list();
            int history_size = history_length;
            if (command.size() == 1)
            {
              for (int i = 0; history[i]; i++)
              {
                std::cout << "    " << i + 1 << "  " << history[i]->line
                          << std::endl;
              }
            }
            else
            {
              int num = history_size - std::stoi(command[1]);
              for (int i = num; history[i]; i++)
              {
                if (i >= num)
                {
                  std::cout << "    " << (i + 1) << "  " << history[i]->line
                            << std::endl;
                }
              }
            }
            exit(0);
          }
          else if (cmd_name == "exit")
          {
            exit(0);
          }

          std::vector<char *> c_args;
          for (auto &arg : command)
          {
            c_args.push_back(&arg[0]);
          }
          c_args.push_back(nullptr);

          execvp(cmd_name.c_str(), c_args.data());
          std::cerr << cmd_name << ": command not found" << std::endl;
          exit(1);
        }
        pids.push_back(pid);

        if (prev_pipe_read != -1)
        {
          close(prev_pipe_read);
        }

        if (!is_last)
        {
          prev_pipe_read = pipe_fds[0];
          close(pipe_fds[1]);
        }
      }

      for (auto p : pids)
      {
        waitpid(p, nullptr, 0);
      }
    }
    else
    {
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

      // comamnd and arguments line assignment
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
      else if (command == "history")
      {
        HIST_ENTRY **history = history_list();
        int history_size = history_length;
        if (command.size() == 1)
        {
          for (int i = 0; history[i]; i++)
          {
            std::cout << "    " << i + 1 << "  " << history[i]->line
                      << std::endl;
          }
        }
        else
        {
          int num = history_size - std::stoi(args[1]);
          for (int i = num; history[i]; i++)
          {
            if (i >= num)
            {
              std::cout << "    " << (i + 1) << "  " << history[i]->line
                        << std::endl;
            }
          }
        }
      }
      // EXECUTE EXTERNAL COMMANDS LIKE UNIX SHELL AND CAT
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
              fd = open(output_file.c_str(), O_WRONLY | O_CREAT | O_APPEND,
                        0644);
            }
            else
            {
              fd =
                  open(output_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
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
  }

  return 0;
}

#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
#include "Commands.h"
#include <time.h>
#include <utime.h>


using namespace std;

#if 0
#define FUNC_ENTRY()  \
  cout << __PRETTY_FUNCTION__ << " --> " << endl;

#define FUNC_EXIT()  \
  cout << __PRETTY_FUNCTION__ << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
#endif

const std::string WHITESPACE = " \n\r\t\f\v";

string _ltrim(const std::string& s)
{
  size_t start = s.find_first_not_of(WHITESPACE);
  return (start == std::string::npos) ? "" : s.substr(start);
}

string _rtrim(const std::string& s)
{
  size_t end = s.find_last_not_of(WHITESPACE);
  return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

string _trim(const std::string& s)
{
  return _rtrim(_ltrim(s));
}

int _parseCommandLine(const char* cmd_line, char** args) {
  FUNC_ENTRY()
  int i = 0;
  std::istringstream iss(_trim(string(cmd_line)).c_str());
  for(std::string s; iss >> s; ) {
    args[i] = (char*)malloc(s.length()+1);
    memset(args[i], 0, s.length()+1);
    strcpy(args[i], s.c_str());
    args[++i] = NULL;
  }
  return i;

  FUNC_EXIT()
}

bool _isBackgroundComamnd(const char* cmd_line) {
  const string str(cmd_line);
  return str[str.find_last_not_of(WHITESPACE)] == '&';
}

void _removeBackgroundSign(char* cmd_line) {
  const string str(cmd_line);
  // find last character other than spaces
  unsigned int idx = str.find_last_not_of(WHITESPACE);
  // if all characters are spaces then return
  if (idx == string::npos) {
    return;
  }
  // if the command line does not end with & then return
  if (cmd_line[idx] != '&') {
    return;
  }
  // replace the & (background sign) with space and then remove all tailing spaces.
  cmd_line[idx] = ' ';
  // truncate the command line string up to the last non-space character
  cmd_line[str.find_last_not_of(WHITESPACE, idx) + 1] = 0;
}

// TODO: Add your implementation for classes in Commands.h 

SmallShell::SmallShell() : last_path(NULL), prompt("smash> ") {
// TODO: add your implementation
  // cleanArgsArray();
}

SmallShell::~SmallShell() {
// TODO: add your implementation
  free(last_path);
  // cleanArgsArray();
}

/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
Command * SmallShell::CreateCommand(const char* cmd_line) {
	// For example:
  string cmd_s = _trim(string(cmd_line));
  string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));
  // cleanArgsArray();
  // int num_of_args = _parseCommandLine(cmd_line, args);

  if (firstWord.compare("pwd") == 0) {
    return new GetCurrDirCommand(cmd_line);
  }
  else if (firstWord.compare("chprompt") == 0) {
    return new ChangePromptCommand(cmd_line, &prompt);
  }
  else if (firstWord.compare("showpid") == 0) {
    return new ShowPidCommand(cmd_line);
  }
  else if(firstWord.compare("quit") == 0) {
    return new QuitCommand(cmd_line, nullptr);
  }
  else if (firstWord.compare("cd") == 0) {
    return new ChangeDirCommand(cmd_line, &last_path);
  }
  /*
  else if ...
  .....
  else {
    return new ExternalCommand(cmd_line);
  }
  */
  return nullptr;
}

void SmallShell::executeCommand(const char *cmd_line) {
  // TODO: Add your implementation here
  // for example:
  Command* cmd = CreateCommand(cmd_line);
  if(cmd) {
    cmd->execute();
  }
  // Please note that you must fork smash process for some commands (e.g., external commands....)
  delete cmd;
}

/** COMMAND **/
Command::Command(const char* cmd_line) : cmd_line(cmd_line) {
    cleanArgsArray();
    num_of_args = _parseCommandLine(cmd_line, args);
  };

void Command::cleanArgsArray() {
  for(int i=0; i<COMMAND_MAX_ARGS; ++i) {
    if(!args[i]) {
      free(args[i]);
    }
    args[i] = nullptr;
  }
}

/** CHPROMPT **/
ChangePromptCommand::ChangePromptCommand(const char* cmd_line, std::string* prompt) : BuiltInCommand(cmd_line), prompt(prompt) {}
void ChangePromptCommand::execute() {
  if(num_of_args == 1) {
    *prompt = "smash> ";
  }
  else {
    *prompt = args[1] + string("> ");
  }
}
/** PWD **/
GetCurrDirCommand::GetCurrDirCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {}

void GetCurrDirCommand::execute() {
  char* path = getcwd(NULL, 0);
  if(path == NULL) {
    perror("smash error: getcwd failed");
    return;
  }
  std::cout << path << "\n";
  free(path);
}

/** SHOWPID **/
ShowPidCommand::ShowPidCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {}

void ShowPidCommand::execute() {
  std::cout << "smash pid is: " << getpid() << "\n";
}

/** CD **/
ChangeDirCommand::ChangeDirCommand(const char* cmd_line, char** plastPwd) : BuiltInCommand(cmd_line), plastPwd(plastPwd) {}

void ChangeDirCommand::execute() {
  if(num_of_args > 2) {
    std::cerr << "smash error: cd: too many arguments\n";
  }
  char* current_path = getcwd(NULL, 0);
  if(current_path == NULL) {
    perror("smash error: getcwd failed");
    return;
  }
  if(strcmp(args[1], "-") == 0) {
    if(*plastPwd == NULL) {
    }
    else {
      int success = chdir(*plastPwd);
      if(success != 0) {
        perror("smash error: chdir failed");
        return;
      }
    }
  } 
  else {
    int success = chdir(args[1]);
    if(success != 0) {
      perror("smash error: chdir failed");
      return;
    }
  }
  free(*plastPwd);
  *plastPwd = current_path;
}

/** QUIT **/
QuitCommand::QuitCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line) {}

void QuitCommand::execute() {
  exit(0);  // temporary
}
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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

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

bool _isBackgroundCommand(const char* cmd_line) {
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

bool is_digits(const std::string &str){
  return str.find_first_not_of("0123456789") == std::string::npos;
}

// TODO: Add your implementation for classes in Commands.h 

SmallShell::SmallShell() : last_path(NULL), job_list(), foreground_cmd(nullptr), foreground_jobid(-1), prompt("smash> ") {
// TODO: add your implementation
}

SmallShell::~SmallShell() {
// TODO: add your implementation
  // free(last_path);
}

/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
Command * SmallShell::CreateCommand(const char* cmd_line) {
  string cmd_s = _trim(string(cmd_line));
  string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));

  if(cmd_s.find(">") != std::string::npos){
    return new RedirectionCommand(cmd_line);
  }
  else if (firstWord.compare("pwd") == 0) {
    return new GetCurrDirCommand(cmd_line);
  }
  else if (firstWord.compare("chprompt") == 0) {
    return new ChangePromptCommand(cmd_line, &prompt);
  }
  else if (firstWord.compare("showpid") == 0) {
    return new ShowPidCommand(cmd_line);
  }
  else if(firstWord.compare("quit") == 0) {
    return new QuitCommand(cmd_line, &job_list);
  }
  else if (firstWord.compare("cd") == 0) {
    return new ChangeDirCommand(cmd_line, &last_path);
  }
  else if (firstWord.compare("jobs") == 0) {
    return new JobsCommand(cmd_line, &job_list);
  }
  else if(firstWord.compare("kill") == 0){
    return new KillCommand(cmd_line, &job_list);
  }
  else if(firstWord.compare("fg") == 0){
    return new ForegroundCommand(cmd_line, &job_list);
  }
  else if(firstWord.compare("bg") == 0){
    return new BackgroundCommand(cmd_line, &job_list);
  }

  else {
    return new ExternalCommand(cmd_line);
  }
  return nullptr;
}

void SmallShell::executeCommand(const char *cmd_line) {
  // TODO: Add your implementation here
  // for example:
  Command* cmd = CreateCommand(cmd_line);
  if(cmd) {
    job_list.removeFinishedJobs();
    cmd->execute();

    if(dynamic_cast<QuitCommand*>(cmd)) {
      delete cmd;
      free(last_path);
      // job_list.~JobsList();
      exit(0);
    }
  }
  // Please note that you must fork smash process for some commands (e.g., external commands....)
  delete cmd;
}

/** COMMAND **/
Command::Command(const char* cmd_line_const, bool ignore_ampersand) : cmd_line_const(cmd_line_const), is_in_bg(false), child_pid(-1) {
    cmd_line = new char[strlen(cmd_line_const)+1];
    strcpy(cmd_line, cmd_line_const);
    if(ignore_ampersand) {
      _removeBackgroundSign(cmd_line);
    }
    for(int i=0; i<COMMAND_MAX_ARGS; ++i) {
      args[i] = nullptr;
    }
    num_of_args = _parseCommandLine(cmd_line, args);
  };

void Command::prepare() {}  
void Command::cleanup() {}
/**************************************************************************************************************/
/**************************************************************************************************************/
/*********************************************INTERNAL COMMANDS************************************************/
/**************************************************************************************************************/
/**************************************************************************************************************/
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
    std::cerr << "smash error: cd: too many arguments" << endl;
    return;
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

/** JOBS **/
JobsCommand::JobsCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line), job_list(jobs) {}

void JobsCommand::execute() {
  job_list->printJobsList();
}

/** KILL **/
KillCommand::KillCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line), job_list(jobs) {}
void KillCommand::execute() {
  if(num_of_args != 3) {
    std::cerr << "smash error: kill: invalid arguments" << endl;
    return;
  }
  int job_id = atoi(args[2]);
  int signal = std::abs(atoi(args[1]));
  std::cout << "job_id: " << job_id << "\tsignal: " << signal << "\n";
  if(job_id == 0 || signal < 1 || signal > 31) {
    std::cerr << "smash error: kill: invalid arguments" << endl;
    return;
  }
  SmallShell& shell = SmallShell::getInstance();
  JobsList::JobEntry* job = shell.getJobById(job_id);
  if(job == nullptr) {
    std::cerr << "smash error: kill: job-id " << job_id << " does not exist" << endl;
    return;
  }
  if(kill(job->pid, signal) == -1) {
    perror("smash error: kill failed");
    return;
  }
  if(signal == SIGCONT){
    job->is_stopped = false;
  }
  else if(signal == SIGSTOP) {
    job->is_stopped = true;
  }
  std::cout << "signal number " << signal << " was sent to pid " << job->pid << "\n";
}

/** FG **/
ForegroundCommand::ForegroundCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line), job_list(jobs) {}

void ForegroundCommand::execute() {
  SmallShell& smash = SmallShell::getInstance();
  JobsList::JobEntry* job;
  int job_id;
  if(num_of_args == 1) {
    job = job_list->getLastJob(&job_id);
    if(job_id == 0) {
      std::cerr << "smash error: fg: jobs list is empty\n";
      return;
    }//sigcont - remove from list - wait
  }
  else if(num_of_args == 2) {
    job_id = atoi(args[1]);
    job = job_list->getJobById(job_id);
    if(job == nullptr && is_digits(string(args[1]))) {
      std::cerr << "smash error: fg: job-id " << job_id << " does not exist\n";
      return;
    }
  }
  else {
    std::cerr << "smash error: fg: invalid arguments\n";
    return;
  }

  if(job->is_stopped == true) {
    if(kill(job->pid, SIGCONT) == -1){
      perror("smash error: kill failed\n");
    }
    job->is_stopped = false;
  }
  string temp = std::move(job->cmd_line);
  ExternalCommand ecmd = ExternalCommand(temp.c_str());
  smash.foreground_cmd = &ecmd;//new ExternalCommand(job->cmd_line); 
  smash.foreground_cmd->child_pid = job->pid;
  smash.foreground_jobid = job->job_id;
  std::cout << smash.foreground_cmd->cmd_line_const << " : " << job->pid << "\n";
  job->cmd_line = "";
  job_list->removeJobById(job_id);
  int success = 0;
  int status = 0;
  do {
    success = waitpid(smash.foreground_cmd->child_pid, &status, WNOHANG | WUNTRACED | WCONTINUED);
  } while(success == 0 || (success > 0 && WIFEXITED(status) == 0 && WIFCONTINUED(status) == 1));
  std:: cout << WIFEXITED(status) << ", " << WIFSTOPPED(status) << ", " << WIFCONTINUED(status) << "\n";
  // free(smash.foreground_cmd);
  smash.foreground_cmd = nullptr;
  smash.foreground_jobid = -1;
}

BackgroundCommand::BackgroundCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line), job_list(jobs) {}

void BackgroundCommand::execute() {
  int job_id;
  JobsList::JobEntry* job;
  if(num_of_args == 1) {
    job = job_list->getLastStoppedJob(&job_id);
    if(job_id == 0) {
      std::cerr << "smash error: bg: there is no stopped jobs to resume\n";
      return;
    }//sigcont - remove from list - wait
  }
  else if(num_of_args == 2) {
    job_id = atoi(args[1]);
    job = job_list->getJobById(job_id);
    if(job == nullptr && is_digits(string(args[1]))) {
      std::cerr << "smash error: fg: job-id " << job_id << " does not exist\n";
      return;
    }
    else if(job->is_stopped == false) {
      std::cerr << "smash error: bg: job-id " << job->job_id << "is already running in the background\n";
      return;
    }
  }
  else {
    std::cerr << "smash error: fg: invalid arguments\n";
    return;
  }
  std::cout << job->cmd_line << " : " << job->pid << "\n";
  if(kill(job->pid, SIGCONT) == -1) {
    perror("smash error: kill failed\n");
    return;
  }
  job->is_stopped = false;
}
// }
/** QUIT **/
QuitCommand::QuitCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line), job_list(jobs) {}

void QuitCommand::execute() {
  if(num_of_args > 1) {
    std::string arg = _trim(std::string(args[1]));
    if(arg.compare("kill") == 0) {
      job_list->killAllJobs();
    }
  }
}

/**************************************************************************************************************/
/**************************************************************************************************************/
/*********************************************EXTERNAL COMMANDS************************************************/
/**************************************************************************************************************/
/**************************************************************************************************************/

ExternalCommand::ExternalCommand(const char* cmd_line) : Command(cmd_line) {
  is_in_bg = _isBackgroundCommand(cmd_line);
}

void ExternalCommand::execute() {
  pid_t pid = fork();
  if(pid == -1) {
    perror("smash error: fork failed");
  }
  if(pid == 0) {  // child
    setpgrp();
    _removeBackgroundSign(cmd_line);
    int success = execl("/bin/bash", "bash", "-c", cmd_line, NULL);
    if(success == -1) {
      perror("smash error: execv failed");
    }
  }
  else {  //parent
    this->child_pid = pid;
    SmallShell& smash = SmallShell::getInstance();
    if(!is_in_bg) {
      smash.foreground_cmd = this;
      int success = 0;
      int status = 0;
      do {
        success = waitpid(pid, &status, WNOHANG | WUNTRACED | WCONTINUED);
      } while(success == 0);
      // std:: cout << WIFEXITED(status) << ", " << WIFSTOPPED(status) << "\n";
      smash.foreground_cmd = nullptr;
      if(success == -1) {
        perror("smash error: waitpid failed");
      }
    }
    else {
      smash.moveToBackground(this);  // calls addJob
    }
  }
}

/**************************************************************************************************************/
/**************************************************************************************************************/
/**************************************JOBS LIST IMPLEMENTATION************************************************/
/**************************************************************************************************************/
/**************************************************************************************************************/

/** JOB_ENTRY **/
JobsList::JobEntry::JobEntry(Command* cmd, bool isStopped, int job_id) 
  : job_id(job_id), pid(cmd->child_pid), start_time(time(NULL)), 
    cmd_line(cmd->cmd_line_const), is_stopped(isStopped) {
      if(pid == -1) {
        // should never happen
        std::cout << "command is not a child proccess but is still in the jobs list\n";
        // perror("smash error: getpid failed");
      }
      // strcpy(this->cmd_line, cmd->cmd_line_const);
    }

JobsList::JobEntry::JobEntry(const JobsList::JobEntry& other) {
  job_id = other.job_id;
  pid = other.pid;
  start_time = other.start_time;
  // cmd_line = new char[strlen(other.cmd_line)+1];
  cmd_line = other.cmd_line;
  // strcpy(this->cmd_line, other.cmd_line);
  is_stopped = other.is_stopped;
}
JobsList::JobEntry& JobsList::JobEntry::operator=(const JobsList::JobEntry& other) {
  if (this == &other)
    return *this;
  job_id = other.job_id;
  pid = other.pid;
  start_time = other.start_time;
  // delete[] cmd_line;
  // cmd_line = new char[strlen(other.cmd_line)+1];
  cmd_line = other.cmd_line;
  // strcpy(this->cmd_line, other.cmd_line);
  is_stopped = other.is_stopped;
  return *this;
}

JobsList::JobEntry::JobEntry(JobsList::JobEntry&& other) {
  // cout << "moved!\n";
  job_id = other.job_id;
  pid = other.pid;
  start_time = other.start_time;
  cmd_line = std::move(other.cmd_line);
  // other.cmd_line = nullptr;
  is_stopped = other.is_stopped;
}

JobsList::JobEntry::~JobEntry() {
  // delete[] cmd_line;
}
/** JOB_LIST **/
int JobsList::getMaxJobID() {
  if(jobs_vec.empty()){
    return 0;
  }
    return std::max_element(jobs_vec.begin(), jobs_vec.end())->job_id;
    // return getLastJob()->job_id;
}

void JobsList::addJob(Command* cmd, bool isStopped) {
  this->removeFinishedJobs();
  SmallShell& smash = SmallShell::getInstance();
  int job_id = (smash.foreground_jobid == -1) ? getMaxJobID()+1 : smash.foreground_jobid;
  // JobEntry job(cmd, isStopped, job_id);
  // jobs_vec.push_back(std::move(job));
  jobs_vec.push_back(JobEntry(cmd, isStopped, job_id));
}

void JobsList::printJobsList() {
  this->removeFinishedJobs();
  std::sort(jobs_vec.begin(), jobs_vec.end());
  for(auto& it : jobs_vec) {
    std::string stopped_string = "";
    if(it.is_stopped == true) {
      stopped_string = "(stopped)";
    }
    time_t current_time = time(NULL);
    std::cout << "[" << it.job_id << "] " << it.cmd_line << " : " << it.pid \
              << " " << difftime(current_time, it.start_time) << " secs " << stopped_string << "\n";
  }
}
void JobsList::killAllJobs() {
  std::sort(jobs_vec.begin(), jobs_vec.end());
  std::cout << "sending SIGKILL signal to " << jobs_vec.size() << " jobs:\n";
  for(auto it = jobs_vec.begin(); it != jobs_vec.end(); ++it) {
    std::cout << it->pid << ": " << it->cmd_line << endl;
    int success = kill(it->pid, SIGKILL);
    if(success == -1) {
      perror("smash error: kill failed");
    }
  }
}

void JobsList::removeFinishedJobs() {
  auto it = jobs_vec.begin();
  while(it != jobs_vec.end()) {
    int status;
    int success = waitpid(it->pid, &status, WNOHANG | WUNTRACED | WCONTINUED);
    if(success > 0) {
      if((WIFEXITED(status) || WIFSIGNALED(status)) && !(WIFSTOPPED(status))) {
        it = jobs_vec.erase(it);
        continue;
      }
    }
    else if(success == -1) {
      perror("smash error: waitpid failed");
    }
    ++it;
  }
}

JobsList::JobEntry* JobsList::getJobById(int jobId) {
  auto it = std::find_if(jobs_vec.begin(), jobs_vec.end(), JobEntry::CompareByJobID(jobId));
  if(it == jobs_vec.end()) {
    return nullptr;
  }
  return &*it;
}

//simply remove the job from the vector(no signals sent)
void JobsList::removeJobById(int jobId) {
  for(auto it = jobs_vec.begin(); it != jobs_vec.end(); ++it) {
    if(it->job_id == jobId) {
        it = jobs_vec.erase(it);
        return;
      }
    }
  }

JobsList::JobEntry* JobsList::getLastJob(int* lastJobId) {
  auto it = std::max_element(jobs_vec.begin(), jobs_vec.end());
  // JobEntry* job = &*std::max_element(jobs_vec.begin(), jobs_vec.end());
  if(it == jobs_vec.end()) {
    *lastJobId = 0;
    return nullptr;
  }
  *lastJobId = it->job_id;
  return &*it;
}

JobsList::JobEntry* JobsList::getLastStoppedJob(int *jobId) {
  auto it = std::max_element(jobs_vec.begin(), jobs_vec.end(), JobEntry::StoppedLessThan);
  if(it == jobs_vec.end() || it->is_stopped == false) {
    *jobId = 0;
    return nullptr;
  }
  *jobId = it->job_id;
  return &*it;
}

/**************************************************************************************************************/
/**************************************************************************************************************/
/*********************************************SPECIAL COMMANDS************************************************/
/**************************************************************************************************************/
/**************************************************************************************************************/

/** REDIRECTION **/
RedirectionCommand::RedirectionCommand(const char* cmd_line) : Command(cmd_line, true), new_file_descriptor(-1), append(false) {
  std::string cmd_line_s = std::string(cmd_line);
  int arrow_index = cmd_line_s.find_first_of(">");
  if(cmd_line_s[arrow_index+1] == '>') {
    this->append = true;
  }
  int file_path_start = arrow_index + (append == true ? 2 : 1); //skip >> instead of >
  cmd = std::string(cmd_line_s.begin(), cmd_line_s.begin()+arrow_index);
  file_path = _trim(std::string(cmd_line_s.begin()+file_path_start, cmd_line_s.end()));
  auto it = cmd.find_last_of('&');
  if(it != std::string::npos) {
    cmd.erase(it);  // removes background sign
  }
}

void RedirectionCommand::prepare() {
  new_file_descriptor = dup(1);
  if(new_file_descriptor == -1) {
    perror("smash error: dup failed");
  }
  if(close(1) == -1) {
    perror("smash error: close failed");
  }
  int out_channel = -1;
  if(this->append == true) {
    out_channel = open(file_path.c_str(), O_WRONLY | O_CREAT | O_APPEND, S_IRWXU | S_IRWXG);
  } 
  else {
    out_channel = open(file_path.c_str(), O_WRONLY | O_CREAT, S_IRWXU | S_IRWXG);
  }
  if(out_channel == -1) {
    perror("smash error: open failed");
  }
}

void RedirectionCommand::cleanup() {
  if(dup2(new_file_descriptor, 1) == -1) {  // returns stdout to channel 1
    perror("smash error: dup2 failed");
  }
  if(close(new_file_descriptor) == -1) {
    perror("smash error: close failed");
  }
}

void RedirectionCommand::execute() {
  prepare();
  SmallShell& smash = SmallShell::getInstance();
  smash.executeCommand(cmd.c_str());
  cleanup();
}
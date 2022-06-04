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

#define STDIN 0
#define STDOUT 1
#define STDERR 2
#define PIPE_RD 0
#define PIPE_WR 1

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
  return str.find_first_not_of("-0123456789") == std::string::npos;
}

SmallShell::SmallShell() : last_path(NULL), job_list(), timed_cmds(), time_this(-1), foreground_cmd(nullptr), 
                           foreground_jobid(-1), prompt("smash> "), smash_pid(getpid()) {}

SmallShell::~SmallShell() {}

void SmallShell::killTimedCmmands() {
  auto it = timed_cmds.begin();
  while(it != timed_cmds.end()) {
    if(it->pid == -1) {  // ran in foreground in finished
      it = timed_cmds.erase(it);
      continue;
    }
    int status;
    int success = waitpid(it->pid, &status, WNOHANG | WUNTRACED | WCONTINUED);
    if(success > 0) {  // finished
      if((WIFEXITED(status) || WIFSIGNALED(status)) && !(WIFSTOPPED(status))) {  // terminated reguraly
        it = timed_cmds.erase(it);
        continue;
      }
    }
    else if(success == -1) {
      perror("smash error: waitpid failed");
      return;
    }
    else if(difftime(time(NULL), it->start_time) >= it->time_to_alarm)  {  // not finished but timed out
      if(kill(it->pid, SIGKILL) == -1) {
        perror("smash error: kill failed");
        return;
      }
      std::cout << "smash: " << it->cmd_s << " timed out!" << std::endl;
      it = timed_cmds.erase(it);
      continue;
    }
    ++it;
  }
}
/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
Command * SmallShell::CreateCommand(const char* cmd_line) {
  string cmd_s = _trim(string(cmd_line));
  string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));

  if(cmd_s.find(">") != std::string::npos) {
    return new RedirectionCommand(cmd_line);
  }
  else if(cmd_s.find("|") != std::string::npos) {
    return new PipeCommand(cmd_line);
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
  else if(firstWord.compare("kill") == 0) {
    return new KillCommand(cmd_line, &job_list);
  }
  else if(firstWord.compare("fg") == 0) {
    return new ForegroundCommand(cmd_line, &job_list);
  }
  else if(firstWord.compare("bg") == 0) {
    return new BackgroundCommand(cmd_line, &job_list);
  }
  else if(firstWord.compare("tail") == 0) {
    return new TailCommand(cmd_line);
  }
  else if(firstWord.compare("touch") == 0) {
    return new TouchCommand(cmd_line);
  }
  else if(firstWord.compare("timeout") == 0) {
    return new TimeoutCommand(cmd_line);
  }

  else {
    return new ExternalCommand(cmd_line);
  }
  return nullptr;
}

void SmallShell::executeCommand(const char *cmd_line) {

  Command* cmd = CreateCommand(cmd_line);
  if(cmd) {
    job_list.removeFinishedJobs();
    cmd->execute();

    if(dynamic_cast<QuitCommand*>(cmd)) {
      delete cmd;
      free(last_path);
      exit(0);
    }
  }
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

Command::~Command() {
  for(int i=0; i<COMMAND_MAX_ARGS; ++i) {
    if(args[i] == nullptr)
      return;
    free(args[i]);
  }
  delete[] cmd_line;
};

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
  std::cout << path << std::endl;
  free(path);
}

/** SHOWPID **/
ShowPidCommand::ShowPidCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {}

void ShowPidCommand::execute() {
  std::cout << "smash pid is " << (SmallShell::getInstance()).smash_pid << endl;
}

/** CD **/
ChangeDirCommand::ChangeDirCommand(const char* cmd_line, char** plastPwd) : BuiltInCommand(cmd_line), plastPwd(plastPwd) {}

void ChangeDirCommand::execute() {
  if(num_of_args > 2) {
    std::cerr << "smash error: cd: too many arguments" << endl;
    return;
  }
  else if(num_of_args == 1) {
    return;
  }
  char* current_path = getcwd(NULL, 0);
  if(current_path == NULL) {
    perror("smash error: getcwd failed");
    return;
  }
  if(strcmp(args[1], "-") == 0) {
    if(*plastPwd == NULL) {
      std::cerr << "smash error: cd: OLDPWD not set" << endl;
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
  if(job_id == 0 || signal < 1 || signal > 31 || atoi(args[1]) > 0 || is_digits(string(args[1])) == false) {
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
  if(signal == SIGCONT) {
    job->is_stopped = false;
  }
  else if(signal == SIGSTOP) {
    job->is_stopped = true;
  }
  std::cout << "signal number " << signal << " was sent to pid " << job->pid << std::endl;
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
      std::cerr << "smash error: fg: jobs list is empty" << std::endl;
      return;
    }
  }
  else if(num_of_args == 2) {
    job_id = atoi(args[1]);
    if(is_digits(string(args[1])) == false) {
      std::cerr << "smash error: fg: invalid arguments" << std::endl;
      return;
    }
    job = job_list->getJobById(job_id);
    if(job == nullptr) {
      std::cerr << "smash error: fg: job-id " << job_id << " does not exist" << std::endl;
      return;
    }
  }
  else {
    std::cerr << "smash error: fg: invalid arguments" << std::endl;
    return;
  }

  if(job->is_stopped == true) {
    if(kill(job->pid, SIGCONT) == -1){
      perror("smash error: kill failed");
      return;
    }
    job->is_stopped = false;
  }
  string temp = std::move(job->cmd_line);
  ExternalCommand ecmd = ExternalCommand(temp.c_str());
  smash.foreground_cmd = &ecmd;
  smash.foreground_cmd->child_pid = job->pid;
  smash.foreground_jobid = job->job_id;
  std::cout << smash.foreground_cmd->cmd_line_const << " : " << job->pid << std::endl;
  job->cmd_line = "";
  job_list->removeJobById(job_id);
  int success = 0;
  int status = 0;
  do {
    success = waitpid(smash.foreground_cmd->child_pid, &status, WNOHANG | WUNTRACED | WCONTINUED);
  } while(success == 0 || (success > 0 && WIFEXITED(status) == 0 && WIFCONTINUED(status) == 1));
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
      std::cerr << "smash error: bg: there is no stopped jobs to resume" << std::endl;
      return;
    }
  }
  else if(num_of_args == 2) {
    job_id = atoi(args[1]);
    job = job_list->getJobById(job_id);
    if(job == nullptr) {
      std::cerr << "smash error: bg: job-id " << job_id << " does not exist" << std::endl;
      return;
    }
    else if(job->is_stopped == false) {
      std::cerr << "smash error: bg: job-id " << job->job_id << " is already running in the background" << std::endl;
      return;
    }
  }
  else {
    std::cerr << "smash error: bg: invalid arguments" << std::endl;
    return;
  }
  std::cout << job->cmd_line << " : " << job->pid << std::endl;
  if(kill(job->pid, SIGCONT) == -1) {
    perror("smash error: kill failed");
    return;
  }
  job->is_stopped = false;
}

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

/** TAIL **/
TailCommand::TailCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {}

void TailCommand::execute() {
  int N = 10, readerr, writeerr;
  std::string file_path;
  if(num_of_args == 2) {
    file_path = args[1];
  }
  else if(num_of_args == 3) {
    N = std::abs(atoi(args[1]));
    std::string tmp = args[1];
    if(tmp.rfind("--", 0) == 0 || tmp.find_first_of('-') == std::string::npos || tmp.find_first_of("0123456789") == std::string::npos) {
      std::cerr << "smash error: tail: invalid arguments" << std::endl;
      return;
    }
    if(N == 0) {
      return;
    }
    file_path = args[2];
  }
  else {
    std::cerr << "smash error: tail: invalid arguments" << std::endl;
    return;
  }

  int fd = open(_trim(file_path).c_str(), 0);
  if(fd == -1) {
    perror("smash error: open failed");
    return;
  }
  int end_offset = lseek(fd, -1, SEEK_END) + 1;  // -1 so seeker will be just before '\0'

  // if(end_offset == 0) {
  //   perror("smash error: lseek failed");
  //   return;
  // }
  int seeker = end_offset - 1;

  char c = '\0';
  while(seeker > 0) {  // exit if reached start of file
    readerr = read(fd, &c, 1);
    if(readerr == -1){
      perror("smash error: read failed");
      return;
    }
    else if(readerr == 0) { // might happen when reading from pipe
      continue;
    }
    if(c == '\n') {
      if(seeker != end_offset - 1) {  // ignores last newline
        N--;
    }
      if(N == 0) break;
    }
    seeker = lseek(fd, seeker-1, SEEK_SET);
    if(seeker == -1) {
      perror("smash error: seek failed");
      return;
    }
  }
  if(seeker != 0) {
    seeker++;  // seek next character after the N+1 '\n'
  }
  int buf_size = std::min(256, end_offset-seeker);
  void* buf[buf_size];
  do {
    readerr = read(fd, buf, buf_size);
    if(readerr == -1) {
      perror("smash error: read failed");
      return;
    }
    writeerr = write(STDOUT, buf, readerr);
    if(writeerr == -1) {
      perror("smash error: write failed");
      return;
    }
  }
  while(readerr > 0);

  if(close(fd) == -1) {
    perror("smash error: close failed");
    return;
  }
}

/* TOUCH */
TouchCommand::TouchCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {}

void TouchCommand::execute() {
  if(num_of_args != 3) {
    std::cerr << "smash error: touch: invalid arguments" << std::endl;
    return;
  }
  std::string timestamp = args[2];
  struct tm timeptr;
  size_t lpos = 0, rpos = timestamp.find_first_of(":");  // stupid code but w/e
  timeptr.tm_sec = atoi(timestamp.substr(lpos, rpos).c_str());
  lpos = rpos+1;
  rpos = timestamp.find_first_of(":", lpos);
  timeptr.tm_min = atoi(timestamp.substr(lpos, rpos).c_str());
  lpos = rpos+1;
  rpos = timestamp.find_first_of(":", lpos);
  timeptr.tm_hour = atoi(timestamp.substr(lpos, rpos).c_str());
  lpos = rpos+1;
  rpos = timestamp.find_first_of(":", lpos);
  timeptr.tm_mday = atoi(timestamp.substr(lpos, rpos).c_str());
  lpos = rpos+1;
  rpos = timestamp.find_first_of(":", lpos);
  timeptr.tm_mon = atoi(timestamp.substr(lpos, rpos).c_str()) - 1;
  lpos = rpos+1;
  rpos = timestamp.find_first_of(":", lpos);
  timeptr.tm_year = atoi(timestamp.substr(lpos, timestamp.size()+1).c_str()) - 1900; 
  timeptr.tm_isdst = true;
  time_t epoch_time = mktime(&timeptr);
  struct utimbuf timestruct {epoch_time, epoch_time};
  if(utime(args[1], &timestruct) == -1) {
    perror("smash error: utime failed");
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
    return;
  }
  if(pid == 0) {  // child
    setpgrp();
    _removeBackgroundSign(cmd_line);
    int success = execl("/bin/bash", "/bin/bash", "-c", cmd_line, NULL);
    if(success == -1) {
      perror("smash error: execv failed");
      return;
    }
  }
  else {  //parent
    this->child_pid = pid;
    SmallShell& smash = SmallShell::getInstance();
    if(smash.time_this.time_this == true) {
      smash.time_this.pid = pid;
      smash.time_this.start_time = time(NULL);
      // smash.time_this->cmd_s = cmd_line_const;
    }
    if(!is_in_bg) {
      auto it = smash.timeThis();  // if time_this == true, adds him to end of timed commands list
      smash.foreground_cmd = this;
      int success = 0;
      int status = 0;
      do {
        success = waitpid(pid, &status, WNOHANG | WUNTRACED | WCONTINUED);
      } while(success == 0);
      smash.foreground_cmd = nullptr;
      smash.foreground_jobid = -1;
      if(success == -1) {
        perror("smash error: waitpid failed");
        return;
      }
      if((WIFEXITED(status) == 1 && WIFSTOPPED(status) == 0) ||
         (WIFSIGNALED(status) == 1 && WTERMSIG(status) == SIGINT)) {  
        smash.reset_timed_cmd(it);  // process finished normally or by signal, mark it as such
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
        std::cout << "command is not a child proccess but is still in the jobs list" << std::endl;
      }
    }

JobsList::JobEntry::JobEntry(const JobsList::JobEntry& other) {
  job_id = other.job_id;
  pid = other.pid;
  start_time = other.start_time;
  cmd_line = other.cmd_line;
  is_stopped = other.is_stopped;
}

JobsList::JobEntry& JobsList::JobEntry::operator=(const JobsList::JobEntry& other) {
  if (this == &other)
    return *this;
  job_id = other.job_id;
  pid = other.pid;
  start_time = other.start_time;
  cmd_line = other.cmd_line;
  is_stopped = other.is_stopped;
  return *this;
}

JobsList::JobEntry::JobEntry(JobsList::JobEntry&& other) {
  job_id = other.job_id;
  pid = other.pid;
  start_time = other.start_time;
  cmd_line = std::move(other.cmd_line);
  is_stopped = other.is_stopped;
}

JobsList::JobEntry::~JobEntry() {}

/** JOB_LIST **/
int JobsList::getMaxJobID() const {
  if(jobs_vec.empty()){
    return 0;
  }
    return std::max_element(jobs_vec.begin(), jobs_vec.end())->job_id;
}

void JobsList::addJob(Command* cmd, bool isStopped) {
  this->removeFinishedJobs();
  SmallShell& smash = SmallShell::getInstance();
  int job_id = (smash.foreground_jobid == -1) ? getMaxJobID()+1 : smash.foreground_jobid;
  jobs_vec.push_back(JobEntry(cmd, isStopped, job_id));
}

void JobsList::printJobsList() {
  this->removeFinishedJobs();
  std::sort(jobs_vec.begin(), jobs_vec.end());
  for(auto& it : jobs_vec) {
    std::string stopped_string = "";
    if(it.is_stopped == true) {
      stopped_string = " (stopped)";
    }
    time_t current_time = time(NULL);
    std::cout << "[" << it.job_id << "] " << it.cmd_line << " : " << it.pid \
              << " " << difftime(current_time, it.start_time) << " secs" << stopped_string << std::endl;
  }
}
void JobsList::killAllJobs() {
  std::sort(jobs_vec.begin(), jobs_vec.end());
  std::cout << "smash: sending SIGKILL signal to " << jobs_vec.size() << " jobs:" << std::endl;
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
      return;
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
/**********************************************SPECIAL COMMANDS************************************************/
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
  new_file_descriptor = dup(STDOUT);
  if(new_file_descriptor == -1) {
    perror("smash error: dup failed");
    cmd = "";
    return;
  }
  if(close(STDOUT) == -1) {
    perror("smash error: close failed");
    cmd = "";
    return;
  }
  int out_channel = -1;
  if(this->append == true) {
    out_channel = open(file_path.c_str(), O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
  } 
  else {
    out_channel = open(file_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
  }
  if(out_channel == -1) {
    perror("smash error: open failed");
    cmd = ""; // reset command so nothing will happen in execute
    return;
  }
}

void RedirectionCommand::cleanup() {
  if(dup2(new_file_descriptor, STDOUT) == -1) {  // returns stdout to channel 1
    perror("smash error: dup2 failed");
    return;
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

/** PIPE **/
PipeCommand::PipeCommand(const char* cmd_line) : Command(cmd_line, true), redir_err(false) {
  std::string cmd_line_s = std::string(cmd_line);
  int pipe_index = cmd_line_s.find_first_of("|");
  if(cmd_line_s[pipe_index+1] == '&') {
    this->redir_err = true;  
  }
  int cmd2_start = pipe_index + (redir_err == true ? 2 : 1);
  cmd1 = string(cmd_line_s.begin(), cmd_line_s.begin()+pipe_index);
  cmd2 = string(cmd_line_s.begin()+cmd2_start, cmd_line_s.end());

  auto it = cmd1.find_last_of('&');
  if(it != std::string::npos) {
    cmd1.erase(it);  // removes background sign for first command
  }
  it = cmd2.find_last_of('&');
  if(it != std::string::npos) {
    cmd2.erase(it);  // removes background sign for second command
  }
}

void PipeCommand::execute(){
  pipe(fd);  // fd[0]: read, fd[1]: write
  SmallShell& smash = SmallShell::getInstance();
  int left_command = fork();
  if(left_command == 0) {
    // first chiild - left command (close read, open write)
    setpgrp();
    if(redir_err == true) {
      dup2(fd[PIPE_WR], STDERR);
    }
    else {
      dup2(fd[PIPE_WR], STDOUT);
    }
    close(fd[0]);
    close(fd[1]);
    smash.executeCommand(cmd1.c_str());
    exit(0);
  }
  int right_command = fork();
  if(right_command == 0) {
    // second child - right command (open read, close write)
    setpgrp();
    dup2(fd[PIPE_RD], STDIN);
    close(fd[0]);
    close(fd[1]);
    smash.executeCommand(cmd2.c_str());
    exit(0);
  }
  close(fd[0]);
  close(fd[1]);
  waitpid(left_command, NULL, 0);
  waitpid(right_command, NULL, 0);
}

/**************************************************************************************************************/
/**************************************************************************************************************/
/**************************************************BONUS*******************************************************/
/**************************************************************************************************************/
/**************************************************************************************************************/

/** TIMEOUT **/
TimeoutCommand::TimeoutCommand(const char* cmd_line) : Command(cmd_line) {}

// TimeoutCommand::~TimeoutCommand() {}

void TimeoutCommand::execute() {
  if(num_of_args < 3) {
    std::cerr << "smash error: timeout: invalid arguments" << std::endl;
    return;
  }
  int duration = atoi(args[1]);
  if(duration < 0) {
    std::cerr << "smash error: timeout: invalid arguments" << std::endl;
    return;
  }
  std::string cmd_line_s = std::string(cmd_line);
  std::size_t cmd_start_index = cmd_line_s.find_first_of(args[1][strlen(args[1])-1]);  // finds the index of the last char of args[1]
  std::string cmd = std::string(cmd_line_s.begin()+cmd_start_index+1, cmd_line_s.end());
  TimedCmd timed_cmd(duration);
  timed_cmd.cmd_s = cmd_line_const;
  SmallShell::getInstance().time_this = timed_cmd;  // copies
  alarm(duration);
  SmallShell::getInstance().executeCommand(cmd.c_str());
}
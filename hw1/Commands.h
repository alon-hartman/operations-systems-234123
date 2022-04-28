#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <vector>
#include <string>
#include <algorithm>
#include <list>

#define COMMAND_ARGS_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)

class Command {
 protected:
  char* args[COMMAND_MAX_ARGS];
  int num_of_args;

 public:
  const char* cmd_line_const;
  char* cmd_line;
  bool is_in_bg;
  pid_t child_pid;
  Command(const char* cmd_line, bool ignore_ampersand=false);
  virtual ~Command();
  virtual void execute() = 0;
  virtual void prepare();
  virtual void cleanup();
};

class BuiltInCommand : public Command {
 public:
  BuiltInCommand(const char* cmd_line) : Command(cmd_line, true) {};
};

class ExternalCommand : public Command {
 public:
  ExternalCommand(const char* cmd_line);
  void execute() override;
};

class PipeCommand : public Command {
  std::string cmd1;
  std::string cmd2;
  int fd[2];
  bool redir_err;
 public:
  PipeCommand(const char* cmd_line);
  virtual ~PipeCommand() {};
  void execute() override;
};

class RedirectionCommand : public Command {
  std::string cmd;
  std::string file_path;
  int new_file_descriptor;
  bool append;
 public:
  explicit RedirectionCommand(const char* cmd_line);
  virtual ~RedirectionCommand() {}
  void execute() override;
  void prepare() override;
  void cleanup() override;
};

class ChangePromptCommand : public BuiltInCommand {
 private:
  std::string* prompt;
 public:
  ChangePromptCommand(const char* cmd_line, std::string* prompt);
  virtual ~ChangePromptCommand() {}
  void execute() override;
};

class ChangeDirCommand : public BuiltInCommand {
 private:
  char** plastPwd;
 public:
  ChangeDirCommand(const char* cmd_line, char** plastPwd);
  virtual ~ChangeDirCommand() {}
  void execute() override;
};

class GetCurrDirCommand : public BuiltInCommand {
 public:
  GetCurrDirCommand(const char* cmd_line);
  virtual ~GetCurrDirCommand() {}
  void execute() override;
};

class ShowPidCommand : public BuiltInCommand {
 public:
  ShowPidCommand(const char* cmd_line);
  virtual ~ShowPidCommand() {}
  void execute() override;
};

class JobsList;
class QuitCommand : public BuiltInCommand {
 public:
  JobsList* job_list;
  QuitCommand(const char* cmd_line, JobsList* jobs);
  virtual ~QuitCommand() {}
  void execute() override;
};




class JobsList {
 public:
  struct JobEntry {
   int job_id;
   pid_t pid;
   time_t start_time;
   std::string cmd_line;
   bool is_stopped;
   bool operator<(const JobEntry& other) const {
    return this->job_id < other.job_id;
   }
   bool operator==(const JobEntry& other) const {
     return this->job_id == other.job_id;
   }
   JobEntry(Command* cmd, bool isStopped, int job_id);
   JobEntry(const JobEntry& other);
   JobEntry& operator=(const JobEntry& other);
   JobEntry(JobEntry&& other); // move constructor
   ~JobEntry();
   static bool StoppedLessThan(const JobEntry& a, const JobEntry& b) {
     return a.job_id * a.is_stopped < b.job_id * b.is_stopped;
   }
   struct CompareByJobID {
     int job_id;
     CompareByJobID(int job_id) : job_id(job_id) {}
     bool operator()(const JobEntry& other) const {
      return job_id == other.job_id;
     }
   };
  };
  std::vector<JobEntry> jobs_vec;

 public:
  JobsList() : jobs_vec() {}
  int getMaxJobID() const;
  void addJob(Command* cmd, bool isStopped = false);
  void printJobsList();
  void killAllJobs();
  void removeFinishedJobs();
  JobEntry * getJobById(int jobId);
  void removeJobById(int jobId); 
  JobEntry * getLastJob(int* lastJobId);
  JobEntry *getLastStoppedJob(int *jobId);
};

class JobsCommand : public BuiltInCommand {
 private:
  JobsList* job_list;
 public:
  JobsCommand(const char* cmd_line, JobsList* jobs);
  virtual ~JobsCommand() {}
  void execute() override;
};

class KillCommand : public BuiltInCommand {
 private:
  JobsList* job_list;
 public:
  KillCommand(const char* cmd_line, JobsList* jobs);
  virtual ~KillCommand() {}
  void execute() override;
};

class ForegroundCommand : public BuiltInCommand {
 private:
  JobsList* job_list;
 public:
  ForegroundCommand(const char* cmd_line, JobsList* jobs);
  virtual ~ForegroundCommand() {}
  void execute() override;
};

class BackgroundCommand : public BuiltInCommand {
 private:
  JobsList* job_list;
 public:
  BackgroundCommand(const char* cmd_line, JobsList* jobs);
  virtual ~BackgroundCommand() {}
  void execute() override;
};

class TailCommand : public BuiltInCommand {
 public:
  TailCommand(const char* cmd_line);
  virtual ~TailCommand() {}
  void execute() override;
};

class TouchCommand : public BuiltInCommand {
 public:
  TouchCommand(const char* cmd_line);
  virtual ~TouchCommand() {}
  void execute() override;
};

class TimeoutCommand : public Command {
 public:
  TimeoutCommand(const char* cmd_line);
  virtual ~TimeoutCommand() {}
  void execute() override;
};

struct TimedCmd {
  int time_to_alarm;
  pid_t pid;
  time_t start_time;
  std::string cmd_s;
  bool time_this;
  TimedCmd(int time_to_alarm) : time_to_alarm(time_to_alarm), pid(-1), start_time(-1), cmd_s(), time_this(true) {
    if(time_to_alarm == -1) {
      time_this = false;
    }
  };
  // TimedCmd(int time_to_alarm, pid_t pid, time_t start_time, bool is_finished) : time_to_alarm(time_to_alarm), pid(pid), start_time(start_time), is_finished(is_finished) {}
};

class SmallShell {
 private:
  char* last_path;
  JobsList job_list;
  std::list<TimedCmd> timed_cmds;

  SmallShell();
 public:
  TimedCmd time_this;
  Command* foreground_cmd;
  int foreground_jobid;
  std::string prompt;
  pid_t smash_pid;
  Command *CreateCommand(const char* cmd_line);
  SmallShell(SmallShell const&)      = delete; // disable copy ctor
  void operator=(SmallShell const&)  = delete; // disable = operator
  static SmallShell& getInstance() // make SmallShell singleton
  {
    static SmallShell instance; // Guaranteed to be destroyed.
    // Instantiated on first use.
    return instance;
  }
  ~SmallShell();
  void executeCommand(const char* cmd_line);
  JobsList::JobEntry* getJobById(int job_id) {
    return job_list.getJobById(job_id);
  }
  std::list<TimedCmd>::iterator timeThis() {
    if(this->time_this.time_this == true) {
      timed_cmds.push_back(time_this);
      this->time_this.time_this = false;
      return std::prev(timed_cmds.end());
    }
    return timed_cmds.end();
  }
  void reset_timed_cmd(std::list<TimedCmd>::iterator& it) {
    if(it != timed_cmds.end()) {
      it->pid = -1;
    }
  }
  void killTimedCmmands();  // in cpp file
  void moveToBackground(Command* cmd, bool is_stopped=false) {
    job_list.addJob(cmd, is_stopped);
    timeThis();
  }
};


#endif

#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <vector>
#include <string>
#include <algorithm>

#define COMMAND_ARGS_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)

class Command {
// TODO: Add your data members
 protected:
  char* args[COMMAND_MAX_ARGS];
  int num_of_args;

 public:
  const char* cmd_line_const;
  char* cmd_line;
  bool is_in_bg;
  pid_t child_pid;
  Command(const char* cmd_line, bool ignore_ampersand=false);
  virtual ~Command() {};
  virtual void execute() = 0;
  //virtual void prepare();
  //virtual void cleanup();
  // TODO: Add your extra methods if needed
  virtual void cleanArgsArray();
};

class BuiltInCommand : public Command {
 public:
  BuiltInCommand(const char* cmd_line) : Command(cmd_line, true) {};
  virtual ~BuiltInCommand() {
    cleanArgsArray();
  }
};

class ExternalCommand : public Command {
 public:
  ExternalCommand(const char* cmd_line);
  virtual ~ExternalCommand() {
    cleanArgsArray();
  }
  void execute() override;
};

class PipeCommand : public Command {
  // TODO: Add your data members
 public:
  PipeCommand(const char* cmd_line);
  virtual ~PipeCommand() {}
  void execute() override;
};

class RedirectionCommand : public Command {
 // TODO: Add your data members
 public:
  explicit RedirectionCommand(const char* cmd_line);
  virtual ~RedirectionCommand() {}
  void execute() override;
  //void prepare() override;
  //void cleanup() override;
};

class ChangePromptCommand : public BuiltInCommand {
 private:
  std::string* prompt;
 public:
  ChangePromptCommand(const char* cmd_line, std::string* prompt);
  virtual ~ChangePromptCommand() {};
  void execute() override;
};

class ChangeDirCommand : public BuiltInCommand {
 private:
  char** plastPwd;
 public:
  ChangeDirCommand(const char* cmd_line, char** plastPwd);
  virtual ~ChangeDirCommand() {};
  void execute() override;
};

class GetCurrDirCommand : public BuiltInCommand {
  // const char* path;
 public:
  GetCurrDirCommand(const char* cmd_line);
  virtual ~GetCurrDirCommand() {};
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
// TODO: Add your data members public:
 public:  // was private initialy 
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
   char* cmd_line;
   bool is_stopped;
   bool operator<(JobEntry& other) {
    return this->job_id < other.job_id;
   }
   bool operator==(JobEntry& other) {
     return this->job_id == other.job_id;
   }
   JobEntry(Command* cmd, bool isStopped, int job_id);
   JobEntry(const JobEntry& other);
   JobEntry& operator=(const JobEntry& other);
   JobEntry(JobEntry&& other);
   ~JobEntry();
   static bool StoppedLessThan(JobEntry& a, JobEntry& b) {
     return a.job_id * a.is_stopped < b.job_id * b.is_stopped;
   }
   struct CompareByJobID {
     int job_id;
     CompareByJobID(int job_id) : job_id(job_id) {}
     bool operator()(JobEntry& other) {
      return job_id == other.job_id;
     }
   };
  };
 // TODO: Add your data members
  std::vector<JobEntry> jobs_vec;

 public:
  JobsList() : jobs_vec() {}
  ~JobsList() {};
  int getMaxJobID();
  void addJob(Command* cmd, bool isStopped = false);
  void printJobsList();
  void killAllJobs();
  void removeFinishedJobs();
  JobEntry * getJobById(int jobId);
  void removeJobById(int jobId); 
  JobEntry * getLastJob(int* lastJobId);
  JobEntry *getLastStoppedJob(int *jobId);
  // TODO: Add extra methods or modify exisitng ones as needed
};

class JobsCommand : public BuiltInCommand {
 // TODO: Add your data members
 private:
  JobsList* job_list;
 public:
  JobsCommand(const char* cmd_line, JobsList* jobs);
  virtual ~JobsCommand() {}
  void execute() override;
};

class KillCommand : public BuiltInCommand {
 // TODO: Add your data members
 private:
  JobsList* job_list;
 public:
  KillCommand(const char* cmd_line, JobsList* jobs);
  virtual ~KillCommand() {}
  void execute() override;
};

class ForegroundCommand : public BuiltInCommand {
 // TODO: Add your data members
 private:
  JobsList* job_list;
 public:
  ForegroundCommand(const char* cmd_line, JobsList* jobs);
  virtual ~ForegroundCommand() {}
  void execute() override;
};

class BackgroundCommand : public BuiltInCommand {
 // TODO: Add your data members
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


class SmallShell {
 private:
  // TODO: Add your data members
  // char* args[COMMAND_MAX_ARGS];
  char* last_path;
  JobsList job_list;

  SmallShell();
  // void cleanArgsArray();
 public:
  Command* foreground_cmd;
  int foreground_jobid;
  std::string prompt;
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
  void moveToBackground(Command* cmd, bool is_stopped=false) {
    job_list.addJob(cmd, is_stopped);
  }
  // pid_t removeFromJobList(int job_id = -1);
};
#endif //SMASH_COMMAND_H_

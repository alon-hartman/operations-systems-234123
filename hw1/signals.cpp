#include <iostream>
#include <signal.h>
#include "signals.h"
#include "Commands.h"

using namespace std;

void ctrlZHandler(int sig_num) {
  std::cout << "smash: got ctrl-Z" << endl;
  SmallShell& shell = SmallShell::getInstance();
  if(shell.foreground_cmd->child_pid != -1) {
    int success = kill(shell.foreground_cmd->child_pid, SIGSTOP);
    if(success == -1){
      perror("smash error: kill failed");
      return;
    }
    shell.moveToBackground(shell.foreground_cmd, true);
  }
}

void ctrlCHandler(int sig_num) {
  std::cout << "smash: got ctrl-C" << endl;
  SmallShell& shell = SmallShell::getInstance();
  if(shell.foreground_cmd != nullptr) {
    int success = kill(shell.foreground_cmd->child_pid, sig_num);
    if(success == -1){
      perror("smash error: kill failed");
      return;
    }
    shell.foreground_cmd = nullptr;
    shell.foreground_jobid = -1;
  }
}

void alarmHandler(int sig_num) {
  std::cout << "smash: got an alarm" << endl;
  SmallShell::getInstance().killTimedCmmands();

}


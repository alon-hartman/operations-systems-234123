#include <iostream>
#include <signal.h>
#include "signals.h"
#include "Commands.h"

using namespace std;

void ctrlZHandler(int sig_num) {
	// TODO: Add your implementation
  std::cout << "smash: got ctrl-Z" << endl;
  SmallShell& shell = SmallShell::getInstance();
  if(shell.foreground_cmd != nullptr) {
    int success = kill(shell.foreground_cmd->child_pid, sig_num);
    std::cout<< "sig number: "<< sig_num <<endl;
    if(success == -1){
      perror("smash error: kill failed\n");
      return;
    }
    shell.moveToBackground(shell.foreground_cmd, true);
    shell.foreground_cmd = nullptr;
  }
}

void ctrlCHandler(int sig_num) {
  // TODO: Add your implementation
  std::cout << "smash: got ctrl-C" << endl;
  SmallShell& shell = SmallShell::getInstance();
  if(shell.foreground_cmd != nullptr) {
    int success = kill(shell.foreground_cmd->child_pid, sig_num);
    std::cout<< "sig number: "<< sig_num <<endl;
    if(success == -1){
      perror("smash error: kill failed\n");
      return;
    }
    shell.foreground_cmd = nullptr;
  }
}

void alarmHandler(int sig_num) {
  // TODO: Add your implementation
}


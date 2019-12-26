// Controls exactly which of its children processes is scheduled by 
// explicitly setting the priority of the chosen process to a higher 
// level than the other processes. (From project specification).

#include "types.h"
#include "user.h"
#include "param.h"
#include "mmu.h"
#include "proc.h"
#include "stddef.h"
#include "pstat.h"

int main(int argc, char *argv[]) {

  setpri(getpid(), 3);

  int timeSlice = atoi(argv[1]);
  int iter = atoi(argv[2]);
  int jobs = atoi(argv[4]);

  int childProcs[64];

  for (int i = 0; i < jobs; i++) {
    int PID = fork2(0);

    if (PID == 0) {
      exec(argv[3], NULL);
      exit();
    } 
    else {
      childProcs[i] = PID;
    }
  }

  for (int j = 0; j < iter; j++) {
    for (int i = 0; i < jobs; i++) {
      setpri(childProcs[i], 3);
      sleep(timeSlice);
      setpri(childProcs[i], 0);
    }
  }

  for (int i = 0; i < jobs; i++) {
    kill(childProcs[i]);
  }

  struct pstat pstat;
  getpinfo(&pstat);

  for (int i = 0; i < NPROC; i++) {

    if (pstat.pid[i]) {
      printf(1, "pid : %d, inuse: %d, priority: %d, "
                "procstate: %d, \n", pstat.pid[i], pstat.inuse[i], pstat.priority[i], (int)pstat.state[i]);

      for (int j = 0; j < NLAYER; j++) {
        printf(1, "ticks at %d is: %d, qtails at %d is: %d\n", j, pstat.ticks[i][j], j, pstat.qtail[i][j]);
      }
    }
  }
  exit();
}

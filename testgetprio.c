
#ifdef CS333_P4
#include "types.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  int pid, rc;

  if (argc != 2) {
    printf(2, "Error: Enter a PID to get priority\n");
    exit();
  }
  else {
      pid = atoi(argv[1]);
  }

  rc = getpriority(pid);
  if (rc < 0) {
      printf(2, "Error: invalid pid\n");
  }
  else
  {
    printf(1, "This is the priority we've received: %d", rc);
  }
  exit();
}
#endif

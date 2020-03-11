#ifdef CS333_P2

#include "types.h"
#include "user.h"

int main(int argc, char *argv[])
{

  int pid;
  int start = uptime();

//fork the children
  pid = fork();
  ++argv;

//check to see if fork worked
  if(pid < 0)
  {
    printf(2, "fork failed :(");
    exit();
  }

  //make sure pid exists
  if(pid)
  {
    pid = wait();
    int stop = uptime();
    stop -= start;
    int ten, hund, thou;

    ten = stop % 1000;
    stop /= 1000;
    hund = ten % 100;
    ten /= 100;
    thou = hund % 10;
    hund /= 10;

    printf(1, "%s ran in %d.%d%d%d time\n", argv[0], stop, ten, hund, thou);

  }

//pid is 0 but there is something in command line, call exec
  else if(argv[0] != NULL)
  {
    exec(argv[0], argv);
    exit();
  }

  exit();

}

#endif //CS333_P2

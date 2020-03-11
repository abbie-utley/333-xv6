#ifdef CS333_P2

#include "types.h"
#include "user.h"
#include "uproc.h"


int main(int argc, char *argv[])
{

  //variables
  int max = 72;
  uint CPUSEC, CPUTEN, CPUHUND,CPUMILLI;
  uint T_SEC, T_TEN, T_HUND, T_MILLI;

  struct uproc *table = malloc(sizeof(struct uproc)*max);

  if(table == NULL)
  {
    printf(2, "malloc failed :(");
    exit();
  }

  int proc_total = getprocs(max, table);

  if(proc_total <=0)
  {
    printf(2, "table failed :(");
    exit();
  }

#ifdef CS333_P4
  printf(1, "\nPID\tName\t\tUID\tGID\tPPID\tPrio\tElapsed\t\tCPU\tState\tSize\n");

#else //end of P4
  printf(1, "\nPID\tName\t\tUID\tGID\tPPID\tElapsed\t\tCPU\tState\tSize\n");
#endif // end of madness

  for(int i = 0; i < proc_total; ++i)
  {
    if(table[i].pid !=0)
    {

      CPUSEC = table[i].CPU_total_ticks;
      CPUTEN = CPUSEC % 1000;
      CPUSEC /= 1000;
      CPUHUND = CPUTEN % 100;
      CPUTEN /= 100;
      CPUMILLI = CPUHUND % 10;
      CPUHUND /= 10;

      T_SEC = table[i].elapsed_ticks;
      T_TEN = T_SEC % 1000;
      T_SEC /= 1000;
      T_HUND = T_TEN % 100;
      T_TEN /= 100;
      T_MILLI = T_HUND % 10;
      T_HUND /= 10;

#ifdef CS333_P4
      printf(1, "\n%d\t%s\t\t%d\t%d\t%d\t%d\t%d.%d%d%d\t\t%d.%d%d%d\t%s%d\n", table[i].pid, table[i].name, table[i].uid, table[i].gid, table[i].ppid, table[i].priority, T_SEC, T_TEN, T_HUND, T_MILLI, CPUSEC,CPUTEN,CPUHUND, CPUMILLI, table[i].state, table[i].size);
#else //project 2

      printf(1, "\n%d\t%s\t\t%d\t%d\t%d\t%d.%d%d%d\t\t%d.%d%d%d\t%s%d\n", table[i].pid, table[i].name, table[i].uid, table[i].gid, table[i].ppid, T_SEC, T_TEN, T_HUND, T_MILLI, CPUSEC,CPUTEN,CPUHUND, CPUMILLI, table[i].state, table[i].size);
#endif //end of p4 and p2 madness
    }
  }
  free(table);
  exit();
}

#endif //CS333_P2 & P4

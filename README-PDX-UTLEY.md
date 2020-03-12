Please read the other two README's for the XV6 that was used, and the changes that were made to it.

My following changes are through 4 projects:

1. Added a system call for the functionality of date. Works in UTC time. ctrl-p prints out the 
information of a process that is active including: PID, Name, Elapsed time, state, size, and PCs. 
This project also traced the system calls with print syscalls turned on in the Makefile.

2. Added a UID and GID field in a process structure. Added a PPID which is defined by its parent's process's PID
This also added a set and get functionality for the UID and the GID, along with syscalls. 
It also tracks the amount of time a process is in the CPU with ticks that was implemented in PDX
version of XV6. 
It implemented the 'ps' call at the command line which essentially prints the same information as ctrl-p except the PCs
and uses the uproc structure in urpoc.h to easily grab the process structure without having to be in proc.c. 
It uses getprocs field in proc.c to translate the process over to user space without having to be in the kernel. 

3. Got rid of the array of processes idea and replaced it with state lists. Organized them by the state of the process: 
UNUSED, EMBRYO, RUNNABLE, RUNNING, SLEEPING, and ZOMBIE. 
With this added functionality, it is easier and faster for a process to switch states and be found in the process structure without
having to loop through and find the right process with the right state.
This project also added a series of control functions. Control-r prints the runnable list, 
control-z prnts the zombie list, control-s prints the sleeping list, and control-f
prints the unused or the free list. There's an added functionality for both testing, and also control-l
prints all the processes on each list - this was given to us by our instructor: Mark Morrissey.

4. This project changed the RUNNABLE state to an MLFQ for better functionality.
Here it can have a maximum priority that is defined in pdx.h and is easily changable to the OS operator.
It exercises both demotion and promotion, (promotion in this case promotes each process by one) and is
easily able to turn off. control-r is updated and shows each maximum priority,
and there's a commented out section that also shows the running list 
so you can better keep track. This project also has added setpriority and get priority system calls which
can change or retrieve the priority of a process. Both control-p and ps have been updated to show the priority of a process. 

As it stands: each project has received a 98% or better. Project 4 has yet to be graded, but projects 2 and 3 have received 100% and
project 1 received a 98 due to the alignment of the header versus the information in control-p.

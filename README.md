# -Crash-Linux-Shell

I created a Struct to represent all processes, also known as 'jobs', that have been started but not completed or terminated. This Struct can keep track of the Process ID number, Process Number, the Command that created the Job, its status (whether running or suspended), and whether it is a Foreground Process or Background Process.

I also programmed signal handlers for SIGQUIT, SIGINT, SIGSTP, and SIGCHILD to appropriately handle signals sent from child processes or user input, such as suspending a running process.

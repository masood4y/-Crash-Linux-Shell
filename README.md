# Crash Linux Shell
(Detailed Breakdown on my Portfolio)

Shells play a pivotal role in enabling users to initiate new processes by entering commands along with their respective arguments. Additionally, they are responsible for capturing various signals, such as keyboard interrupts, and effectively redirecting them to the relevant processes.

I undertook the task of constructing a small shell known as "Crash," which served as a simplified version of the familiar Linux shell.

I created a Struct to represent all processes, also known as 'jobs', that have been started but not completed or terminated. This Struct can keep track of the Process ID number, Process Number, the Command that created the Job, its status (whether running or suspended), and whether it is a Foreground Process or Background Process.

I also programmed signal handlers for SIGQUIT, SIGINT, SIGSTP, and SIGCHILD to appropriately handle signals sent from child processes or user input, such as suspending a running process.

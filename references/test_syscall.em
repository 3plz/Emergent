--Module SyscallTest;

-[Init]
mod.add(std:sys);
mod.add(std:io);

// On Linux x86_64, syscall 1 is write and file descriptor 1 is stdout.
var bytes: int;
Syscall(1, 1, "Hello from direct Linux Syscall!\n", 33) >> bytes;
io.Print(bytes); // Prints the number of bytes written: 33.

// Read the process ID using syscall 39 (getpid).
var pid: int;
Syscall(39) >> pid;
io.Print(pid); // Prints a positive process ID.

// Invoke write through the sys.Call wrapper.
var bytes2: int;
sys.Call(1, 1, "Hello via sys.Call!\n", 20) >> bytes2;
io.Print(bytes2); // Prints the number of bytes written: 20.

// Read the process ID through the sys.GetPid wrapper.
var pid2: int;
sys.GetPid() >> pid2;
io.Print(pid2); // Prints the same process ID as pid.

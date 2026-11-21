--Module SyscallTest;

-[Init]
mod.add(std:sys);
mod.add(std:io);

// 1. Direct write syscall: SYS_write = 1, fd = 1 (stdout)
var bytes: int;
Syscall(1, 1, "Hello from direct Linux Syscall!\n", 33) >> bytes;
io.Print(bytes); // 33

// 2. Get process PID via SYS_getpid = 39
var pid: int;
Syscall(39) >> pid;
io.Print(pid); // process id > 0

// 3. Via sys.Call
var bytes2: int;
sys.Call(1, 1, "Hello via sys.Call!\n", 20) >> bytes2;
io.Print(bytes2); // 20

// 4. Via high-level submodule sys.GetPid
var pid2: int;
sys.GetPid() >> pid2;
io.Print(pid2); // identical to pid

--Module sys;

#[sys]
description: {Standard system interface module providing direct Linux kernel system calls (Syscall / sys.Call) and OS primitives. \
Calls the kernel with zero overhead according to System V ABI register conventions.
};

#[Call]
arguments: {num;int;Linux syscall number (e.g. 1 for SYS_write, 0 for SYS_read, 39 for SYS_getpid, 60 for SYS_exit)};
returns: {int;Kernel syscall return value (positive return or negative error code)};
description: {Executes a Linux kernel syscall with the given syscall number and up to 6 register arguments.
};

#[Syscall]
arguments: {num;int;Linux syscall number};
returns: {int;Kernel return code};
description: {Built-in direct system call invocation. Supports Data Flow: Syscall(num, ...) >> result.
};

#[Write]
arguments: {fd;int;Target file descriptor (1 for stdout, 2 for stderr)};
arguments: {buf;str;Data string buffer to write};
arguments: {count;int;Number of bytes to write};
returns: {int;Number of bytes successfully written};
description: {Convenience submodule for the SYS_write (1) kernel system call.
};
-[Write] {fd: int, buf: str, count: int}
var written: int;
Syscall(1, fd, buf, count) >> written;
return written;

#[GetPid]
returns: {int;Current process ID};
description: {Convenience submodule for the SYS_getpid (39) kernel system call.
};
-[GetPid]
var pid: int;
Syscall(39) >> pid;
return pid;

#[Exit]
arguments: {code;int;Process exit status code};
returns: {void;Terminates the calling process immediately via SYS_exit (60)};
description: {Immediate process termination via direct kernel syscall.
};
-[Exit] {code: int}
Syscall(60, code);

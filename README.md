# wMSR

A minimal Windows kernel driver that exposes x86 **Model-Specific Register (MSR)** read/write to userspace via IOCTLs, paired with a header-only userspace library for C99 and C++20.

CPUs with >64 cores are handled properly, and the device `\\.\msr` is accessible to **SYSTEM** and **Administrators** only, keeping the driver secure.

Inspired by the Linux `msr` kernel module [`arch/x86/kernel/msr.c`](https://github.com/torvalds/linux/blob/master/arch/x86/kernel/msr.c).

## Usage

### Download the latest release
Download `msr.sys` from the [latest release](../../releases/latest) and place it at `C:\msr.sys`.

### Load the `msr.sys` driver (from elevated command prompt)

> **Note:** Test-signed drivers require test signing mode to be enabled. This is a one-time step that requires a reboot.
```cmd
bcdedit /set testsigning on
```

Make sure to reboot, then create and start the driver service:
```cmd
sc create msr type= kernel binPath= C:\msr.sys
sc start msr
```

> ### Uninstallation

Stop and delete the driver service:
```cmd
sc stop msr
sc delete msr
```

Delete the `msr.sys` driver binary itself:
```cmd
del C:\msr.sys
```

Disable test signing mode:
```cmd
bcdedit /set testsigning off
```

and reboot.

## Userspace API

Drop `msr.hpp` into your project's include path and include it:

```c
#include <msr.hpp>
```

### C++20

```cpp
try {
    msr::device dev; // opens \\.\msr; throws std::system_error on failure

    // Read MSR 0x1A2 (MSR_TEMPERATURE_TARGET) on logical CPU 0
    msr::value value = dev.read(0, 0x1A2);

    // Write a 64 bit value to an MSR on logical CPU 0
    dev.write(0, 0x1A2, 0x3640000);

    // or write a split value to an MSR on logical CPU 0
    dev.write(0, 0x1A2, { .lo = 0x0640000, .hi = 0x3 });

} catch (std::exception const& error) {
    std::cerr << "Error: " << error.what();
}
```

### C99

```c
HANDLE dev = msr_open(); // opens \\.\msr; returns INVALID_HANDLE_VALUE on failure

// Read MSR 0x1A2 (MSR_TEMPERATURE_TARGET) on logical CPU 0
MSR_VALUE value;
msr_read(dev, 0, 0x1A2, &value);

// Write a value to an MSR on logical CPU 0
msr_write(dev, 0, 0x1A2, (MSR_VALUE) { .q = 0x3640000 });

// or write a split value to an MSR on logical CPU 0
msr_write(dev, 0, 0x1A2, (MSR_VALUE) { .lo = 0x0640000, .hi = 0x3 });
msr_close(dev);
```

`msr_read` and `msr_write` return `BOOL`. On failure, call `GetLastError()` for the error code.


## IOCTL interface

The driver exposes two IOCTLs over the `\\.\msr` device using **buffered I/O**.

| IOCTL | Code | Direction |
|-------|------|-----------|
| `IOCTL_READ_MSR` | `CTL_CODE(40000, 0x800, METHOD_BUFFERED, FILE_READ_ACCESS)` | In/Out |
| `IOCTL_WRITE_MSR` | `CTL_CODE(40000, 0x801, METHOD_BUFFERED, FILE_WRITE_ACCESS)` | In |

Both IOCTLs use `MSR_REQUEST` as the input (and output for reads) buffer:

```c
typedef struct _MSR_REQUEST {
    MSR_NO    msr_no;   // MSR register number (32-bit)
    MSR_CPU   cpu;      // Flat system-wide processor index (32-bit)
    MSR_VALUE val;      // Value: input for write, output for read (64-bit)
} MSR_REQUEST;
```


## Build `msr.sys`

Use the provided build script (Release x64 by default):

```cmd
scripts\build <configuration, optional> <platform, optional>
```

Or, build directly with MSBuild:

```cmd
msbuild driver\msr.vcxproj /p:Configuration=Release /p:Platform=x64
```

The driver is output at `driver\<platform>\<configuration>\msr.sys`


### Requirements

- Windows 10 x64 (or later)
- **Visual Studio** with the C++ desktop & WDK workload
- **Windows SDK** matching the installed VS version
- **Windows Driver Kit (WDK)** matching the installed SDK version

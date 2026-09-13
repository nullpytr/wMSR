#ifndef MSR_HPP
#define MSR_HPP

#ifdef MSR_HPP_KERNEL_DRIVER_MODE
#include <ntddk.h>
#else
#include <windows.h>
#endif

#define MSR_DEVICE_TYPE 40000
#define IOCTL_READ_MSR  CTL_CODE(MSR_DEVICE_TYPE, 0x800, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_WRITE_MSR CTL_CODE(MSR_DEVICE_TYPE, 0x801, METHOD_BUFFERED, FILE_WRITE_ACCESS)

#define MSR_NT_DEVICE_NAME      L"\\Device\\msr"
#define MSR_DOS_DEVICE_NAME     L"\\DosDevices\\msr"
#define MSR_WIN32_DEVICE_NAME   L"\\\\.\\msr"

#ifdef __cplusplus
namespace msr::detail { // C++ wraps the C API, see below
#endif

typedef unsigned __int32 MSR_DOUBLE;
typedef unsigned __int64 MSR_QUAD;
typedef unsigned __int32 MSR_NO;
typedef unsigned __int32 MSR_CPU;

#pragma warning(push)
#pragma warning(disable: 4201)
typedef struct _MSR_VALUE {
    union {
        struct {
            MSR_DOUBLE lo; // EAX
            MSR_DOUBLE hi; // EDX
        };
        MSR_QUAD q;
    };
} MSR_VALUE;
#pragma warning(pop)

typedef struct _MSR_REQUEST {
    MSR_NO    msr_no;
    MSR_CPU   cpu;
    MSR_VALUE val;
} MSR_REQUEST, *PMSR_REQUEST;

/* -- Userspace API -- */
#ifndef MSR_HPP_KERNEL_DRIVER_MODE

#ifdef __cplusplus
#define MSR_INLINE inline
#else
#define MSR_INLINE static inline
#endif

MSR_INLINE HANDLE msr_open(void) {
    return CreateFileW(
        /* [in] lpFileName            */ MSR_WIN32_DEVICE_NAME,
        /* [in] dwDesiredAccess       */ GENERIC_READ | GENERIC_WRITE,
        /* [in] dwShareMode           */ 0,
        /* [in] lpSecurityAttributes  */ NULL,
        /* [in] dwCreationDisposition */ OPEN_EXISTING,
        /* [in] dwFlagsAndAttributes  */ 0,
        /* [in] hTemplateFile         */ NULL
    );
}

MSR_INLINE void msr_close(HANDLE device) {
    CloseHandle(device);
}

MSR_INLINE BOOL msr_ioctl(HANDLE device, DWORD const control_code, PMSR_REQUEST request) {
    DWORD bytes_returned;
    return DeviceIoControl(
        /* [in ] hDevice          */ device,
        /* [in ] dwIoControlCode  */ control_code,
        /* [in ] lpInBuffer       */ request,
        /* [in ] nInBufferSize    */ sizeof(MSR_REQUEST),
        /* [out] lpOutBuffer      */ request,
        /* [in ] nOutBufferSize   */ sizeof(MSR_REQUEST),
        /* [out] lpBytesReturned  */ &bytes_returned,
        /* [in ] lpOverlapped     */ NULL
    );
}

MSR_INLINE BOOL msr_read(HANDLE device, MSR_CPU cpu, MSR_NO reg, MSR_QUAD *value) {
    MSR_REQUEST request = {
        .msr_no = reg,
        .cpu = cpu
    };
    BOOL result = msr_ioctl(device, IOCTL_READ_MSR, &request);
    if (result) *value = request.val.q;
    return result;
}

MSR_INLINE BOOL msr_write(HANDLE device, MSR_CPU cpu, MSR_NO reg, MSR_QUAD value) {
    MSR_REQUEST request = { 
        .msr_no = reg,
        .cpu = cpu,
        .val = { .q = value }
    };
    return msr_ioctl(device, IOCTL_WRITE_MSR, &request);
}
#undef MSR_INLINE
#endif // !MSR_HPP_KERNEL_DRIVER_MODE (USERSPACE - C/++)

#ifdef __cplusplus
} // namespace msr::detail

#ifndef MSR_HPP_KERNEL_DRIVER_MODE
#include <utility>
#include <cstdint>
#include <system_error>

namespace msr {

using u32 = std::uint32_t;
using u64 = std::uint64_t;

class device {
public:
    device() {
        m_handle = detail::msr_open();
            
        if (m_handle == INVALID_HANDLE_VALUE)
            error("Failed to open MSR device");
    }

    ~device() {
        if (m_handle != INVALID_HANDLE_VALUE)
            detail::msr_close(m_handle);
    }

    device(device const&) = delete;
    device& operator=(device const&) = delete;

    device(device&& other) noexcept : m_handle(std::exchange(other.m_handle, INVALID_HANDLE_VALUE)) {}

    device& operator=(device&& other) noexcept {
        if (this != &other) {
            if (m_handle != INVALID_HANDLE_VALUE)
                detail::msr_close(m_handle);

            m_handle = std::exchange(other.m_handle, INVALID_HANDLE_VALUE);
        }
        return *this;
    }

    u64 read(u32 const cpu, u32 const reg) const {
        u64 value;
        if (!detail::msr_read(m_handle, cpu, reg, &value))
            error("IOCTL_READ_MSR failed");

        return value;
    }

    void write(u32 const cpu, u32 const reg, u64 const value) const {
        if (!detail::msr_write(m_handle, cpu, reg, value))
            error("IOCTL_WRITE_MSR failed");
    }

private:
    /* Helpers */
    [[noreturn]] void error(char const* message) const {
        throw std::system_error(GetLastError(), std::system_category(), message);
    }

    /* Members */
    HANDLE m_handle;
};

} // namespace msr

#endif // !MSR_HPP_KERNEL_DRIVER_MODE (USERSPACE - C++)
#endif // __cplusplus

#endif // MSR_HPP

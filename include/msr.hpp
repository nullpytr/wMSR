#ifndef MSR_HPP
#define MSR_HPP

/* config */
#define CONFIG_CPP defined(__cplusplus)
#define CONFIG_USERSPACE !defined(MSR_HPP_KERNEL_DRIVER_MODE)

#define CONFIG_DETAIL_OPEN()
#define CONFIG_DETAIL_CLOSE()

#if CONFIG_USERSPACE
#include <windows.h>

#if CONFIG_CPP
#define CONFIG_INLINE inline

#undef CONFIG_DETAIL_OPEN
#undef CONFIG_DETAIL_CLOSE
#define CONFIG_DETAIL_OPEN() namespace msr::detail { // C++ wraps the C userspace API, see below
#define CONFIG_DETAIL_CLOSE() } // namespace msr::detail

#else
#define CONFIG_INLINE static inline
#endif // CONFIG_CPP

#else // MSR_HPP_KERNEL_DRIVER_MODE
#include <ntddk.h>
#endif // CONFIG_USERSPACE
/* -- */


#define MSR_DEVICE_TYPE 40000
#define IOCTL_READ_MSR  CTL_CODE(MSR_DEVICE_TYPE, 0x800, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_WRITE_MSR CTL_CODE(MSR_DEVICE_TYPE, 0x801, METHOD_BUFFERED, FILE_WRITE_ACCESS)

#define MSR_NT_DEVICE_NAME      L"\\Device\\msr"
#define MSR_DOS_DEVICE_NAME     L"\\DosDevices\\msr"
#define MSR_WIN32_DEVICE_NAME   L"\\\\.\\msr"

CONFIG_DETAIL_OPEN()

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
#if CONFIG_USERSPACE

CONFIG_INLINE HANDLE msr_open(void) {
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

CONFIG_INLINE void msr_close(HANDLE device) {
    CloseHandle(device);
}

CONFIG_INLINE BOOL msr_ioctl(HANDLE device, DWORD const control_code, PMSR_REQUEST request) {
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

CONFIG_INLINE BOOL msr_read(HANDLE device, MSR_CPU cpu, MSR_NO reg, MSR_VALUE *value) {
    MSR_REQUEST request = {
        .msr_no = reg,
        .cpu = cpu
    };
    BOOL success = msr_ioctl(device, IOCTL_READ_MSR, &request);
    if (success) *value = request.val;
    return success;
}

CONFIG_INLINE BOOL msr_write(HANDLE device, MSR_CPU cpu, MSR_NO reg, MSR_VALUE value) {
    MSR_REQUEST request = { 
        .msr_no = reg,
        .cpu = cpu,
        .val = value
    };
    return msr_ioctl(device, IOCTL_WRITE_MSR, &request);
}

#undef CONFIG_INLINE
CONFIG_DETAIL_CLOSE()

/* -- C++ Userspace API -- */
#if CONFIG_CPP

#include <utility>
#include <cstdint>
#include <system_error>

namespace msr {

using u32 = std::uint32_t;
using u64 = std::uint64_t;
using handle_t = HANDLE;
using ioctl_t = DWORD;

using value = detail::MSR_VALUE;
using request = detail::MSR_REQUEST;

struct ioctl {
    static constexpr auto read       { ioctl_t(IOCTL_READ_MSR) };
    static constexpr auto write      { ioctl_t(IOCTL_WRITE_MSR) };
};

class device {
public:
    static constexpr auto type       { MSR_DEVICE_TYPE };

    struct name {
        static constexpr auto nt     { MSR_NT_DEVICE_NAME };
        static constexpr auto dos    { MSR_DOS_DEVICE_NAME };
        static constexpr auto win32  { MSR_WIN32_DEVICE_NAME };
    };

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

    msr::value read(u32 const cpu, u32 const reg) const {
        msr::value val;
        if (!detail::msr_read(m_handle, cpu, reg, &val))
            error("IOCTL_READ_MSR failed");

        return val;
    }

    void write(u32 const cpu, u32 const reg, msr::value const val) const {
        if (!detail::msr_write(m_handle, cpu, reg, val))
            error("IOCTL_WRITE_MSR failed");
    }

    void write(u32 const cpu, u32 const reg, u64 const val) const {
        write(cpu, reg, { .q = val });
    }

    bool ioctl(msr::ioctl_t control_code, request& req) const {
        return detail::msr_ioctl(m_handle, control_code, &req);
    }

    handle_t const& handle() const {
        return m_handle;
    }
    
private:
    /* Helpers */
    [[noreturn]] void error(char const* message) const {
        throw std::system_error(GetLastError(), std::system_category(), message);
    }

    /* Members */
    handle_t m_handle;
};

} // namespace msr

#undef IOCTL_READ_MSR
#undef IOCTL_WRITE_MSR
#undef MSR_DEVICE_TYPE
#undef MSR_NT_DEVICE_NAME
#undef MSR_DOS_DEVICE_NAME
#undef MSR_WIN32_DEVICE_NAME
#endif // CONFIG_CPP

#endif // CONFIG_USERSPACE

/* undef config */
#undef CONFIG_CPP
#undef CONFIG_USERSPACE
#undef CONFIG_DETAIL_OPEN
#undef CONFIG_DETAIL_CLOSE
/* -- */

#endif // MSR_HPP

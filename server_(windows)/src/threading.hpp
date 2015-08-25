#pragma once

#include <windows.h>

namespace taosocks {
    class locker_t {
    public:
        locker_t() {
            ::InitializeCriticalSection(&_cs);
        }

        ~locker_t() {
            ::DeleteCriticalSection(&_cs);
        }

        void lock() {
            return ::EnterCriticalSection(&_cs);
        }

        void unlock() {
            return ::LeaveCriticalSection(&_cs);
        }

        bool try_lock() {
            return !!::TryEnterCriticalSection(&_cs);
        }

    protected:
        CRITICAL_SECTION _cs;
    };

    class event_t {
    public:
        event_t() {

        }

        ~event_t() {

        }

        int init(bool manual, bool initial) {
            _handle = ::CreateEvent(nullptr, manual, initial, nullptr);
            return _handle != nullptr;
        }

        int uninit() {
            if (::CloseHandle(_handle)){
                _handle = nullptr;
                return 0;
            }

            return -1;
        }

        int set() {
            return (int)!!::SetEvent(_handle) - 1;
        }

        int reset() {
            return (int)!!::ResetEvent(_handle) - 1;
        }

        int wait(int timeout = INFINITE) {
            int r = ::WaitForSingleObject(_handle, timeout);

            return (r == WAIT_OBJECT_0+0) - 1;
        }
    protected:
        HANDLE _handle;
    };
}

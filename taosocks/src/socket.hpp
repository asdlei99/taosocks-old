#pragma once

#include <stdint.h>

#include <queue>

#include <WinSock2.h>
#include <WS2tcpip.h>
#include <windows.h>

#include "threading.hpp"

namespace taosocks {
    class win_sock {
    public:
        win_sock() {
            WSADATA _wsa;
            ::WSAStartup(MAKEWORD(2, 2), &_wsa);
        }

        ~win_sock() {
            ::WSACleanup();
        }
    };

    struct client_t {
        in_addr     addr;
        uint16_t    port;
        SOCKET      fd;
    };

    class socket_server {
    public:
        socket_server(const char* addr, uint16_t port, uint16_t backlog) {
            _addr.S_un.S_addr = ::inet_addr(addr);
            _port = port;
            _backlog = backlog;
        }

        ~socket_server() {

        }

    public:
        bool start() {
            _fd = ::socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (_fd == -1) return false;

            sockaddr_in server_addr = {0};
            server_addr.sin_family  = AF_INET;
            server_addr.sin_addr    = _addr;
            server_addr.sin_port    = htons(_port);

            if (::bind(_fd, (sockaddr*)&server_addr, sizeof(server_addr)) == -1)
                return false;

            if (::listen(_fd, _backlog) == -1)
                return false;

            return true;
        }

        bool accept(client_t* c) {
            SOCKET fd;
            sockaddr_in addr;
            int len = sizeof(addr);

            fd = ::accept(_fd, (sockaddr*)&addr, &len);
            if (fd != -1) {
                c->addr = addr.sin_addr;
                c->port = ntohs(addr.sin_port);
                c->fd   = fd;

                return true;
            }

            return false;
        }

    protected:
        SOCKET      _fd;
    protected:
        in_addr     _addr;
        uint16_t    _port;
        uint16_t    _backlog;
    };

    std::string resolve(const char* host) {
        struct addrinfo hints;
        struct addrinfo* pres = nullptr;
        int res;

        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;

        res = ::getaddrinfo(host, nullptr, &hints, &pres);
        if (res != 0)
            throw "resolve error.";

        return ::inet_ntoa(((sockaddr_in*)pres->ai_addr)->sin_addr);
    }

    class socket_client {
    public:
        socket_client(const char* ip, uint16_t port) {
            _addr.S_un.S_addr = ::inet_addr(ip);
            _port = port;
        }

        ~socket_client() {

        }

    public:
        bool connect() {
            _fd = ::socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (_fd == -1) return false;

            sockaddr_in server_addr = { 0 };
            server_addr.sin_family = AF_INET;
            server_addr.sin_addr = _addr;
            server_addr.sin_port = htons(_port);

            if (::connect(_fd, (sockaddr*)&server_addr, sizeof(server_addr)) == -1)
                throw "connect error.";

            return true;
        }

    protected:

    public:
        SOCKET      _fd;
        in_addr     _addr;
        uint16_t    _port;
    };

    class client_queue {
    public:
        client_queue() {
            _h_event = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);
        }

        ~client_queue() {
            ::CloseHandle(_h_event);
            _h_event = nullptr;
        }

    public:
        void push(taosocks::client_t& client) {
            _client_queue.push(client);
            ::SetEvent(_h_event);
            return;
        }

        taosocks::client_t& pop() {
            taosocks::client_t* p = nullptr;

            for (;;) {
                if (!size()) {
                    ::WaitForSingleObject(_h_event, INFINITE);
                }

                if (!_lock.try_lock())
                    continue;

                if (!size()) {
                    _lock.unlock();
                    continue;
                }

                p = &_client_queue.front();
                _client_queue.pop();

                _lock.unlock();

                break;
            }

            return *p;
        }

        int size() {
            return _client_queue.size();
        }

    protected:
        taosocks::locker                _lock;
        std::queue<taosocks::client_t>  _client_queue;
        HANDLE                          _h_event;
    };
}

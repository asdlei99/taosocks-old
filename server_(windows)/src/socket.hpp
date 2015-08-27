#pragma once

#include <stdint.h>

#include <vector>
#include <queue>

#include <WinSock2.h>
#include <WS2tcpip.h>
#include <windows.h>

#include "threading.hpp"

namespace taosocks {
    /* WinSock 初始化 */
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

    /* 域名解析 */
    class resolver_t {
    public:
        int resolve(const char* host) {
            struct addrinfo hints;
            struct addrinfo* pres = nullptr;

            memset(&hints, 0, sizeof(hints));
            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_STREAM;
            hints.ai_protocol = IPPROTO_TCP;

            if (::getaddrinfo(host, nullptr, &hints, &pres) == 0) {
                _addrs.clear();
                while (pres) {
                    _addrs.push_back(reinterpret_cast<sockaddr_in*>(pres->ai_addr)->sin_addr);
                    pres = pres->ai_next;
                }

                return (int)_addrs.size();
            }

            return -1;
        }

        int size() {
            return (int)_addrs.size();
        }

        /* 把 in_addr 转化成点分十进制形式 */
        std::string to_string(int i) {
            if (i < 0 || i>size()-1) {
                return "0.0.0.0";
            }

            return ::inet_ntoa(_addrs[i]);
        }

    protected:
        std::vector<in_addr>    _addrs;
    };


    struct client_t {
        in_addr     addr;
        uint16_t    port;
        SOCKET      fd;
    };

    class socket_server_t {
    public:
        socket_server_t() {

        }

        ~socket_server_t() {

        }

    public:
        int start(const char* ip, uint16_t port, uint16_t backlog) {
            _addr.S_un.S_addr   = ::inet_addr(ip);
            _port               = port;
            _backlog            = backlog;

            _fd = ::socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (_fd == -1) return -1;

            sockaddr_in server_addr = {0};
            server_addr.sin_family  = AF_INET;
            server_addr.sin_addr    = _addr;
            server_addr.sin_port    = htons(_port);

            if (::bind(_fd, (sockaddr*)&server_addr, sizeof(server_addr)) == -1)
                return -1;

            if (::listen(_fd, _backlog) == -1)
                return -1;

            return 0;
        }

        int accept(client_t* c) {
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

        int send(client_t& c, const uint8_t* data, int len) {
            int sent = 0;

            while (sent != len) {
                int n = ::send(c.fd, (char*)data+sent, len - sent, 0);

                // send error, or graceful shutdown
                if (n <= 0) return n;

                sent += n;
            }

            return sent;
        }

        int recv(client_t& c, uint8_t* data, int len) {
            int rcvd = 0;

            while (rcvd != len) {
                int n = ::recv(c.fd, (char*)data + rcvd, len - rcvd, 0);

                if (n <= 0) return n;

                rcvd += n;
            }

            return rcvd;
        }

        SOCKET fd() const {
            return _fd;
        }

    protected:
        SOCKET      _fd;
        in_addr     _addr;
        uint16_t    _port;
        uint16_t    _backlog;
    };

    class socket_client_t {
    public:
        socket_client_t() {

        }

        ~socket_client_t() {

        }

    public:
        int connect(const char* ip, uint16_t port) {
            _addr.S_un.S_addr   = ::inet_addr(ip);
            _port               = port;

            _fd = ::socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (_fd == -1) return -1;

            sockaddr_in server_addr = { 0 };
            server_addr.sin_family  = AF_INET;
            server_addr.sin_addr    = _addr;
            server_addr.sin_port    = ::htons(_port);

            if (::connect(_fd, (sockaddr*)&server_addr, sizeof(server_addr)) == -1)
                return -1;

            return 0;
        }

        int send(const uint8_t* data, int len) {
            int sent = 0;

            while (sent != len) {
                int n = ::send(_fd, (char*)data+sent, len - sent, 0);

                // send error, or graceful shutdown
                if (n <= 0) return n;

                sent += n;
            }

            return sent;
        }

        int recv(uint8_t* data, int len) {
            int rcvd = 0;

            while (rcvd != len) {
                int n = ::recv(_fd, (char*)data + rcvd, len - rcvd, 0);

                if (n <= 0) return n;

                rcvd += n;
            }

            return rcvd;
        }

        SOCKET fd() const {
            return _fd;
        }

    protected:
        SOCKET      _fd;
        in_addr     _addr;
        uint16_t    _port;
    };

    class client_queue {
    public:
        client_queue() {
            _event.init(false, false);
        }

        ~client_queue() {
            _event.uninit();
        }

    public:
        void push(taosocks::client_t client) {
            _client_queue.push(client);
            _event.set();
            return;
        }

        taosocks::client_t pop() {
            taosocks::client_t* p = nullptr;

            for (;;) {
                if (!size()) {
                    _event.wait();
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
        taosocks::locker_t              _lock;
        std::queue<taosocks::client_t>  _client_queue;
        event_t                         _event;
    };
}

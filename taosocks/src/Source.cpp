#include <stdint.h>
#include <cassert>
#include <cstdio>

#include <fstream>
#include <iostream>
#include <string>

#include <process.h>

#include "socket.hpp"

namespace taosocks {
    enum class auth_methods {
        none            = 0x00,
        gssapi          = 0x01,
        login           = 0x02,
        iana_start      = 0x03,
        iana_end        = 0x7F,
        private_start   = 0x80,
        private_end     = 0xFE,
        unacceptable    = 0xFF,
    };

    enum class socks_version {
        v4  = 0x04,
        v4a = v4,
        v5  = 0x05,
    };

    enum class socks_command {
        tcp_stream  = 0x01,
        tcp_binding = 0x02,
        udp_port    = 0x03,    
    };

    enum class addr_type {
        ipv4    = 0x01,
        domain  = 0x03,
        ipv6    = 0x04,
    };

    enum class request_status {
        granted                     = 0x00,
        failure                     = 0x01,
        not_allowed                 = 0x02,
        network_unreadchable        = 0x03,
        host_unreachable            = 0x04,
        refused_by_host             = 0x05,
        ttl_expired                 = 0x06,
        command_not_supported       = 0x07,
        address_type_unsupported    = 0x08,
    };

    class socks_server {
    public:
        socks_server(client_t& client)
            : _client(client)
            , _socket_client(nullptr)
        {
            //DWORD tid = ::GetCurrentThreadId();
            //int cfd = client.fd;

            //char buf[128];
            //sprintf(buf, "%d-%d.txt", tid, cfd);
            //_data.open(buf, std::ios::binary);
        }

        ~socks_server()
        {
            //_data.close();
            delete _socket_client;
        }

    public:
        void run() {
            try{
                ::printf("thread_id: %10d, client fd: %10d\n", ::GetCurrentThreadId(), _client.fd);
                auth() && request() && respond();
            }
            catch (const char* err){

            }
            ::closesocket(_client.fd);
            if (_socket_client) {
                ::closesocket(_socket_client->_fd);
            }
        }

    protected:
        bool auth() {
            uint8_t c;

            // incoming
            c = read_byte();
            if (c != (uint8_t)socks_version::v5) {
                uint8_t d[8];
                for (int i = 0; i < sizeof(d); i++){
                    d[i] = read_byte();
                }
                assert(0);
            }

            c = read_byte();
            assert(c >= 1);

            uint8_t* p = new uint8_t[c];
            read(_client.fd, p, c);

            //c = read_byte();
            //assert(c == (uint8_t)auth_methods::none);

            // outgoing
            write_byte((uint8_t)socks_version::v5);

            write_byte((uint8_t)auth_methods::none);

            return true;
        }

        bool request() {
            uint8_t c;

            c = read_byte();
            assert(c == (uint8_t)socks_version::v5);

            c = read_byte();
            if (c != (uint8_t)socks_command::tcp_stream) {
                assert(0);
            }

            c = read_byte();
            assert(c == 0);

            c = read_byte();
            //assert(c == (uint8_t)addr_type::domain);

            std::string dom;

            if (c == (uint8_t)addr_type::domain) {
                c = read_byte();
                char* pdomain = new char[c];
                if (read(_client.fd, (uint8_t*)pdomain, c) != c)
                    throw "request error.";

                std::string domain(pdomain, c);
                delete[] pdomain;

                dom = domain;
            }
            else if (c == (uint8_t)addr_type::ipv4) {
                uint8_t ips[4];
                read(_client.fd, ips, 4);

                char buf[128];
                sprintf(buf, "%d.%d.%d.%d", ips[0], ips[1], ips[2], ips[3]);

                dom = buf;
            }

            uint16_t net_port;
            if (read(_client.fd, (uint8_t*)&net_port, 2) != 2)
                throw "request error.";

            _domain = dom;
            _port = ntohs(net_port);

            std::string ip = resolve(dom.c_str());
            in_addr addr;
            addr.S_un.S_addr = ::inet_addr(ip.c_str());

            _socket_client = new socket_client(ip.c_str(), _port);
            _socket_client->connect();

            write_byte((uint8_t)socks_version::v5);
            write_byte((uint8_t)request_status::granted);
            write_byte(0);
            write_byte((uint8_t)addr_type::ipv4);

            write(_client.fd, (uint8_t*)&addr.S_un.S_addr, 4);
            write(_client.fd, (uint8_t*)&net_port, 2);

            return true;
        }

        bool respond() {
            SOCKET cfd = _client.fd;
            SOCKET sfd = _socket_client->_fd;

            int r;

            int n;
            for (;;) {
                fd_set rfds;
                FD_ZERO(&rfds);

                FD_SET(sfd, &rfds);
                FD_SET(cfd, &rfds);

                n = ::select(-1, &rfds, nullptr, nullptr, nullptr);
                if (n== -1) break;
                else if (n == 0) continue;

                //std::cout << "select returns " << n;

                uint8_t buf[10240];
                u_long count;

                if (FD_ISSET(cfd, &rfds)) {
                    r = ::ioctlsocket(cfd, FIONREAD, &count);
                    if (r == -1) {
                        //std::cout << ", ioctlsocket returns -1, WSAGetLastERror() == " << ::WSAGetLastError();
                        break;
                    }
                    assert(r == 0);

                    count = min(sizeof(buf), count);

                    //if (count > 0)
                    //std::cout << ", fdset set: cfd, bytes: " << count;

                    if (count == 0) {
                        ::Sleep(500);
                        break;
                        continue;
                    }

                    r = read(cfd, buf, count);
                    if (r == 0 || r == -1) break;
                    assert(write(sfd, buf, count) == count);
                }

                if (FD_ISSET(sfd, &rfds)) {
                    r = ::ioctlsocket(sfd, FIONREAD, &count);
                    if (r == -1) {
                        //std::cout << ", ioctlsocket returns -1, WSAGetLastERror() == " << ::WSAGetLastError();
                        break;
                    }

                    assert(r == 0);

                    count = min(sizeof(buf), count);

                    //if (count > 0)
                    //std::cout << ", fdset set: sfd, bytes: " << count;

                    if (count == 0) {
                        ::Sleep(500);
                        break;
                        continue;
                    }

                    r = read(sfd, buf, count);
                    if (r == 0 || r == -1) break;
                    assert(write(cfd, buf, count) == count);
                }
            }
            
            ::closesocket(cfd);
            ::closesocket(sfd);

            return true;
        }

        void out(bool r, uint8_t* a, int b) {
            return;
            if (b <= 0) return;
            char buf[4];
            //_data << (r ? "RD: " : "WR: ");
            for (int i = 0; i < b; i++){
                sprintf(buf, "%02X ", a[i]);
                //_data << buf;
            }
            //_data << "\r\n";
        }

        uint8_t read_byte() {
            uint8_t c;
            if (::recv(_client.fd, (char*)&c, 1, 0) == 1) {
                out(true, &c, 1);
                return c;
            }
            else{
                int r = ::WSAGetLastError();
                //assert(0);
                throw "read_byte error.";
            }
        }

        bool write_byte(uint8_t c) {
            if (::send(_client.fd, (char*)&c, 1, 0) == 1) {
                out(false, &c, 1);
                return true;
            }
            else
                throw "write_byte error.";
        }

        int read(SOCKET fd, uint8_t* buf, int len) {
            int n = ::recv(fd, (char*)buf, len, 0);
            out(true, buf, n);
            return n;
        }

        int write(SOCKET fd, uint8_t* buf, int len) {
            int n = ::send(fd, (char*)buf, len, 0);
            out(false, buf, len);
            return n;
        }
    protected:
        //std::ofstream   _data;
        socket_client*  _socket_client;
        client_t&       _client;
        std::string     _domain;
        uint16_t        _port;
    };
}

unsigned int __stdcall worker_thread(void* ud) {
    auto& queue = *reinterpret_cast<taosocks::client_queue*>(ud);
    taosocks::client_t* c = nullptr;

    while (c = &queue.pop()) {
        taosocks::socks_server server(*c);
        server.run();
    }

    return 0;
}

void create_worker_threads(taosocks::client_queue& queue) {
    SYSTEM_INFO si;
    ::GetSystemInfo(&si);

    DWORD n_threads = si.dwNumberOfProcessors * 10;

    for (int i = 0; i < (int)n_threads; i++){
        _beginthreadex(nullptr, 0, worker_thread, &queue, 0, nullptr);
        printf("thread %d created.\n", i+1);
    }
}

int main() {
    const int mt_mode = 1;

    taosocks::win_sock wsa;

    taosocks::client_queue queue;
    taosocks::socket_server server("127.0.0.1", 1080, 128);
    server.start();

    create_worker_threads(queue);

    taosocks::client_t client;
    while (server.accept(&client)) {
        //std::cout << "accepted...\n";
        if (mt_mode) {
            queue.push(client);
        }
        else {
            taosocks::socks_server server(client);
            server.run();
        }
    }

    return 0;
}

#include <stdint.h>
#include <cassert>
#include <cstdio>

#include <fstream>
#include <iostream>
#include <string>

#include <process.h>

#include "socket.hpp"

unsigned int __stdcall worker_thread(void* ud) {
    auto& queue = *reinterpret_cast<taosocks::client_queue*>(ud);
    taosocks::client_t c = queue.pop();


    return 0;
}

void create_worker_threads(taosocks::client_queue& queue) {
    SYSTEM_INFO si;
    ::GetSystemInfo(&si);

    DWORD n_threads = si.dwNumberOfProcessors * 10;

    // set to 1
    n_threads = 1;

    for (int i = 0; i < (int)n_threads; i++){
        _beginthreadex(nullptr, 0, worker_thread, &queue, 0, nullptr);
        printf("thread %d created.\n", i+1);
    }
}

int main() {
    taosocks::win_sock wsa;

    taosocks::client_queue queue;
    taosocks::socket_server_t server;
    server.start("127.0.0.1", 1080, 128);

    create_worker_threads(queue);

    taosocks::client_t client;
    while (server.accept(&client)) {
        queue.push(client);
    }

    return 0;
}

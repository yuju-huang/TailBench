/** $lic$
 * Copyright (C) 2016-2017 by Massachusetts Institute of Technology
 *
 * This file is part of TailBench.
 *
 * If you use this software in your research, we request that you reference the
 * TaiBench paper ("TailBench: A Benchmark Suite and Evaluation Methodology for
 * Latency-Critical Applications", Kasture and Sanchez, IISWC-2016) as the
 * source in any publications that use this software, and that you send us a
 * citation of your work.
 *
 * TailBench is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.
 */

#include "client.h"
#include "helpers.h"

#include <assert.h>
#include <pthread.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <iostream>
#include <string>
#include <vector>

#ifdef CLOSED_LOOP
static bool _send(NetworkedClient* client) {
    Request* req = client->startReq();
    if (!client->send(req)) {
        std::cerr << "[CLIENT] send() failed : " << client->errmsg() \
            << std::endl;
        std::cerr << "[CLIENT] Not sending further request" << std::endl;
        client->dumpStats();
        syscall(SYS_exit_group, 0);
        return false;
    }
    return true;
}

static bool _recv(NetworkedClient* client) {
    Response resp;
    if (!client->recv(&resp)) {
        std::cerr << "[CLIENT] recv() failed : " << client->errmsg() \
            << std::endl;
        client->dumpStats();
        syscall(SYS_exit_group, 0);
        return false;
    }

    if (resp.type == RESPONSE) {
        client->finiReq(&resp);
    } else if (resp.type == ROI_BEGIN) {
        client->startRoi();
    } else if (resp.type == FINISH) {
        client->dumpStats();
        syscall(SYS_exit_group, 0);
    } else {
        std::cerr << "Unknown response type: " << resp.type << std::endl;
        return false;
    }
    return true;
}

void* closed_loop(void* c) {
    NetworkedClient* client = reinterpret_cast<NetworkedClient*>(c);
    while (true) {
        if (!_send(client)) break;
        if (!_recv(client)) break;
    }
}
#endif

void* send(void* c) {
    NetworkedClient* client = reinterpret_cast<NetworkedClient*>(c);
    while (true) {
        Request* req = client->startReq();
        if (!client->send(req)) {
            std::cerr << "[CLIENT] send() failed : " << client->errmsg() \
                << std::endl;
            std::cerr << "[CLIENT] Not sending further request" << std::endl;
            client->dumpStats();
            syscall(SYS_exit_group, 0);
            break; // We are done
        }
    }
    return nullptr;
}

void* recv(void* c) {
    NetworkedClient* client = reinterpret_cast<NetworkedClient*>(c);

    Response resp;
    while (true) {
        if (!client->recv(&resp)) {
            std::cerr << "[CLIENT] recv() failed : " << client->errmsg() \
                << std::endl;
            return nullptr;
        }

        if (resp.type == RESPONSE) {
            client->finiReq(&resp);
        } else if (resp.type == ROI_BEGIN) {
            client->startRoi();
        } else if (resp.type == FINISH) {
            client->dumpStats();
            syscall(SYS_exit_group, 0);
        } else {
            std::cerr << "Unknown response type: " << resp.type << std::endl;
            return nullptr;
        }
    }
}

int sleepInSec;
void* dump(void* c) {
    NetworkedClient* client = reinterpret_cast<NetworkedClient*>(c);

    while (true) {
        sleep(sleepInSec);
        client->dumpAndClearStats();
    }
}

int main(int argc, char* argv[]) {
    int nthreads = getOpt<int>("TBENCH_CLIENT_THREADS", 1);
    std::string server = getOpt<std::string>("TBENCH_SERVER", "");
    int serverport = getOpt<int>("TBENCH_SERVER_PORT", 8080);
    sleepInSec = getOpt<int>("TBENCH_MEASURE_SLEEP_SEC", 5);

    NetworkedClient* client = new NetworkedClient(nthreads, server, serverport);

#ifdef CLOSED_LOOP
    std::vector<pthread_t> clients(nthreads);
    for (int t = 0; t < nthreads; ++t) {
        int status = pthread_create(&clients[t], nullptr, closed_loop,
                reinterpret_cast<void*>(client));
        assert(status == 0);
    }

    // Thread to periodically dump stats.
    pthread_t dumper;
    assert(pthread_create(&dumper, nullptr, dump,
                          reinterpret_cast<void*>(client)) == 0);

    for (int t = 0; t < nthreads; ++t) {
        int status;
        status = pthread_join(clients[t], nullptr);
        assert(status == 0);
    }
    assert(pthread_join(dumper, nullptr) == 0);
#else
    std::vector<pthread_t> senders(nthreads);
    std::vector<pthread_t> receivers(nthreads);

    for (int t = 0; t < nthreads; ++t) {
        int status = pthread_create(&senders[t], nullptr, send, 
                reinterpret_cast<void*>(client));
        assert(status == 0);
    }

    for (int t = 0; t < nthreads; ++t) {
        int status = pthread_create(&receivers[t], nullptr, recv, 
                reinterpret_cast<void*>(client));
        assert(status == 0);
    }

    for (int t = 0; t < nthreads; ++t) {
        int status;
        status = pthread_join(senders[t], nullptr);
        assert(status == 0);
        status = pthread_join(receiver[t], nullptr);
        assert(status == 0);
    }
#endif

    return 0;
}

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
#include "msgq.h"
#include "helpers.h"

#include <assert.h>
#include <pthread.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <boost/algorithm/string.hpp>

#define RL 1

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

std::string workloadDec;
void* wrk(void* c) {
    assert(!workloadDec.empty());
    NetworkedClient* client = reinterpret_cast<NetworkedClient*>(c);

    std::ifstream fd(workloadDec, std::ifstream::in);
    std::string line;
    while (std::getline(fd, line)) {
        if (fd.fail()) {
            std::cerr << "Reading workload description file error\n";
            client->dumpStats();
            syscall(SYS_exit_group, 0);
            return nullptr;
        }

        // Ignore comments;
        if (line[0] == '#') continue;

        // Parse workload in format QPS, TIME_IN_SEC
        std::vector<std::string> strs;
        boost::split(strs, line, boost::is_any_of(","));
        assert(strs.size() == 2);
        const int qps = std::stoi(strs[0]);
        const int time = std::stoi(strs[1]);
        std::cout << "qps=" << qps << ", time=" << time << std::endl;
        client->updateQps(qps);
        sleep(time);
    }

    std::cout << "Finish all workload in description file, exit\n";
    client->dumpStats();
    syscall(SYS_exit_group, 0);
    return nullptr;
}

int sleepInSec;
void* dump(void* c) {
    NetworkedClient* client = reinterpret_cast<NetworkedClient*>(c);

#ifdef RL
    int mqid = mq_init();
    assert(mqid >= 0);
#endif

    while (true) {
#ifdef RL
        struct mq_msgbuf recv_buf;
        struct mq_msgbuf snd_buf;
        lats_t* lats = &snd_buf.data.lats;
        snd_buf.type = CMD_PUT_LAT;

        mq_recv(mqid, &recv_buf);
        if (recv_buf.type == CMD_GET_LAT) {
            // May in the warmup stage
            while (client->getAndClearStats(*lats) == false) {
                sleep(1);
            }
            mq_send(mqid, &snd_buf);
        } else {
            assert(false);
        }
#else
        sleep(sleepInSec);
        client->dumpAndClearStats();
#endif
    }
}

int main(int argc, char* argv[]) {
    int nthreads = getOpt<int>("TBENCH_CLIENT_THREADS", 1);
    std::string server = getOpt<std::string>("TBENCH_SERVER", "");
    int serverport = getOpt<int>("TBENCH_SERVER_PORT", 8080);
    sleepInSec = getOpt<int>("TBENCH_MEASURE_SLEEP_SEC", 5);
    workloadDec = getOpt<std::string>("TBENCH_WORKLOAD_DEC", "");

    NetworkedClient* client = new NetworkedClient(nthreads, server, serverport);

#ifdef CLOSED_LOOP
    std::vector<pthread_t> clients(nthreads);
    for (int t = 0; t < nthreads; ++t) {
        int status = pthread_create(&clients[t], nullptr, closed_loop,
                reinterpret_cast<void*>(client));
        assert(status == 0);
    }

    // Thread to change workload according to workloadDec file.
    pthread_t wrker;
    if (!workloadDec.empty()) {
        assert(pthread_create(&wrker, nullptr, wrk,
                              reinterpret_cast<void*>(client)) == 0);
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
    assert(pthread_join(wrker, nullptr) == 0);
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
        status = pthread_join(receivers[t], nullptr);
        assert(status == 0);
    }
#endif

    return 0;
}

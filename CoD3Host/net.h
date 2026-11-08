#pragma once

// The network's state, for the settings menu's Multiplayer tab
// (kernel_net.cpp).

#include <string>
#include <vector>

namespace Net
{
    struct Summary
    {
        std::string address;               // what System Link gives out, and why
        std::vector<std::string> adapters; // one line each
        std::vector<std::string> peers;
        unsigned sockets = 0;
        unsigned long long sent = 0, received = 0;
        std::string lastFrom;              // where the last packet came from
    };

    Summary Status();

    // The side channel (kernel_net.cpp): a datagram to every machine a
    // System Link search reaches (or to one address, as s_addr), and the
    // next one waiting here (-1 for none).
    void SideSend(const void* data, int size, unsigned int only = 0);
    int SideReceive(void* data, int size, unsigned int* from);

    // 0.5.428: a game hosted here off the public list (at most 2 s), before
    // the process ends.
    void PublicShutdown();
}

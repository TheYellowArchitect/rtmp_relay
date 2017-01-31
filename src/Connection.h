//
//  rtmp_relay
//

#pragma once

#include <random>
#include <map>
#include "Socket.h"
#include "RTMP.h"

namespace relay
{
    class Connection
    {
        const std::string name = "Connection";
    public:
        enum class Type
        {
            PUSH,
            PULL
        };

        enum class State
        {
            UNINITIALIZED = 0,
            VERSION_RECEIVED = 1,
            VERSION_SENT = 2,
            ACK_SENT = 3,
            HANDSHAKE_DONE = 4
        };

        Connection(cppsocket::Socket& aSocket, Type aType);

        void update();

    private:
        void handleConnect(cppsocket::Socket&);
        void handleConnectError(cppsocket::Socket&);
        void handleRead(cppsocket::Socket&, const std::vector<uint8_t>& newData);
        void handleClose(cppsocket::Socket&);

        bool handlePacket(const rtmp::Packet& packet);

        void sendServerBandwidth();
        void sendClientBandwidth();
        void sendPing();
        void sendSetChunkSize();
        void sendCheckBW();

        void sendFCPublish();
        void sendFCUnpublish();
        void sendPublish();

        void sendConnect();
        void sendConnectResult(double transactionId);

        void sendBWDone();
        void sendCheckBWResult(double transactionId);

        void sendCreateStream();
        void sendCreateStreamResult(double transactionId);

        void sendReleaseStream();
        void sendReleaseStreamResult(double transactionId);

        void sendPlayStatus(double transactionId);
        void sendStopStatus(double transactionId);

        void sendDeleteStream();

        const uint64_t id;

        std::random_device rd;
        std::mt19937 generator;
        
        Type type;
        State state;
        cppsocket::Socket& socket;

        std::vector<uint8_t> data;

        uint32_t inChunkSize = 128;
        uint32_t outChunkSize = 128;
        uint32_t serverBandwidth = 2500000;

        std::map<uint32_t, rtmp::Header> receivedPackets;
        std::map<uint32_t, rtmp::Header> sentPackets;

        uint32_t invokeId = 0;
        std::map<uint32_t, std::string> invokes;
    };
}

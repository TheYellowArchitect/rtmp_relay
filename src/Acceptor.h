//
//  rtmp_relay
//

#pragma once

#include <vector>
#include <functional>
#include "Socket.h"

class Acceptor: public Socket
{
public:
    Acceptor(Network& network, int socketFd = -1);
    virtual ~Acceptor();
    
    Acceptor(Acceptor&& other);
    Acceptor& operator=(Acceptor&& other);
    
    bool startAccept(uint16_t newPort);
    void setAcceptCallback(const std::function<void(Socket)>& newAcceptCallback);

    virtual bool isWritable() { return false; }

protected:
    virtual bool read();
    
    std::function<void(Socket)> acceptCallback;
};

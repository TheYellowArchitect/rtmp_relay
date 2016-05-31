//
//  rtmp_relay
//

#include <iostream>
#include <signal.h>
#include "Relay.h"

relay::Relay rel;

static void signalHandler(int signo)
{
    if (signo == SIGUSR1)
    {
        rel.printInfo();
    }
    else if (signo == SIGPIPE)
    {
        std::cerr << "Received SIGPIPE" << std::endl;
    }
}

int main(int argc, const char * argv[])
{
    if (argc < 2)
    {
        std::cerr << "Too few arguments" << std::endl;

        const char* exe = argc >= 1 ? argv[0] : "rtmp_relay";
        std::cerr << "Usage: " << exe << " <path to config file>" << std::endl;

        return 1;
    }

    if (signal(SIGUSR1, signalHandler) == SIG_ERR)
    {
        std::cerr << "Failed to capure SIGUSR1" << std::endl;
        return 1;
    }

    if (signal(SIGPIPE, signalHandler) == SIG_ERR)
    {
        std::cerr << "Failed to capure SIGPIPE" << std::endl;
        return 1;
    }
    
    if (!rel.init(argv[1]))
    {
        std::cerr << "Failed to init relay" << std::endl;
        return 1;
    }
    
    rel.run();
    
    return 0;
}

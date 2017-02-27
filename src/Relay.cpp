//
//  rtmp_relay
//

#include <ctime>
#include <memory>
#include <algorithm>
#include <functional>
#include <iostream>
#include <chrono>
#include <thread>
#include "yaml-cpp/yaml.h"
#include "Log.h"
#include "Relay.h"
#include "Status.h"
#include "Connection.h"

using namespace cppsocket;

namespace relay
{
    uint64_t Relay::currentId = 0;

    Relay::Relay(Network& aNetwork):
        network(aNetwork)
    {
        previousTime = std::chrono::steady_clock::now();
    }

    Relay::Relay(Relay&& other):
        network(other.network),
        previousTime(other.previousTime),
        connections(std::move(other.connections))
    {
    }

    Relay& Relay::operator=(Relay&& other)
    {
        previousTime = other.previousTime;
        connections = std::move(other.connections);

        return *this;
    }

    bool Relay::init(const std::string& config)
    {
        connections.clear();
        status.reset();

        YAML::Node document;

        try
        {
            document = YAML::LoadFile(config);
        }
        catch (YAML::BadFile)
        {
            Log(Log::Level::ERR) << "Failed to open " << config;
            return false;
        }
        catch (YAML::ParserException e)
        {
            Log(Log::Level::ERR) << "Failed to parse " << config << ", " << e.msg << " on line " << e.mark.line << " column " << e.mark.column;
            return false;
        }

        if (document["log"])
        {
            const YAML::Node& logObject = document["log"];

            if (logObject["level"])
            {
                Log::threshold = static_cast<Log::Level>(logObject["level"].as<uint32_t>());
            }

#if !defined(_MSC_VER)
            if (logObject["syslogIdent"])
            {
                syslogIdent = logObject["syslogIdent"].as<std::string>();
            }

            if (logObject["syslogFacility"])
            {
                std::string facility = logObject["syslogFacility"].as<std::string>();

                if (facility == "LOG_USER") syslogFacility = LOG_USER;
                else if (facility == "LOG_LOCAL0") syslogFacility = LOG_LOCAL0;
                else if (facility == "LOG_LOCAL1") syslogFacility = LOG_LOCAL1;
                else if (facility == "LOG_LOCAL2") syslogFacility = LOG_LOCAL2;
                else if (facility == "LOG_LOCAL3") syslogFacility = LOG_LOCAL3;
                else if (facility == "LOG_LOCAL4") syslogFacility = LOG_LOCAL4;
                else if (facility == "LOG_LOCAL5") syslogFacility = LOG_LOCAL5;
                else if (facility == "LOG_LOCAL6") syslogFacility = LOG_LOCAL6;
                else if (facility == "LOG_LOCAL7") syslogFacility = LOG_LOCAL7;
            }
#endif
        }

        openLog();

        if (document["statusPage"])
        {
            const YAML::Node& statusPageObject = document["statusPage"];

            if (statusPageObject["listen"])
            {
                status.reset(new Status(network, *this, statusPageObject["listen"].as<std::string>()));
            }
        }

        if (document["pingInterval"])
        {
            pingInterval = document["pingInterval"].as<float>();
        }

        std::set<std::string> listenAddresses;

        const YAML::Node& serversArray = document["servers"];

        for (size_t serverIndex = 0; serverIndex < serversArray.size(); ++serverIndex)
        {
            const YAML::Node& serverObject = serversArray[serverIndex];

            Server::Description serverDescription;

            const YAML::Node& inputArray = serverObject["inputs"];

            for (size_t inputIndex = 0; inputIndex < inputArray.size(); ++inputIndex)
            {
                const YAML::Node& inputObject = inputArray[inputIndex];

                Connection::Description inputDescription;

                if (inputObject["type"].as<std::string>() == "host") inputDescription.type = Connection::Type::HOST;
                else if (inputObject["type"].as<std::string>() == "client") inputDescription.type = Connection::Type::CLIENT;

                if (inputObject["address"].IsSequence())
                {
                    const YAML::Node& addressArray = inputObject["address"];

                    for (size_t addressIndex = 0; addressIndex < addressArray.size(); ++addressIndex)
                    {
                        std::string address = addressArray[addressIndex].as<std::string>();
                        std::pair<uint32_t, uint16_t> addr = Socket::getAddress(address);

                        inputDescription.addresses.push_back(std::make_pair(addr.first, addr.second));

                        if (inputDescription.type == Connection::Type::HOST)
                        {
                            listenAddresses.insert(address);
                        }
                    }
                }
                else
                {
                    std::string address = inputObject["address"].as<std::string>();
                    std::pair<uint32_t, uint16_t> addr = Socket::getAddress(address);

                    inputDescription.addresses.push_back(std::make_pair(addr.first, addr.second));
                }

                if (inputObject["connectionTimeout"]) inputDescription.connectionTimeout = inputObject["connectionTimeout"].as<float>();
                if (inputObject["reconnectInterval"]) inputDescription.reconnectInterval = inputObject["reconnectInterval"].as<float>();
                if (inputObject["reconnectCount"]) inputDescription.reconnectCount = inputObject["reconnectCount"].as<uint32_t>();

                if (inputObject["applicationName"]) inputDescription.applicationName = inputObject["applicationName"].as<std::string>();
                if (inputObject["streamName"]) inputDescription.streamName = inputObject["streamName"].as<std::string>();
                if (inputObject["overrideApplicationName"]) inputDescription.overrideApplicationName = inputObject["overrideApplicationName"].as<std::string>();
                if (inputObject["overrideStreamName"]) inputDescription.overrideStreamName = inputObject["overrideStreamName"].as<std::string>();
                if (inputObject["video"]) inputDescription.video = inputObject["video"].as<bool>();
                if (inputObject["audio"]) inputDescription.audio = inputObject["audio"].as<bool>();
                if (inputObject["data"]) inputDescription.data = inputObject["data"].as<bool>();

                serverDescription.inputDescriptions.push_back(inputDescription);
            }

            const YAML::Node& outputArray = serverObject["outputs"];

            for (size_t outputIndex = 0; outputIndex < outputArray.size(); ++outputIndex)
            {
                const YAML::Node& outputObject = outputArray[outputIndex];

                Connection::Description outputDescription;

                if (outputObject["type"].as<std::string>() == "host") outputDescription.type = Connection::Type::HOST;
                else if (outputObject["type"].as<std::string>() == "client") outputDescription.type = Connection::Type::CLIENT;

                if (outputObject["address"].IsSequence())
                {
                    const YAML::Node& addressArray = outputObject["address"];

                    for (size_t addressIndex = 0; addressIndex < addressArray.size(); ++addressIndex)
                    {
                        std::string address = addressArray[addressIndex].as<std::string>();
                        std::pair<uint32_t, uint16_t> addr = Socket::getAddress(address);

                        outputDescription.addresses.push_back(std::make_pair(addr.first, addr.second));

                        if (outputDescription.type == Connection::Type::HOST)
                        {
                            listenAddresses.insert(address);
                        }
                    }
                }
                else
                {
                    std::string address = outputObject["address"].as<std::string>();
                    std::pair<uint32_t, uint16_t> addr = Socket::getAddress(address);

                    outputDescription.addresses.push_back(std::make_pair(addr.first, addr.second));
                }

                if (outputObject["connectionTimeout"]) outputDescription.connectionTimeout = outputObject["connectionTimeout"].as<float>();
                if (outputObject["reconnectInterval"]) outputDescription.reconnectInterval = outputObject["reconnectInterval"].as<float>();
                if (outputObject["reconnectCount"]) outputDescription.reconnectCount = outputObject["reconnectCount"].as<uint32_t>();

                if (outputObject["applicationName"]) outputDescription.applicationName = outputObject["applicationName"].as<std::string>();
                if (outputObject["streamName"]) outputDescription.streamName = outputObject["streamName"].as<std::string>();
                if (outputObject["overrideApplicationName"]) outputDescription.overrideApplicationName = outputObject["overrideApplicationName"].as<std::string>();
                if (outputObject["overrideStreamName"]) outputDescription.overrideStreamName = outputObject["overrideStreamName"].as<std::string>();
                if (outputObject["video"]) outputDescription.video = outputObject["video"].as<bool>();
                if (outputObject["audio"]) outputDescription.audio = outputObject["audio"].as<bool>();
                if (outputObject["data"]) outputDescription.data = outputObject["data"].as<bool>();

                serverDescription.outputDescriptions.push_back(outputDescription);
            }

            std::unique_ptr<Server> server(new Server(*this, network, serverDescription));

            for (const Connection::Description& inputDescription : serverDescription.inputDescriptions)
            {
                if (inputDescription.type == Connection::Type::CLIENT)
                {
                    Socket socket(network);

                    std::unique_ptr<Connection> connection(new Connection(*this,
                                                                          socket,
                                                                          inputDescription.addresses,
                                                                          inputDescription.connectionTimeout,
                                                                          inputDescription.reconnectInterval,
                                                                          inputDescription.reconnectCount,
                                                                          Connection::StreamType::INPUT,
                                                                          *server,
                                                                          inputDescription.applicationName,
                                                                          inputDescription.streamName,
                                                                          inputDescription.overrideApplicationName,
                                                                          inputDescription.overrideStreamName));

                    connections.push_back(std::move(connection));
                }
            }

            servers.push_back(std::move(server));
        }

        for (const std::string& address : listenAddresses)
        {
            cppsocket::Socket acceptor(network);
            acceptor.startAccept(address);
            acceptors.push_back(std::move(acceptor));
        }

        return true;
    }

    Server* Relay::getServer(const std::pair<uint32_t, uint16_t>& address, Connection::StreamType type, std::string applicationName, std::string streamName) const
    {
        for (const std::unique_ptr<Server>& server : servers)
        {
            const Server::Description& serverDescription = server->getDescription();

            if (type == Connection::StreamType::INPUT)
            {
                for (const Connection::Description& inputDescription : serverDescription.inputDescriptions)
                {
                    if ((inputDescription.applicationName.empty() || inputDescription.applicationName == applicationName) &&
                        (inputDescription.streamName.empty() || inputDescription.streamName == streamName))
                    {
                        if (type == Connection::StreamType::INPUT)
                        {
                            if (std::find(inputDescription.addresses.begin(),
                                          inputDescription.addresses.end(),
                                          address) != inputDescription.addresses.end())
                            {
                                return server.get();
                            }
                        }
                        else if (type == Connection::StreamType::OUTPUT)
                        {
                            for (const Connection::Description& outputDescription : serverDescription.outputDescriptions)
                            {
                                if (std::find(outputDescription.addresses.begin(),
                                              outputDescription.addresses.end(),
                                              address) != outputDescription.addresses.end())
                                {
                                    return server.get();
                                }
                            }
                        }
                    }
                }
            }
        }

        return nullptr;
    }

    void Relay::close()
    {
        connections.clear();
        status.reset();
        active = false;
    }

    void Relay::run()
    {
        const std::chrono::microseconds sleepTime(10000);

        while (active)
        {
            auto currentTime = std::chrono::steady_clock::now();
            float delta = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - previousTime).count() / 1000.0f;
            previousTime = currentTime;

            network.update();

            if (status) status->update(delta);

            for (const auto& connection : connections)
            {
                connection->update(delta);
            }

            std::this_thread::sleep_for(sleepTime);
        }
    }

    void Relay::getInfo(std::string& str, ReportType reportType) const
    {
        switch (reportType)
        {
            case ReportType::TEXT:
            {
                str = "Connections:\n";
                for (const auto& connection : connections)
                {
                    connection->getInfo(str, reportType);
                }
                break;
            }
            case ReportType::HTML:
            {
                str = "<html><title>Status</title><body>";

                for (const auto& connection : connections)
                {
                    connection->getInfo(str, reportType);
                }

                str += "</body></html>";

                break;
            }
            case ReportType::JSON:
            {
                str = "{\"servers\":[";

                bool first = true;

                for (const auto& connection : connections)
                {
                    if (!first) str += ",";
                    first = false;
                    connection->getInfo(str, reportType);
                }

                str += "]}";
                
                break;
            }
        }
    }

    void Relay::openLog()
    {
#if !defined(_MSC_VER)
        openlog(syslogIdent.empty() ? nullptr : syslogIdent.c_str(), 0, syslogFacility);
#endif
    }

    void Relay::closeLog()
    {
#if !defined(_MSC_VER)
        closelog();
#endif
    }

    void Relay::handleAccept(cppsocket::Socket&, cppsocket::Socket& clientSocket)
    {
        std::unique_ptr<Connection> connection(new Connection(*this,
                                                              clientSocket,
                                                              pingInterval));

        connections.push_back(std::move(connection));
    }
}

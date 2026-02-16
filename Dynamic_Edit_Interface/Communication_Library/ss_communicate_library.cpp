#include "ss_communicate_library.h"
#include "ss_communicate_tcp_client.h"
#include "ss_communicate_tcp_server.h"
#include "ss_communicate_serial.h"

CommunicateLibrary::CommunicateLibrary()
{

}

CommunicateLibrary::~CommunicateLibrary()
{

}

CommunicateLibrary& CommunicateLibrary::Instance()
{
    static CommunicateLibrary communicate_library;
    return communicate_library;
}

std::shared_ptr<CommunicateInterface> CommunicateLibrary::CreateCommunicateFactory(CommunicateType type)
{
    std::shared_ptr<CommunicateInterface> interface_c;
    if (type == CommunicateType::TCP_CLIENT) {
        interface_c = std::make_shared<CommunicateTcpClient>();
    }
    else if (type == CommunicateType::TCP_SERVER) {
        interface_c = std::make_shared<CommunicateTcpServer>();
    }
    else if (type == CommunicateType::SERIAL) {
        interface_c = std::make_shared<CommunicateSerial>();
    }
    return interface_c;
}
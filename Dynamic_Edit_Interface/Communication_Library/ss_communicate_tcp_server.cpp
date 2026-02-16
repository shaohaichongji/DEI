#include "ss_communicate_tcp_server.h"

#include "ss_communicate_tcp_server_private.h"

CommunicateTcpServer::CommunicateTcpServer()
{
    communicate_tcp_server_impl_ = std::make_shared<CommunicateTcpServerPrivate>();
}

CommunicateTcpServer::~CommunicateTcpServer()
{

}

bool CommunicateTcpServer::Init()
{
    return communicate_tcp_server_impl_->Init();
}

bool CommunicateTcpServer::Connect(ConnectionInfo connect_info) const
{
    return communicate_tcp_server_impl_->Connect(connect_info);
}

bool CommunicateTcpServer::IsConnected() const
{
    return communicate_tcp_server_impl_->IsConnected();
}

void CommunicateTcpServer::Disconnect()
{
    communicate_tcp_server_impl_->Disconnect();
}

bool CommunicateTcpServer::ReConnect()
{
    communicate_tcp_server_impl_->ReConnect();
    return true;
}

CommunicateConnectState CommunicateTcpServer::GetState() const
{
    return CommunicateConnectState::BoundState;
}

int64_t CommunicateTcpServer::WriteData(const char* data, int64_t len)
{
    return communicate_tcp_server_impl_->WriteData(data, len);
}

int64_t CommunicateTcpServer::WriteData(const char* data)
{
    return communicate_tcp_server_impl_->WriteData(data);;
}

ConnectionInfo CommunicateTcpServer::GetCommunicateInfo()
{
    return communicate_tcp_server_impl_->connect_info_;
}

void CommunicateTcpServer::SetDataCallback(DataCallback callback)
{
    communicate_tcp_server_impl_->SetDataCallback(callback);
}

void CommunicateTcpServer::SetErrorCallback(ErrorCallback callback)
{
    communicate_tcp_server_impl_->SetErrorCallback(callback);
}
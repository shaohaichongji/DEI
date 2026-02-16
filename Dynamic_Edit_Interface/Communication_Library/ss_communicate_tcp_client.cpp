#include "ss_communicate_tcp_client.h"

#include "ss_communicate_tcp_client_private.h"

CommunicateTcpClient::CommunicateTcpClient()
{
    communicate_tcp_client_impl_ = std::make_shared<CommunicateTcpClientPrivate>();
}

CommunicateTcpClient::~CommunicateTcpClient()
{
    if (communicate_tcp_client_impl_)
        communicate_tcp_client_impl_->Disconnect();
}


bool CommunicateTcpClient::Init()
{
    return communicate_tcp_client_impl_->Init();
}

bool CommunicateTcpClient::Connect(ConnectionInfo connect_info) const
{
    communicate_tcp_client_impl_->connect_info_ = connect_info;
    return communicate_tcp_client_impl_->Connect(connect_info);
}

bool CommunicateTcpClient::IsConnected() const
{
    return communicate_tcp_client_impl_->IsConnected();
}

void CommunicateTcpClient::Disconnect()
{
    communicate_tcp_client_impl_->Disconnect();
}

bool CommunicateTcpClient::ReConnect()
{
    return communicate_tcp_client_impl_->ReConnect();
}

CommunicateConnectState CommunicateTcpClient::GetState() const
{
    return communicate_tcp_client_impl_->GetState();
}

int64_t CommunicateTcpClient::WriteData(const char* data)
{
    return communicate_tcp_client_impl_->WriteData(data);
}

int64_t CommunicateTcpClient::WriteData(const char* data, int64_t len)
{
    return communicate_tcp_client_impl_->WriteData(data, len);
}

ConnectionInfo CommunicateTcpClient::GetCommunicateInfo()
{
    return communicate_tcp_client_impl_->GetCommunicateInfo();
}

void CommunicateTcpClient::SetDataCallback(DataCallback callback)
{
    communicate_tcp_client_impl_->SetDataCallback(callback);
}

void CommunicateTcpClient::SetErrorCallback(ErrorCallback callback)
{
    communicate_tcp_client_impl_->SetErrorCallback(callback);
}

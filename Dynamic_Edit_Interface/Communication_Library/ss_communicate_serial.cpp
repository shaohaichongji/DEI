#include "ss_communicate_serial.h"
#include "ss_communicate_serial_private.h"

CommunicateSerial::CommunicateSerial()
{
    impl_ = std::make_shared<CommunicateSerialPrivate>();
}

CommunicateSerial::~CommunicateSerial()
{
    Disconnect();
}

bool CommunicateSerial::Init()
{
    return impl_->Init();
}

bool CommunicateSerial::Connect(ConnectionInfo connect_info) const
{
    impl_->connect_info_ = connect_info;
    return impl_->Connect(connect_info);
}

bool CommunicateSerial::IsConnected() const
{
    return impl_->IsConnected();
}

void CommunicateSerial::Disconnect()
{
    impl_->Disconnect();
}

bool CommunicateSerial::ReConnect()
{
    return impl_->ReConnect();
}

CommunicateConnectState CommunicateSerial::GetState() const
{
    return impl_->GetState();
}

int64_t CommunicateSerial::WriteData(const char* data)
{
    return impl_->WriteData(data);
}

int64_t CommunicateSerial::WriteData(const char* data, int64_t len)
{
    return impl_->WriteData(data, len);
}

ConnectionInfo CommunicateSerial::GetCommunicateInfo()
{
    return impl_->GetCommunicateInfo();
}

void CommunicateSerial::SetDataCallback(DataCallback callback)
{
    impl_->SetDataCallback(callback);
}

void CommunicateSerial::SetErrorCallback(ErrorCallback callback)
{
    impl_->SetErrorCallback(callback);
}

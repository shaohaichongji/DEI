#pragma once

#include <memory>
#include "ss_communicate_interface.h"

#ifdef SS_COMMUNICATE_LIBRARY_LIBRARY
#  define SS_COMMUNICATE_LIBRARY_EXPORT __declspec(dllexport)
#else
#  define SS_COMMUNICATE_LIBRARY_EXPORT __declspec(dllimport)
#endif

class SS_COMMUNICATE_LIBRARY_EXPORT CommunicateLibrary
{
public:
    static CommunicateLibrary& Instance();
    std::shared_ptr<CommunicateInterface> CreateCommunicateFactory(CommunicateType type);
private:
    CommunicateLibrary();
    ~CommunicateLibrary();
    CommunicateLibrary(const CommunicateLibrary&) = delete;
    CommunicateLibrary& operator = (const CommunicateLibrary&) = delete;
    CommunicateLibrary(const CommunicateLibrary&&) = delete;


};


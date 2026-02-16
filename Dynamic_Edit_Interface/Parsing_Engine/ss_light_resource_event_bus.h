#pragma once
#include <functional>
#include <mutex>
#include <unordered_map>
#include <atomic>
#include "ss_light_resource_events.h"

class SS_LightEventBus
{
public:
    using Handler = std::function<void(const SS_LightEvent&)>;

    struct Subscription
    {
        uint64_t id = 0;
        SS_LightEventBus* bus = nullptr;
        void Reset()
        {
            if (bus && id) bus->Unsubscribe(id);
            bus = nullptr; id = 0;
        }
        ~Subscription() { Reset(); }
        Subscription() = default;
        Subscription(uint64_t i, SS_LightEventBus* b) : id(i), bus(b) {}
        Subscription(const Subscription&) = delete;
        Subscription& operator=(const Subscription&) = delete;
        Subscription(Subscription&& o) noexcept { id = o.id; bus = o.bus; o.id = 0; o.bus = nullptr; }
        Subscription& operator=(Subscription&& o) noexcept
        {
            if (this != &o) { Reset(); id = o.id; bus = o.bus; o.id = 0; o.bus = nullptr; }
            return *this;
        }
    };

public:
    Subscription Subscribe(Handler cb)
    {
        const uint64_t id = ++next_id_;
        std::lock_guard<std::mutex> lk(mtx_);
        handlers_[id] = std::move(cb);
        return Subscription{ id, this };
    }

    void Unsubscribe(uint64_t id)
    {
        std::lock_guard<std::mutex> lk(mtx_);
        handlers_.erase(id);
    }

    void Publish(const SS_LightEvent& ev)
    {
        // copy handlers to avoid deadlock/reentrancy issues
        std::unordered_map<uint64_t, Handler> copy;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            copy = handlers_;
        }
        for (auto& kv : copy)
        {
            if (kv.second) kv.second(ev);
        }
    }

private:
    std::mutex mtx_;
    std::unordered_map<uint64_t, Handler> handlers_;
    std::atomic<uint64_t> next_id_{ 0 };
};

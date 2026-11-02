#pragma once
#include <atomic>
#include <array>
#include <cstddef>
#include <cstdint>

namespace kt {
// Preallocated multiple-producer, single-consumer queue. Reserving a slot never
// waits for the writer or performs I/O. A reserved slot cannot be consumed or
// reused until its producer commits it, including after the host movement call.
template<class T,std::size_t Capacity> class AuditQueue {
    static_assert(Capacity>=2&&(Capacity&(Capacity-1))==0,"power-of-two capacity");
    static_assert(std::atomic<std::size_t>::is_always_lock_free,"native lock-free index required");
    struct Slot { std::atomic<std::size_t> sequence{0};T value{}; };
    std::array<Slot,Capacity> slots_{};
    std::atomic<std::size_t> head_{0};
    std::array<unsigned char,64> producerConsumerPadding_{};
    std::size_t tail_=0; // Exactly one consumer; separate from producer cache line.
public:
    enum class Result { Reserved,Full,Busy };
    struct Ticket {std::size_t position=0;};
    AuditQueue() {for(std::size_t i=0;i<Capacity;++i) slots_[i].sequence.store(i,std::memory_order_relaxed);}
    Result reserve(Ticket& ticket) {
        auto position=head_.load(std::memory_order_relaxed);
        for(unsigned attempt=0;attempt<64;++attempt) {
            auto& slot=slots_[position&(Capacity-1)];
            const auto sequence=slot.sequence.load(std::memory_order_acquire);
            const auto difference=static_cast<std::intptr_t>(sequence-position);
            if(difference<0) return Result::Full;
            if(difference==0) {
                if(head_.compare_exchange_weak(position,position+1,std::memory_order_relaxed)) {
                    ticket.position=position;return Result::Reserved;
                }
            } else position=head_.load(std::memory_order_relaxed);
        }
        return Result::Busy; // Caller refuses this action; ordinary contention is not fatal.
    }
    void commit(Ticket ticket,const T& value) {
        auto& slot=slots_[ticket.position&(Capacity-1)];slot.value=value;
        slot.sequence.store(ticket.position+1,std::memory_order_release);
    }
    bool pop(T& value) {
        auto& slot=slots_[tail_&(Capacity-1)];
        if(slot.sequence.load(std::memory_order_acquire)!=tail_+1) return false;
        value=slot.value;slot.sequence.store(tail_+Capacity,std::memory_order_release);++tail_;return true;
    }
};

// Used only under the corresponding producer's existing serialization. Active
// decisions always retain every frame; stable idle traffic is one record/sec.
template<class Key> class AuditSampler {
    Key prior_{};double last_=0;bool seen_=false;
public:
    bool keep(const Key& key,double now,bool required=false) {
        if(!seen_||required||key!=prior_||now<last_||now-last_>=1) {
            seen_=true;prior_=key;last_=now;return true;
        }
        return false;
    }
};
}

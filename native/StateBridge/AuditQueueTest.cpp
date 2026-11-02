// Owned memory only: force reservation ordering, wraparound and writer overlap.
#include "AuditQueue.h"
#include <thread>
#include <vector>
#include <cstdio>
#include <cstdlib>
void check(bool ok,const char* why) {if(!ok) {fprintf(stderr,"FAIL: %s\n",why);exit(1);}}
struct Record {unsigned producer=0,index=0;std::uint64_t check=0;};
int main() {
    using Small=kt::AuditQueue<unsigned,4>;Small small;Small::Ticket first,second,t;
    check(small.reserve(first)==Small::Result::Reserved,"first reservation");
    check(small.reserve(second)==Small::Result::Reserved,"second reservation");
    small.commit(second,2);unsigned value=0;
    check(!small.pop(value),"consumer cannot pass an uncommitted host call");
    small.commit(first,1);
    check(small.pop(value)&&value==1&&small.pop(value)&&value==2,"reservation order survives delayed producer");
    for(unsigned cycle=0;cycle<1000;++cycle) {
        for(unsigned i=0;i<4;++i) {check(small.reserve(t)==Small::Result::Reserved,"full capacity usable");small.commit(t,i);}
        check(small.reserve(t)==Small::Result::Full,"full queue refuses without overwrite");
        for(unsigned i=0;i<4;++i) check(small.pop(value)&&value==i,"wraparound preserves committed data");
        check(!small.pop(value),"drained queue empty");
    }
    constexpr unsigned Producers=9,PerProducer=12000;
    using Queue=kt::AuditQueue<Record,2048>;Queue queue;
    std::vector<unsigned char> seen(Producers*PerProducer,0);
    std::atomic<bool> start{false};std::atomic<unsigned> completed{0};
    std::vector<std::thread> workers;
    for(unsigned p=0;p<Producers;++p) workers.emplace_back([&,p] {
        while(!start.load()) std::this_thread::yield();
        for(unsigned i=0;i<PerProducer;++i) {
            Queue::Ticket ticket;
            while(queue.reserve(ticket)!=Queue::Result::Reserved) std::this_thread::yield();
            if(i%127==0) std::this_thread::yield(); // Writer meets in-flight host calls.
            queue.commit(ticket,{p,i,(static_cast<std::uint64_t>(p)<<32)^i^0xa1975ee1ull});
        }
        ++completed;
    });
    start=true;unsigned received=0;Record record;
    while(received<Producers*PerProducer) {
        if(!queue.pop(record)) {std::this_thread::yield();continue;}
        check(record.producer<Producers&&record.index<PerProducer,"record bounds");
        check(record.check==((static_cast<std::uint64_t>(record.producer)<<32)^record.index^0xa1975ee1ull),"no torn record");
        auto& count=seen[record.producer*PerProducer+record.index];check(count++==0,"no duplicate record");++received;
        if(received%199==0) std::this_thread::yield();
    }
    for(auto& worker:workers) worker.join();
    check(completed==Producers&&!queue.pop(record),"every concurrent record delivered once");
    kt::AuditSampler<unsigned> sampler;unsigned kept=0;
    // Eight hours at 120 Hz retains at most one stable idle record/sec, rather
    // than exhausting a research capture after a few minutes.
    for(unsigned frame=0;frame<8*3600*120;++frame) if(sampler.keep(1,frame/120.0)) ++kept;
    check(kept==8*3600,"idle duration sampling bound");
    check(sampler.keep(2,28800.0)&&!sampler.keep(2,28800.001),"state transition retained immediately");
    for(unsigned i=0;i<120;++i) check(sampler.keep(2,28800.002+i/120.0,true),"every active request retained");
    check(sampler.keep(2,0),"clock rollback retains diagnostic");
    puts("AUDIT FIXTURES PASS: 108000 records across nine producers and one consumer, delayed commit, full/wraparound refusal and eight-hour idle sampling.");
}

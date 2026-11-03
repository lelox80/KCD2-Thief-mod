#pragma once

#include <atomic>
#include <cstdint>

namespace ktvault {
struct JumpEdge {
    std::uint64_t epoch=0;
    std::uint32_t entity=0;
    bool down=true;

    bool update(std::uint64_t nextEpoch,std::uint32_t nextEntity,bool known,const unsigned char (&bits)[2]) {
        const bool held=!known||bits[0]!=0;
        const bool fresh=epoch==nextEpoch&&entity==nextEntity&&!down&&known&&bits[0]==1&&bits[1]==0;
        epoch=nextEpoch;
        entity=nextEntity;
        down=held;
        return fresh;
    }
};

inline bool claimCooldown(std::atomic<std::uint64_t>& next,std::uint64_t now) {
    auto ready=next.load();
    return now>=ready&&next.compare_exchange_strong(ready,now+1500);
}
}

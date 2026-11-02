#include "eternity/game/random.hpp"

namespace eternity::game {
void ServerRandom::seed(std::uint32_t value) noexcept {
    words_[0]=value;for(std::size_t i=1;i<words_.size();++i) words_[i]=words_[i-1]*69069U;seed_=value;index_=624;
}
std::uint32_t ServerRandom::next(std::uint32_t bound) noexcept {
    if(bound==0) return 0;
    if(index_>=624) {
        if(index_==625) seed(seed_);
        for(std::size_t i=0;i<623;++i) {const auto y=(words_[i]&0x80000000U)|(words_[i+1]&0x7fffffffU);words_[i]=words_[(i+397)%624]^(y>>1)^((y&1U)?0x9908b0dfU:0U);}
        const auto y=(words_[623]&0x80000000U)|(words_[0]&0x7fffffffU);words_[623]=words_[396]^(y>>1)^((y&1U)?0x9908b0dfU:0U);index_=0;
    }
    auto value=words_[index_++];value^=value>>11;value^=(value&0xff3a58adU)<<7;value^=(value&0xffffdf8cU)<<15;value^=value>>18;return value%bound;
}
}

#include "eternity/game/item_ids.hpp"
#include "eternity/game/characters.hpp"
#include <iostream>
#include <set>
#include <thread>
#include <vector>

void require(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
int main() {
    using namespace eternity::game;
    try {
        const auto base=std::chrono::sys_days{std::chrono::year{2024}/1/1};
        ItemIds ids(3);
        const auto first=ids.next({1000,base});
        require(first==0x6042000000010003ULL,"Original item ID calendar/counter/server layout changed");
        require(ids.next({2999,base+std::chrono::seconds{1}})==0x6042000000020003ULL,"Cached two-second item ID interval changed");
        require(ids.next({3000,base+std::chrono::seconds{2}})==0x6042000200010003ULL,"Item ID interval boundary did not reset counter/calendar");
        ItemIds future(0);
        require(future.next({1000,std::chrono::sys_days{std::chrono::year{2032}/1/1}})==0x8042000000010000ULL,
                "Item identifiers lost the signed SQL bigint high bit");
        ItemIds shared(7);std::array<std::vector<std::uint64_t>,8> generated;std::vector<std::jthread> threads;
        for(std::size_t i=0;i<generated.size();++i) threads.emplace_back([&,i] {
            for(unsigned int n=0;n<1000;++n) generated[i].push_back(shared.next({1000,base}));
        });
        threads.clear();std::set<std::uint64_t> unique;
        for(const auto& batch:generated) for(const auto id:batch) unique.insert(id);
        require(unique.size()==8000,"Concurrent item ID generation produced duplicates");
        ItemIds exhausted(0);
        for(unsigned int n=0;n<65535;++n) (void)exhausted.next({1000,base});
        require(exhausted.next({1000,base})==0x6042000000000000ULL,"Original low16 counter truncation changed");
        require(exhausted.raw_next({1000,base})==0x6042000000010000ULL,"Raw and server adapters do not share counter");
        require(exhausted.next({3000,base+std::chrono::seconds{2}})==0x6042000200010000ULL,"Wrapped item interval did not refresh at its boundary");
        ItemIds truncated(255);
        require(truncated.next({0xffffffffffffffffULL,base})==0x60420000000100ffULL,"Snapshot tick did not use low DWORD");
        require(truncated.next({0x100000000ULL,base+std::chrono::seconds{2}})==0x60420000000200ffULL,"Absolute wrapped deadline did not retain cached calendar");
        truncated.set_start_id(0xffffffffU);
        require(truncated.next({1999,base+std::chrono::seconds{3}})==0x60420003000000ffULL,"Baseline DWORD increment did not wrap");
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}

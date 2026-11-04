#include "eternity/game/vertical_movement.hpp"
#include <array>
#include <bit>
#include <cstdint>
#include <cfenv>
#include <limits>
#if defined(__x86_64__) && defined(__SSE2__)
#include <xmmintrin.h>
#endif
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity::game;
static float real(std::uint32_t x) { return std::bit_cast<float>(x); }
static std::uint32_t bits(float x) { return std::bit_cast<std::uint32_t>(x); }
static UserPosition position(const std::array<std::uint32_t,30>& a, std::size_t i) {return {real(a[i]),real(a[i+1]),real(a[i+2])};}
static std::array<std::uint32_t,11> words(const ZoneMovementCheck& s) {
    return {s.back_time,s.check_time,bits(s.back.x),bits(s.back.y),bits(s.back.z),s.previous_time,
            bits(s.previous.x),bits(s.previous.y),bits(s.previous.z),bits(s.total_distance),bits(s.maximum_speed)};
}
int main(int argc,char** argv) {
    if(argc!=2) return 1;
    std::ifstream in(argv[1]); if(!in) return 1;
    std::string line;unsigned cases=0;
    while(std::getline(in,line)) {
        if(line.empty() || line[0]=='#') continue;
        std::array<std::uint32_t,30> a{};std::istringstream stream(line);
        for(auto& x:a) if(!(stream>>std::hex>>x)) throw std::runtime_error("Malformed movement oracle");
        ZoneMovementCheck s; s.back_time=a[0];s.check_time=a[1];s.back=position(a,2);s.previous_time=a[5];s.previous=position(a,6);s.total_distance=real(a[9]);s.maximum_speed=real(a[10]);
        unsigned calls=0;const auto result=vertical_movement_update(s,position(a,11),real(a[14]),[&]{if(calls>=2) throw std::runtime_error("Extra clock sample");return a[15+calls++];});
        const auto out=words(s);
        for(unsigned i=0;i<11;++i) if(out[i]!=a[17+i]) {std::cerr<<"Case "<<cases<<" field "<<i<<" expected "<<std::hex<<a[17+i]<<" got "<<out[i]<<'\n';return 1;}
        if(result!=(a[28]!=0) || calls!=a[29]) {std::cerr<<"Case "<<cases<<" result/clock mismatch\n";return 1;}
        ++cases;
    }
    if(cases!=4096) throw std::runtime_error("Incomplete movement oracle");
    auto unsupported=[](ZoneMovementCheck s, UserPosition p, float speed, unsigned expected_calls) {
        const auto before=words(s);unsigned calls=0;bool rejected=false;
        try {vertical_movement_update(s,p,speed,[&]{++calls;return 0U;});} catch(const std::logic_error&) {rejected=true;}
        if(!rejected || words(s)!=before || calls!=expected_calls) throw std::runtime_error("Unsupported movement input/environment changed state or clock ordering");
    };
    const auto saved_round=std::fegetround();
    if(std::fesetround(FE_DOWNWARD)!=0) throw std::runtime_error("Cannot set rounding mode");
    unsupported({}, {}, 1, 0);
    if(std::fegetround()!=FE_DOWNWARD || std::fesetround(saved_round)!=0) throw std::runtime_error("Rounding mode changed");
#if defined(__x86_64__) && defined(__SSE2__)
    const auto mxcsr=_mm_getcsr();
    for(unsigned mask: {0x8000U,0x0040U,0x2000U}) {
        _mm_setcsr(mxcsr|mask);unsupported({}, {}, 1, 0);
        if(_mm_getcsr()!=(mxcsr|mask)) throw std::runtime_error("MXCSR changed");
    }
    _mm_setcsr(mxcsr);
#endif
    unsupported({}, {std::numeric_limits<float>::infinity(),0,0}, 1, 0);
    unsupported({}, {}, std::numeric_limits<float>::quiet_NaN(), 0);
    ZoneMovementCheck overflow;overflow.previous.x=std::numeric_limits<float>::max();
    unsupported(overflow, {-std::numeric_limits<float>::max(),0,0}, 1, 1);
    std::cout<<cases<<" original movement checks and environment guards passed\n";
}

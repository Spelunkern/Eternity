#include "eternity/game/legacy_sql.hpp"
#include <bit>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace eternity::game {
std::string legacy_sql_coordinate(float value) {
    // _fltout2 00450877 emits 17 significant digits; _fptostr 00450746
    // rounds the retained decimal suffix upward at >=5, then _cftof prints it.
    // This differs from Linux printf at exact ties and at large magnitudes.
    if(value==0) return "0.000000"; // The original normalizes negative zero.
    const bool negative=std::signbit(value);
    if(std::isinf(value)) return negative?"-1.#INF00":"1.#INF00";
    if(std::isnan(value)) return std::bit_cast<std::uint32_t>(value)==0xffc00000U?"-1.#IND00":negative?"-1.#QNAN0":"1.#QNAN0";
    char buffer[128];const auto formatted=std::to_chars(buffer,buffer+sizeof(buffer),std::fabs(static_cast<double>(value)),std::chars_format::scientific,16);
    if(formatted.ec!=std::errc{}) throw std::runtime_error("Cannot format original character coordinate");
    const std::string scientific(buffer,formatted.ptr);const auto exponent_at=scientific.find('e');
    const int keep=std::stoi(scientific.substr(exponent_at+1))+7;
    std::string digits;for(std::size_t i=0;i<exponent_at;++i) if(scientific[i]!='.') digits+=scientific[i];
    std::string scaled;
    if(keep>=0) {
        scaled=digits.substr(0,static_cast<std::size_t>(keep));
        if(keep>static_cast<int>(digits.size())) scaled.append(static_cast<std::size_t>(keep)-digits.size(),'0');
        if(keep<static_cast<int>(digits.size()) && digits[static_cast<std::size_t>(keep)]>='5') {
            std::size_t at=scaled.size();while(at && scaled[at-1]=='9') {scaled[--at]='0';}
            if(at) ++scaled[at-1];else scaled.insert(scaled.begin(),'1');
        }
    }
    if(scaled.size()<7) scaled.insert(scaled.begin(),7-scaled.size(),'0');
    scaled.insert(scaled.size()-6,1,'.');if(negative) scaled.insert(scaled.begin(),'-');return scaled;
}
}

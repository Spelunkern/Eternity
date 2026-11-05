#pragma once
#include "eternity/game/obelisk_runtime_calendar.hpp"
#include <chrono>
#include <ctime>
#include <stdexcept>

namespace eternity::game {
// Native local clock and OLE DATE conversion for valid world calendars in the
// packed 2000..2063 range. Invalid values leave the output unchanged.
class WorldNativeCalendar final:public ObeliskRuntimeCalendarPlatform {
public:
    ObeliskRuntimeCalendar local_calendar() override {
        const auto now=std::time(nullptr);std::tm value{};
        if(now==static_cast<std::time_t>(-1) || !localtime_r(&now,&value))throw std::runtime_error("Cannot read local world calendar");
        return {static_cast<std::uint16_t>(value.tm_year+1900),static_cast<std::uint16_t>(value.tm_mon+1),
            static_cast<std::uint16_t>(value.tm_wday),static_cast<std::uint16_t>(value.tm_mday),
            static_cast<std::uint16_t>(value.tm_hour),static_cast<std::uint16_t>(value.tm_min),static_cast<std::uint16_t>(value.tm_sec),0};
    }
    bool variant_day(const ObeliskRuntimeCalendar& value,double& output) override {
        if(value.year<2000 || value.year>2063 || value.hour>23 || value.minute>59 || value.second>59)return false;
        const std::chrono::year_month_day date{std::chrono::year{value.year},std::chrono::month{value.month},std::chrono::day{value.day}};
        if(!date.ok())return false;
        constexpr auto origin=std::chrono::sys_days{std::chrono::year{1899}/12/30};
        const auto days=(std::chrono::sys_days{date}-origin).count();
        // OLE DATE binary64 addition order matches the captured Wine OLE
        // responses; rearranging this expression changes elapsed seconds.
        volatile double result=static_cast<double>(days);
        result=result+static_cast<double>(value.hour)/24.0;
        result=result+static_cast<double>(value.minute)/1440.0;
        result=result+static_cast<double>(value.second)/86400.0;
        output=result;return true;
    }
};
}

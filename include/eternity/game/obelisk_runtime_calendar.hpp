#pragma once
#include "eternity/game/obelisk_runtime_ownership.hpp"

namespace eternity::game {
struct ObeliskRuntimeCalendar {
    std::uint16_t year=0,month=0,weekday=0,day=0,hour=0,minute=0,second=0,millisecond=0;
};
class ObeliskRuntimeCalendarPlatform {
public:
    virtual ~ObeliskRuntimeCalendarPlatform()=default;
    // Original uses GetLocalTime, despite its wrapper name GetSystemTime.
    virtual ObeliskRuntimeCalendar local_calendar()=0;
    // SystemTimeToVariantTime: conversion may fail and leave the output intact.
    // Original ignores the boolean return and starts each output at +0.0.
    virtual bool variant_day(const ObeliskRuntimeCalendar&,double& output)=0;
};
// GetSystemTime 004cb8e0: wrapping additive packing, no host year/range checks.
std::uint32_t pack_obelisk_calendar(const ObeliskRuntimeCalendar&) noexcept;
ObeliskRuntimeCalendar unpack_obelisk_calendar(std::uint32_t) noexcept;
// Native bodies of GetSystemTime / SubTimeSecond 004cb9d0. The platform owns
// GetLocalTime and OLE date conversion; no default calendar approximation.
class ObeliskRuntimeCalendarClock final:public ObeliskRuntimeOwnershipClock {
public:
    explicit ObeliskRuntimeCalendarClock(ObeliskRuntimeCalendarPlatform& platform):platform_(platform) {}
    std::uint32_t packed_now() override;
    std::uint32_t seconds_between(std::uint32_t now,std::uint32_t change) override;
private:
    ObeliskRuntimeCalendarPlatform& platform_;
};
}

#pragma once
#include "eternity/game/mob_primary_zones.hpp"
#include <variant>

namespace eternity::game {
// Pins the actual source and destination against primary shutdown and instance
// recycling. The provider lease must retain the world/session services used by
// the transfer; it may not be a non-owning alias. All acquisition and transfer
// operations run on the world's serialized executor.
class VerticalMoveZoneLeases final {
public:
    static std::shared_ptr<VerticalMoveZoneLeases> create(std::shared_ptr<MobPrimaryZones> authority,
        RuntimeZone& source,RuntimeZone& destination,std::shared_ptr<void> provider_lease) {
        if(!authority || !provider_lease)throw std::invalid_argument("Zone transfer requires retained authority and providers");
        auto first=acquire(*authority,source);
        auto second=&source==&destination?Lease{}:acquire(*authority,destination);
        return std::shared_ptr<VerticalMoveZoneLeases>(new VerticalMoveZoneLeases(std::move(authority),std::move(provider_lease),
            source,destination,std::move(first),std::move(second)));
    }
    void validate(RuntimeZone& source,RuntimeZone& destination) const {
        if(&source!=source_ || &destination!=destination_)throw std::logic_error("Zone transfer lease identity differs");
        visit(first_,source);
        if(source_!=destination_)visit(second_,destination);
    }
private:
    using Lease=std::variant<std::monostate,MobPrimaryZoneLease,MobInstanceZoneLease>;
    static Lease acquire(MobPrimaryZones& authority,RuntimeZone& zone) {
        if(!zone.map)throw std::logic_error("Zone transfer lease has no map");
        if(zone.map->settings.create_type==0) {
            auto held=authority.lease_primary(zone);if(!held)throw std::logic_error("Source or destination primary is closing");return Lease(std::move(held));
        }
        auto held=authority.lease_instance(zone);if(!held)throw std::logic_error("Source or destination instance is closing");return Lease(std::move(held));
    }
    static void visit(const Lease& lease,RuntimeZone& expected) {
        std::visit([&](const auto& held){
            using T=std::decay_t<decltype(held)>;
            if constexpr(std::is_same_v<T,std::monostate>)throw std::logic_error("Zone transfer lease is empty");
            else held.visit([&](RuntimeZone& actual){if(&actual!=&expected)throw std::logic_error("Zone transfer lease changed zone");});
        },lease);
    }
    VerticalMoveZoneLeases(std::shared_ptr<MobPrimaryZones> authority,std::shared_ptr<void> providers,
        RuntimeZone& source,RuntimeZone& destination,Lease first,Lease second)
        :authority_(std::move(authority)),providers_(std::move(providers)),source_(&source),destination_(&destination),first_(std::move(first)),second_(std::move(second)) {}
    std::shared_ptr<MobPrimaryZones> authority_;std::shared_ptr<void> providers_;RuntimeZone* source_;RuntimeZone* destination_;Lease first_,second_;
};
}

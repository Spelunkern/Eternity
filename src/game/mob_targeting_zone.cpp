#include "eternity/game/mob_targeting_zone.hpp"
#include <stdexcept>
namespace eternity::game {
MobTargetingZone::MobTargetingZone(ZoneMobs& z,const MobTargetingBindingCatalog& c,MobTargetingZoneWorld& w):zone_(z),catalog_(c),world_(w) {}
const MobTargetingBindingDefinition& MobTargetingZone::definition(const MobRuntime& m) const {
    const auto* d=m.definition?catalog_.mob(m.definition->id):nullptr;
    if(!d) throw std::invalid_argument("Targeting requires a live catalog mob definition");
    return *d;
}
class MobTargetingZone::Runtime final:public MobTargetingRuntimeServices {
public:
    Runtime(MobTargetingZone& o,const MobTargetingZoneUser& u):owner(o),user(u) {}
    MobTargetingBindings bindings(const MobRuntime& m) const override {return owner.definition(m).targeting();}
    std::int32_t random(MobRuntime& m) override {return owner.world_.random(m);}
    std::uint32_t tick_ms(MobRuntime& m) override {return owner.world_.tick_ms(m);}
    void help(MobRuntime& m,const MobTargetingUser&,std::uint8_t kind) override {
        if(kind!=2 && kind!=3 && kind!=5) throw std::invalid_argument("Unknown targeting help kind");
        owner.find_help(m,user,static_cast<MobTargetingHelpKind>(kind));
    }
    void dispatch(MobRuntime& m,std::uint32_t e,std::uint32_t a) override {owner.world_.dispatch(m,e,a);}
private:
    MobTargetingZone& owner;const MobTargetingZoneUser& user;
};
class MobTargetingZone::Help final:public MobTargetingHelpServices {
public:
    Help(MobTargetingZone& o,MobRuntime& m,const MobTargetingZoneUser& u):owner(o),source(m),user(u) {refresh();}
    MobTargetingHelpSource projection;
    void refresh() {
        // Bounds/radius/gates are cached by the recovered coordinator. Only
        // source position/type are read again after callbacks in its inner loop.
        projection.x=source.position.x;projection.z=source.position.z;projection.mob_type=source.type;
    }
    void visit_cell(std::int32_t x,std::int32_t z,const std::function<void(const MobTargetingHelpCandidate&)>& visit) override {
        owner.zone_.visit_cell({x,z},[&](MobRuntime& m) {
            current=&m;
            visit({mob_status(m),m.type,owner.definition(m).ai,m.position.x,m.position.z});
            current=nullptr; // No actor access after callback; it may be detached.
        });
    }
    void set_target_without_help() override {
        owner.set_target(*current,user,false);
        refresh(); // Source lifetime is retained separately by the caller.
    }
private:
    MobTargetingZone& owner;MobRuntime& source;const MobTargetingZoneUser& user;MobRuntime* current=nullptr;
};
void MobTargetingZone::set_target(MobRuntime& m,const MobTargetingZoneUser& u,bool initialize) {
    Runtime runtime(*this,u);mob_set_target(m,u.target(),initialize,runtime);
}
void MobTargetingZone::find_help(MobRuntime& m,const MobTargetingZoneUser& u,MobTargetingHelpKind kind) {
    if(m.zone!=&zone_ || !m.cell || zone_.find(m.id)!=&m)
        throw std::invalid_argument("Targeting help requires retained source membership");
    const auto& d=definition(m);Help help(*this,m,u);auto& p=help.projection;
    p.zone_user_count=world_.user_count(zone_);p.cells_x=p.cells_z=zone_.cell_count();
    p.cell_x=m.cell->x;p.cell_z=m.cell->z;p.country=d.country;p.radius=d.chase_range;
    mob_find_targeting_help(kind,p,u.help(),help);
}
}

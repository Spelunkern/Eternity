#include "eternity/game/mob_drops_delivery.hpp"
#include "eternity/game/actor.hpp"
#include <stdexcept>

namespace eternity::game {
namespace {
class Delivery final:public MobLootDeliveryWorld {
public:
    Delivery(const MobDropDeliveryContext& context,GroundItemPool& pool,ZoneItems& zone,RandomSource& random,MobDropDeliveryServices& services):context_(context),pool_(pool),zone_(zone),random_(random),services_(services) {}
    MobDropDeliveryResult result;
    MobLootObject* create(const MobLootItem& definition,std::uint32_t owner,std::uint32_t party) override {
        const auto& canonical=services_.definition(definition);
        if(canonical.id!=definition.id || canonical.type!=definition.type || canonical.type_id!=definition.type_id) throw std::invalid_argument("Drop definition identity mismatch");
        item_=create_mob_drop_item(context_.creation,canonical,owner,party,pool_,random_,services_);
        if(!item_) {result.destination=MobDropDestination::allocation_failed;return nullptr;}
        object_={&definition,item_->user_id,item_->party_id};result={MobDropDestination::unlinked,{},item_};return &object_;
    }
    void sequence_log(MobLootObject& object,MobLootUser* user) override {
        require(object);item_->item.make_type="V";services_.sequence_log(*item_,user);refresh();
    }
    bool zone_item(MobLootUser& credited,MobLootUser* candidate) override {
        if(!candidate) throw std::invalid_argument("Original zone helper requires retained candidate");
        return mob_drop_zone_enabled(context_.zone_identity,credited,*candidate);
    }
    bool range_item(MobLootUser& candidate,MobLootUser* credited,std::int32_t radius) override {
        if(!credited) throw std::invalid_argument("Original range helper requires retained credit");
        return mob_drop_range_enabled(context_.zone_identity,*credited,candidate,radius);
    }
    void log_item(MobLootObject& object,MobLootUser* user) override {require(object);services_.log_item(*item_,user);refresh();}
    void give_item(MobLootUser& user,MobLootObject& object) override {
        require(object);sync();const auto target=services_.recipient(user,*item_);
        if(target.actor.id()!=user.id) throw std::invalid_argument("Drop recipient identity mismatch");
        const auto receipt=target.actor.receive_item(*item_,target.world,pool_);
        result={MobDropDestination::inventory,receipt,nullptr};refresh();
    }
    void enter_item(MobLootObject& object,std::uint8_t appearance) override {
        require(object);sync();const bool entered=zone_.enter(*item_,appearance);
        result={entered?MobDropDestination::ground:MobDropDestination::unlinked,{},item_};refresh();
    }
private:
    void require(const MobLootObject& object) const {if(&object!=&object_ || !item_) throw std::invalid_argument("Wrong mob drop invocation handle");}
    void sync() {item_->user_id=object_.owner;item_->party_id=object_.party;}
    void refresh() {object_.owner=item_->user_id;object_.party=item_->party_id;}
    const MobDropDeliveryContext& context_;GroundItemPool& pool_;ZoneItems& zone_;RandomSource& random_;MobDropDeliveryServices& services_;
    MobLootObject object_;GroundItem* item_=nullptr;
};
}
MobDropDeliveryResult deliver_mob_drop(const MobLootItem* definition,MobLootUser* user,MobLootParty* party,
    bool sequence,bool logged,const MobDropDeliveryContext& context,GroundItemPool& pool,ZoneItems& zone,RandomSource& random,MobDropDeliveryServices& services) {
    Delivery delivery(context,pool,zone,random,services);mob_deliver_item(definition,user,party,sequence,logged,random,delivery);return delivery.result;
}
MobDropDeliveryResult place_mob_drop_money(const MobDropCreationContext& context,std::uint32_t owner,
    std::uint32_t amount,GroundItemPool& pool,ZoneItems& zone,RandomSource& random) {
    auto* item=create_mob_drop_money(context,owner,amount,pool,random);if(!item) return {MobDropDestination::allocation_failed,{},nullptr};
    const bool entered=zone.enter(*item,2);return {entered?MobDropDestination::ground:MobDropDestination::unlinked,{},item};
}
}

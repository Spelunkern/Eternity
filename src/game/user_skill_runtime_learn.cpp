#include "eternity/game/user_skill_runtime_learn.hpp"
#include <algorithm>
#include <bit>
#include <stdexcept>
namespace eternity::game {
namespace {
void put(net::Packet& p,std::size_t at,std::uint32_t v,unsigned n){for(unsigned i=0;i<n;++i)p[at+i]=static_cast<std::uint8_t>(v>>(8*i));}
void error(Actor& actor,std::uint8_t reason,std::int32_t id,std::int32_t level){net::Packet p(7,0);put(p,0,0x209,2);p[2]=reason;put(p,4,static_cast<std::uint32_t>(id),2);p[6]=static_cast<std::uint8_t>(level);actor.send_self(p);}
bool eligible(Actor& actor,PlayerCore& player,const UserSkillDefinition& def,std::int32_t id,std::int32_t level){
    if(player.skill_points<def.learn_points){error(actor,1,id,level);return false;}
    if(actor.status().level<def.required_level){error(actor,2,id,level);return false;}
    const auto& identity=actor.appearance().identity;
    const auto family=identity.family;
    switch(def.country){
        case 0:if(family!=0)return false;break;case 1:if(family!=1)return false;break;
        case 2:if(family!=0&&family!=1)return false;break;case 3:if(family!=3)return false;break;
        case 4:if(family!=2)return false;break;case 5:if(family!=3&&family!=2)return false;break;
        default:break;
    }
    if(identity.job>=def.jobs.size())throw std::out_of_range("User job outside original six eligibility bytes");
    const auto grow=identity.grow;
    if(!def.jobs[identity.job] || (grow==0&&def.grow!=0) || (grow==1&&def.grow>1) ||
       (grow==2&&def.grow==3) || def.type_show==0){error(actor,3,id,level);return false;}
    return true;
}
}
void UserSkillRuntime::learn(PlayerCore& player,std::int32_t id,std::int32_t level,UserSkillLearnWorld& world){
    if(player.character.id!=actor_->id())throw std::invalid_argument("Skill learning requires the bound PlayerCore");
    if(level<1||level>3){error(*actor_,3,id,level);return;}
    // Actual user definitions use WORD IDs; do not wrap malformed int requests.
    const auto resolve=[&](){return id>0&&id<=65535?services_.definition(static_cast<std::uint16_t>(id),static_cast<std::uint8_t>(level)):nullptr;};
    const auto* requested=resolve();if(!requested){error(*actor_,3,id,level);return;}
    std::int32_t index=0;
    for(;index<count_;++index){const auto item=slot(static_cast<std::uint32_t>(index));if(item&&item->skill_id==static_cast<std::uint32_t>(id))break;}
    const bool upgrade=index<count_;
    if(upgrade){if(std::bit_cast<std::int32_t>(slot(static_cast<std::uint32_t>(index))->level)>=level){error(*actor_,3,id,level);return;}}
    else if(level!=1)return;
    // EnableLearnSkill does a SECOND live definition lookup. Its eligibility
    // cost may differ; deduction below still uses the FIRST captured definition.
    const auto* validation=resolve();if(!validation||!eligible(*actor_,player,*validation,id,level))return;
    player.skill_points=static_cast<std::uint16_t>(player.skill_points-requested->learn_points);
    UserSkillHandle created;
    if(upgrade){
        auto item=slot(static_cast<std::uint32_t>(index));if(!item)throw std::logic_error("Learned slot disappeared during eligibility");
        item->definition=requested;slot(static_cast<std::uint32_t>(index))->level=static_cast<std::uint32_t>(level);
        const auto now=world.tick_ms();item=slot(static_cast<std::uint32_t>(index));if(!item)throw std::logic_error("Learned slot disappeared during clock callback");item->reset_tick=now;
    }else{
        created=services_.allocate();if(!created)return; // No point refund.
        created->definition=requested;created->skill_id=static_cast<std::uint32_t>(id);created->level=1;created->number=std::bit_cast<std::uint32_t>(count_);
        created->reset_tick=world.tick_ms();
        const auto at=std::bit_cast<std::uint32_t>(count_);if(at>=slots_.size())throw std::out_of_range("Learned append outside original 256 slots");
        if(slots_[at]){services_.strand(std::move(slots_[at]));}
        slots_[at]=created;count_=std::bit_cast<std::int32_t>(std::bit_cast<std::uint32_t>(count_)+1U);
    }
    world.apply_passive(*this,*requested);
    const auto current=[&](){auto p=upgrade?slot(static_cast<std::uint32_t>(index)):created;if(!p||!p->definition)throw std::logic_error("Learned runtime disappeared during dispatch");return p;};
    const auto published=current();const auto number=published->number;
    const auto notify_id=upgrade?static_cast<std::uint32_t>(id):published->skill_id;
    const auto notify_level=upgrade?static_cast<std::uint32_t>(level):published->level;
    net::Packet db(11,0);put(db,0,0x801,2);put(db,2,actor_->account_id(),4);put(db,6,player.skill_points,2);put(db,8,notify_id,2);db[10]=static_cast<std::uint8_t>(notify_level);world.send_database(db);
    net::Packet client(7,0);put(client,0,0x209,2);client[3]=static_cast<std::uint8_t>(number);put(client,4,notify_id,2);client[6]=static_cast<std::uint8_t>(notify_level);actor_->send_self(client);
    net::Packet log(110,0);world.populate_log(*actor_,std::span<std::uint8_t>(log).subspan(2,72));
    const auto name=current()->definition->name;const auto length=std::min(name.size(),name.find('\0'));if(length>=32)throw std::invalid_argument("Skill log name exceeds original 32-byte buffer");
    put(log,0,0x309,2);put(log,74,static_cast<std::uint32_t>(id),2);log[76]=static_cast<std::uint8_t>(upgrade?level:1);std::copy_n(name.begin(),length,log.begin()+77);log[109]=requested->learn_points;world.send_log(log);
}
}

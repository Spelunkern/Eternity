#include "eternity/game/actor.hpp"
#include <bit>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace eternity;using namespace eternity::game;
namespace {
void require(bool value,const std::string& message) {if(!value) throw std::runtime_error(message);}
std::string hex(const net::Packet& bytes) {static constexpr char digits[]="0123456789abcdef";std::string result;for(auto byte:bytes) {result+=digits[byte>>4];result+=digits[byte&15];}return result;}
using State=std::array<std::uint32_t,38>;
State read(std::istream& input) {State result{};for(auto& value:result) input>>value;require(static_cast<bool>(input),"Truncated action state");return result;}
void initialize(Actor& actor,const State& values) {
    std::size_t index=0;const auto next=[&] {return values.at(index++);};const auto signed_next=[&] {return std::bit_cast<std::int32_t>(next());};
    auto& status=actor.status();auto& appearance=actor.appearance();auto& actions=actor.actions();auto& attack=actor.attack_state();
    status.in_zone=next()!=0;appearance.visible=next()!=0;status.shape=static_cast<std::uint8_t>(next());status.form_mob=static_cast<std::uint16_t>(next());
    status.skills.equipment.transformed=status.shape!=0;status.skills.equipment.attack_speed=2;status.skills.equipment.move_speed=2;
    appearance.vehicle_mode=signed_next();actions.vehicle_request=next();appearance.vehicle_addition=signed_next();appearance.vehicle_shape=signed_next();appearance.vehicle_partner=next();
    actions.vehicle_invitation=next();actions.vehicle_invitation_until=next();actions.move_type=signed_next();actions.pending_move=next();actions.move_slot=next();for(auto& slot:actions.move_slots) slot=next();
    attack.mode=signed_next();attack.recovery=signed_next();attack.previous_tick=next();attack.next_tick=next();attack.resume_tick=next();
    status.current_target_type=signed_next();status.current_target=next();actions.previous_target_type=signed_next();actions.previous_target=next();
    status.action_block_b=signed_next();status.fixed_form=signed_next();status.casting=signed_next();status.shape_template=next();
    appearance.copied_user.emplace();require(index==values.size(),"Action state mapping changed");
}
State snapshot(const Actor& actor) {
    const auto& status=actor.status();const auto& appearance=actor.appearance();const auto& actions=actor.actions();const auto& attack=actor.attack_state();
    State result{};std::size_t index=0;const auto put=[&](auto value) {result.at(index++)=static_cast<std::uint32_t>(value);};
    put(status.in_zone);put(appearance.visible);put(status.shape);put(status.form_mob);
    put(appearance.vehicle_mode);put(actions.vehicle_request);put(appearance.vehicle_addition);put(appearance.vehicle_shape);put(appearance.vehicle_partner);put(actions.vehicle_invitation);put(actions.vehicle_invitation_until);
    put(actions.move_type);put(actions.pending_move);put(actions.move_slot);for(auto slot:actions.move_slots) put(slot);
    put(attack.mode);put(attack.recovery);put(attack.previous_tick);put(attack.next_tick);put(attack.resume_tick);
    put(status.current_target_type);put(status.current_target);put(actions.previous_target_type);put(actions.previous_target);
    put(status.action_block_b);put(status.fixed_form);put(status.casting);put(status.shape_template);
    require(index==result.size(),"Action result mapping changed");return result;
}
struct Scene final:ActorScene {
    std::vector<std::string>& events;std::uint32_t tick;
    Scene(std::vector<std::string>& trace,std::uint32_t value):events(trace),tick(value) {}
    void send_self(const net::Packet& packet) noexcept override {events.push_back("S"+hex(packet));}
    void send_nearby(const net::Packet& packet) override {events.push_back("N"+hex(packet));}
    void send_cell_view(const net::Packet& packet) override {events.push_back("C"+hex(packet));}
    std::optional<bool> party_kind() const noexcept override {return {};}
    void send_party(const net::Packet&) noexcept override {std::terminate();}
    std::uint32_t tick_ms() const noexcept override {events.emplace_back("T");return tick;}
    void release_effect_id(std::uint32_t id) noexcept override {events.push_back("F"+std::to_string(id));}
    std::optional<std::uint32_t> allocate_effect_id() noexcept override {std::terminate();}
    void unexpected() {throw std::runtime_error("Unexpected action-cancellation scene operation");}
    void reset_damage() override {unexpected();}
    void stop_target_mob(std::uint32_t) override {unexpected();}
    void taunt(std::uint16_t,float) override {unexpected();}
    ZoneUsers* user_zone() override {unexpected();return nullptr;}
    UserShapeSocial user_shape_social() override {unexpected();return {};}
    void finish_periodic_damage(EffectStatus&,const SkillDefinition&,const EffectSource&,std::uint32_t) override {unexpected();}
};
struct World final:ActorActionWorld {
    Actor& source;Actor& peer;std::vector<std::string>& events;bool available;
    World(Actor& actor,Actor& other,std::vector<std::string>& trace,bool found):source(actor),peer(other),events(trace),available(found) {}
    Actor* find_user(Actor& requester,std::uint32_t id) override {
        require(&requester==&source,"Unexpected action lookup requester");events.push_back("Q"+std::to_string(id));
        if(id==source.id()) return &source;
        return id==peer.id() && available && peer.status().in_zone==source.status().in_zone?&peer:nullptr;
    }
    void send_move_cancel(Actor& actor,const net::Packet& packet) override {require(&actor==&source && actor.status().in_zone,"Unexpected ordinary move-cancel route");events.push_back("M"+hex(packet));}
};
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected action cancellation oracle");std::ifstream input(argv[1]);require(input.good(),"Cannot open action cancellation oracle");
        const ItemDefinitions items;const EnchantValues enchant;const MobAttackTemplates mobs;const ActorEquipmentRules rules{items,enchant,mobs};
        unsigned int count=0;std::string line;
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;
            std::istringstream row(line);unsigned int op=0,available=0;std::uint32_t tick=0;row>>op>>tick>>available;
            const auto first=read(row),second=read(row),expected_first=read(row),expected_second=read(row);
            std::size_t event_count=0;row>>event_count;std::vector<std::string> expected(event_count);for(auto& event:expected) row>>event;
            require(static_cast<bool>(row) && op<3,"Malformed action cancellation vector");std::string extra;require(!(row>>extra),"Trailing action cancellation data");
            std::vector<std::string> events;Scene scene(events,tick),peer_scene(events,tick);
            Actor source(9001,{}, {},{},scene,rules),peer(9002,{}, {},{},peer_scene,rules);initialize(source,first);initialize(peer,second);World world(source,peer,events,available!=0);
            if(op==0) source.cancel_move(world);else if(op==1) source.cancel_vehicle(world);else source.stop_casting(world);
            const auto context=" at action vector "+std::to_string(count)+" operation "+std::to_string(op);
            require(snapshot(source)==expected_first,"Source action state differs"+context);require(snapshot(peer)==expected_second,"Peer action state differs"+context);
            require(events==expected,"Action notification order differs"+context);
            require(source.appearance().copied_user.has_value() && peer.appearance().copied_user.has_value(),"Action cancellation lost cached copied profile"+context);++count;
        }
        require(count==1536,"Action cancellation coverage changed");
        std::vector<std::string> events;Scene scene(events,0xfffffff0),peer_scene(events,0xfffffff0);EffectStatus status;
        status.in_zone=true;status.shape=101;status.fixed_form=1;status.action_block_b=1;status.current_target_type=1;status.current_target=9001;
        status.skills.equipment.attack_speed=2;status.skills.equipment.move_speed=2;
        Actor source(9001,status,{},{{300,1,30},{301,1,30}},scene,rules),peer(9002,{}, {},{},peer_scene,rules);peer.status().in_zone=true;World world(source,peer,events,true);
        source.appearance().vehicle_mode=2;source.appearance().vehicle_partner=9002;peer.appearance().vehicle_mode=3;peer.appearance().vehicle_partner=9001;
        auto& actions=source.actions();actions.vehicle_invitation=9002;actions.vehicle_invitation_until=55;actions.pending_move=1;actions.move_type=2;actions.move_slot=10;actions.move_slots[10]=123;
        actions.previous_target_type=2;actions.previous_target=777;source.attack_state().mode=3;source.attack_state().previous_tick=123;
        std::array<SkillDefinition,2> effects{};for(std::size_t i=0;i<effects.size();++i) {auto& skill=effects[i];skill.id=static_cast<std::uint16_t>(300+i);skill.level=1;skill.effect=3;skill.detail=i==0?90:66;skill.change_type=101;source.effects().append(static_cast<std::uint32_t>(2000+i),{{skill.id,1,30},0,30000,false},skill);}
        source.stop_casting(world);
        require(source.effects().size()==0 && source.replica().snapshot().empty() && source.status().shape==0 && source.status().fixed_form==0 && source.status().action_block_b==0,"Full action cancellation did not remove transform/sleep effects");
        require(source.attack_state().mode==2 && source.attack_state().next_tick==1 && source.attack_state().resume_tick==0xfffffff0 && source.status().current_target_type==2 && source.status().current_target==777,"Full action cancellation lost target restoration or resume tick");
        require(source.appearance().vehicle_partner==0 && peer.appearance().vehicle_partner==0 && source.appearance().vehicle_mode==0 && peer.appearance().vehicle_mode==0 && actions.vehicle_invitation==0 && actions.pending_move==0 && actions.move_slots[10]==0,"Full action cancellation retained vehicle/move state");
        require(events.size()==13 && events[0]=="Q9002" && events[1]=="C1c022a23000000000000" && events[2]=="Q9002" && events[3]=="C1d052923000065" && events[4]=="M22022923000000" && events[5]=="S0e05d0070000" && events[6]=="C1d052923000000" && events[8]=="F2000" && events[9]=="S0e05d1070000" && events[10]=="F2001" && events[12]=="T","Full action cancellation phase ordering changed");
        std::cout<<count<<" original two-actor action cancellations and concrete active-effect integration passed\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}

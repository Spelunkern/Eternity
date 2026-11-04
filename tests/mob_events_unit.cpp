#include "eternity/game/mob_events.hpp"
#include <array>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity::game;
using Fields = std::array<std::uint32_t,7>;
Fields fields(const MobRuntime& m) { return {static_cast<std::uint32_t>(m.lua_ai), static_cast<std::uint32_t>(m.only_lua_attack), m.combat_timeout, static_cast<std::uint32_t>(m.current_attack), static_cast<std::uint32_t>(m.move_mode), m.function_type, m.lua_attacked_count}; }
void assign(MobRuntime& m, const Fields& f) { m.lua_ai=static_cast<std::int32_t>(f[0]); m.only_lua_attack=static_cast<std::int32_t>(f[1]); m.combat_timeout=f[2]; m.current_attack=static_cast<std::int32_t>(f[3]); m.move_mode=static_cast<std::int32_t>(f[4]); m.function_type=f[5]; m.lua_attacked_count=f[6]; }
void check(bool ok) { if (!ok) throw std::runtime_error("Original mob event mismatch"); }
struct Services : MobEventServices {
    bool mutate=false; unsigned ticks=0; std::uint32_t now=0, call_tick=0, argument=0; bool has_arg=false;
    std::string name="-"; Fields observed{};
    std::uint32_t tick() override { ++ticks; return now; }
    void lua_call(MobRuntime& m, std::string_view function, std::uint32_t at, std::optional<std::uint32_t> value) override {
        name=function; call_tick=at; has_arg=value.has_value(); argument=value.value_or(0); observed=fields(m);
        if (mutate) assign(m, {100,101,102,103,104,105,106});
    }
};
int main(int argc, char** argv) {
    try {
        check(argc==2); std::ifstream input(argv[1]); check(input.good()); std::string line; unsigned count=0;
        while (std::getline(input,line)) {
            if (line.empty() || line[0]=='#') continue;
            std::istringstream row(line); unsigned kind,mutate,ticks,has_arg; std::uint32_t arg,now,call_tick,call_arg;
            Fields initial{},observed{},final{}; std::string function;
            row>>kind>>mutate>>arg>>now; for(auto& v:initial) row>>v;
            row>>ticks>>function>>call_tick>>has_arg>>call_arg;
            for(auto& v:observed) row>>v;
            for(auto& v:final) row>>v;
            check(!row.fail());
            MobRuntime mob; assign(mob,initial); mob.status=42; mob.state_stack=std::vector<std::int32_t>{7,8};
            Services services; services.now=now; services.mutate=mutate!=0;
            dispatch_mob_event(mob,static_cast<MobEvent>(kind),arg,services);
            check(fields(mob)==final && services.ticks==ticks && services.name==function && services.call_tick==call_tick && services.has_arg==(has_arg!=0) && services.argument==call_arg && services.observed==observed);
            check(mob.status==42 && *mob.state_stack==std::vector<std::int32_t>({7,8})); ++count;
        }
        check(count==512); std::cout<<count<<" original event cases matched\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}

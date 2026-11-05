#include "eternity/game/guild_house_shop.hpp"
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>

using namespace eternity::game;
namespace {
void check(bool value,const std::string& message) {if(!value) throw std::runtime_error(message);}
double hex_double(const std::string& text) {return std::strtod(text.c_str(),nullptr);}
}
int main(int argc,char** argv) {
    try {
        check(argc==2,"Expected the guild shop oracle");std::ifstream input(argv[1]);check(input.good(),"Cannot open the guild shop oracle");
        std::string line;std::size_t checked=0,rejected=0;
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#')continue;
            std::istringstream row(line);std::int32_t country=0;unsigned rank=0,npc=0,type=0;row>>country>>rank>>npc>>type;
            GuildHouseShopConfig config;std::string text;
            for(auto& rate:config.buy_rate) {row>>text;rate=hex_double(text);}
            for(auto& rate:config.potion_buy_rate) {row>>text;rate=hex_double(text);}
            for(auto& by_country:config.npc_ids) for(auto& by_type:by_country) for(auto& by_level:by_type) for(auto& id:by_level) row>>id;
            GuildNpcs npcs;for(auto& slot:npcs.slots) row>>slot.type>>slot.level;
            const auto result=guild_house_shop_buy_rate(npcs,country,static_cast<std::uint8_t>(rank),config,static_cast<std::uint16_t>(npc),static_cast<std::uint8_t>(type));
            std::uint32_t expected=0;unsigned detail=0;std::int32_t after=0;row>>expected>>text>>detail>>after;
            const auto context=" at guild shop vector "+std::to_string(checked);
            check(result.result==expected,"Result differs"+context);check(result.rate==hex_double(text),"Rate differs"+context);
            check(!expected || result.detail==detail,"Detail differs"+context);check(country==after,"Country differs"+context);
            for(const auto& slot:npcs.slots) {unsigned level=0;row>>level;check(slot.level==level,"Stored level differs"+context);}
            rejected+=expected!=0;++checked;
        }
        check(checked==4096 && rejected>200,"Guild shop coverage changed");
        // The real GuildHouse.ini shape replaces rates and NPC levels, keeping untouched cells.
        GuildHouseShopConfig loaded;
        guild_house_apply_shop_profile(loaded,"[ShopDefault]\nBuyDecreRate=0.95, 0.90, 0.85, 0.80\nBuyPotionDecreRate=0.5\n[NpcLevel_Light]\nShopWeapon2=198\n[NpcLevel_Dark]\nShopGeneral3=225\n");
        check(loaded.buy_rate==std::array<double,4>{0.95,0.90,0.85,0.80},"Buy rates were not loaded");
        check(loaded.potion_buy_rate==std::array<double,4>{0.5,0.8,0.7,0.6},"Missing rate tokens were not retained");
        check(loaded.npc_ids[0][1][2][0]==198 && loaded.npc_ids[1][3][3][0]==225 && loaded.npc_ids[0][1][1][0]==196,"NPC levels were not loaded");
        std::cout<<"Guild house shop rate matched "<<checked<<" original executions\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}

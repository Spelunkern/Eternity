#include "eternity/game/entry_delivery.hpp"

namespace eternity::game {
EntryDelivery::EntryDelivery(const PlayerSnapshot& player,const LoadedActor& loaded,const SocialCatalog& social,const EntryCatalogs& catalogs,ExperienceBounds experience,std::string guild_name,EntryDeliveryPorts ports):player_(player),loaded_(loaded),social_(social),catalogs_(catalogs),experience_(experience),guild_(std::move(guild_name)),ports_(std::move(ports)) {
    if(!loaded.actor || loaded.learned.invalid || loaded.actor->id()!=player.core.character.id || loaded.actor->account_id()!=player.core.user_id || social.account_id()!=player.core.user_id || social.character_id()!=player.core.character.id || catalogs.account_id()!=player.core.user_id) throw std::invalid_argument("Entry delivery requires matching loaded character/catalog ownership");
    if(!ports_.send_self || !ports_.send_peer || !ports_.social || !ports_.tick || !ports_.kill_count) throw std::invalid_argument("Incomplete entry delivery services");
}
bool EntryDelivery::send(CharacterEntryStep step) const {
    const auto batch=[&](std::vector<net::Packet> packets){for(auto& p:packets) ports_.send_self(std::move(p));};
    auto& actor=*loaded_.actor;using Step=CharacterEntryStep;
    switch(step) {
    case Step::init_friends:social_.notify_online(ports_.social,ports_.send_peer);break;
    case Step::data: {auto core=player_.core;core.money=actor.money();auto guild=ports_.guild_name?ports_.guild_name():guild_;ports_.send_self(character_data(core,actor.status().skills.equipment.base.maximum,experience_,guild));break;}
    case Step::items:batch(character_items(actor.inventory(),actor.status().skills.max_bags));break;
    case Step::bank:batch(character_bank(player_.bank,player_.core.stored_money));break;
    case Step::skills:ports_.send_self(character_skills(loaded_.learned.active,ports_.tick()));break;
    case Step::effect_packets:batch(actor.effects().initial_packets(ports_.tick()));break;
    case Step::quests: {std::vector<QuestPacketEntry> rows;for(const auto& quest:loaded_.quests.active) rows.push_back(quest.packet);batch(character_quests(rows,ports_.tick()));break;}
    case Step::finished_quests:batch(character_finished_quests(loaded_.quests.finished));break;
    case Step::friends:social_.send_packets(ports_.social,ports_.send_self);break;
    case Step::speed:ports_.send_self(character_speed(actor.id(),actor.status()));break;
    case Step::kc: {const auto country=actor.appearance().identity.country;const auto state=ports_.kill_count(country);ports_.send_self(character_kill_count(country,state.points,state.expires,state.expires?ports_.tick():0));break;}
    case Step::save_points:if(auto p=catalogs_.save_point_packet()) ports_.send_self(std::move(*p));break;
    case Step::honor:ports_.send_self(character_honor(player_.core.hunger));break;
    case Step::purchase_history:ports_.send_self(catalogs_.history_packet());break;
    case Step::stored_points:if(auto p=catalogs_.stored_packet()) ports_.send_self(std::move(*p));break;
    case Step::quick:ports_.send_self(character_quick_slots(player_.quick_slots));break;
    default:return false;
    }
    return true;
}
}

#include "eternity/game/guild_catalog.hpp"
#include <algorithm>

namespace eternity::game {
namespace {
std::uint32_t get(std::span<const std::uint8_t> p,std::size_t at,unsigned int n=4) {std::uint32_t v=0;for(unsigned int i=0;i<n;++i) v|=std::uint32_t{p[at+i]}<<(8*i);return v;}
std::string text(std::span<const std::uint8_t> p,std::size_t at,std::size_t width) {const auto value=p.subspan(at,width);const auto end=std::find(value.begin(),value.end(),0);if(end==value.end()) throw std::invalid_argument("Unterminated guild load string");return {value.begin(),end};}
}
GuildCatalog::GuildCatalog(GuildCatalogPorts port):ports_(std::move(port)) {if(!ports_.allocate_guild || !ports_.allocate_member || !ports_.transport.allocate || !ports_.transport.release || !ports_.transport.send) throw std::invalid_argument("Incomplete guild catalog services");}
std::string GuildCatalog::name_key(std::string_view value) {std::string key(value.substr(0,value.find('\0')));for(auto& c:key) if(c>='A' && c<='Z') c=static_cast<char>(c-'A'+'a');return key;}
LoadedGuild* GuildCatalog::find(std::uint32_t id) const {const auto it=ids_.find(id);return it==ids_.end()?nullptr:it->second;}
LoadedGuild* GuildCatalog::find(std::string_view name) const {const auto it=names_.find(name_key(name));return it==names_.end()?nullptr:it->second;}
std::map<std::uint32_t,GuildListing> GuildCatalog::listings() const {std::map<std::uint32_t,GuildListing> out;for(const auto& [id,guild]:ids_) out.emplace(id,guild->metadata.listing);return out;}
EntryCatalogUpdate GuildCatalog::apply(std::span<const std::uint8_t> p) {
    if(p.size()<2) return EntryCatalogUpdate::rejected;const auto op=get(p,0,2);
    if(op==0xb01) {if(p.size()<6) return EntryCatalogUpdate::rejected;maximum_=get(p,2);return EntryCatalogUpdate::applied;}
    if(op==0xb0e) {if(p.size()<7) return EntryCatalogUpdate::rejected;auto* guild=find(get(p,2));if(!guild) return EntryCatalogUpdate::applied;return guild->npcs.apply(p)?EntryCatalogUpdate::applied:EntryCatalogUpdate::rejected;}
    if(op==0xb0d) {if(p.size()<7) return EntryCatalogUpdate::rejected;auto* guild=find(get(p,2));if(!guild) return EntryCatalogUpdate::applied;if(!guild->bank) return EntryCatalogUpdate::unsupported;return guild->bank->apply(p)==GuildBankUpdate::applied?EntryCatalogUpdate::applied:EntryCatalogUpdate::rejected;}
    if(op!=0xb02 && op!=0xb03) return EntryCatalogUpdate::unsupported;
    const auto header=op==0xb02?3U:7U;if(p.size()<header) return EntryCatalogUpdate::rejected;
    LoadedGuild* guild=op==0xb03?find(get(p,2)):nullptr;if(op==0xb03 && !guild) return EntryCatalogUpdate::applied;
    const auto count=p[header-1];const auto width=op==0xb02?135U:29U;if(p.size()<header+count*width) return EntryCatalogUpdate::rejected;
    try {for(unsigned int i=0;i<count;++i) {
        const auto row=p.subspan(header+i*width,width);
        if(op==0xb02) {
            if(!ports_.allocate_guild()) continue;GuildMetadata data;
            auto& info=data.listing;info.id=get(row,0);info.name=text(row,4,25);info.master=text(row,29,21);info.country=row[50];info.points=get(row,51);info.rank=row[55];data.etin=get(row,56);data.use_house=row[60];info.notice=text(row,61,65);data.buy_house=row[126];data.keep_etin=get(row,127);data.return_count=get(row,131);
            auto owned=std::make_unique<LoadedGuild>(std::move(data),ports_.transport,ports_.bank);auto* value=owned.get();retained_.push_back(std::move(owned));ids_.emplace(value->metadata.listing.id,value);names_.emplace(name_key(value->metadata.listing.name),value);maximum_=std::max(maximum_,value->metadata.listing.id);
        } else {
            if(!ports_.allocate_member()) continue;auto member=std::make_shared<GuildMember>();member->id=get(row,0);member->level=static_cast<std::int32_t>(get(row,4,2));member->job=row[6];const auto raw_rank=row[7];member->rank=raw_rank;member->name=text(row,8,21);
            auto& metadata=guild->metadata;if(member->name==metadata.listing.master && member->rank!=1) member->rank=1;
            if(raw_rank==1) metadata.listing.master=member->name;
            if(raw_rank!=0) {guild->membership.offline.emplace(member->id,member);if(raw_rank<=2) ++metadata.officers;}
            else guild->membership.applications.emplace(member->id,member);
            directory_.add(member->id,&guild->membership);retained_members_.push_back(std::move(member));
        }
    }} catch(const std::invalid_argument&) {return EntryCatalogUpdate::rejected;}
    return EntryCatalogUpdate::applied;
}
}

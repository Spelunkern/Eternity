#pragma once
#include "eternity/game/entry_catalogs.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <limits>

namespace eternity::game {
// Retained ps_dbAgent CUser friend/block slots. LoadFriend 00425f80 writes
// ID, job, name and memo but leaves family/grow bytes and unused slots intact;
// LoadBlock 00426500 likewise leaves unused slots intact. A reused owner must
// retain this object across loads. SQL NULL friend/block names are deliberately
// rejected until the original reused ODBC scratch buffer is represented.
class VerticalSocialReplica {
public:
    explicit VerticalSocialReplica(std::uint32_t account):account_(account){}
    struct FriendSlot {std::uint32_t id=0;std::uint8_t family=0,grow=0,job=0;std::string name,memo;std::int32_t save_type=0;};
    struct BlockSlot {std::uint32_t id=0;std::string name,memo;std::int32_t save_type=0;};
    std::uint32_t account_id() const noexcept {return account_;}
    std::array<FriendSlot,100>& friends() noexcept {return friends_;}
    std::array<BlockSlot,100>& blocked() noexcept {return blocked_;}
    bool load_friends(std::span<const storage::Row> rows) {
        for(std::size_t i=0;i<std::min(rows.size(),friends_.size());++i) {
            const auto& row=rows[i];std::uint32_t id=0,family=0,grow=0,job=0,extra=0;
            if(row.size()!=7 || !number(row[0],id,true) || !row[1] || !number(row[2],family,false) || !number(row[3],grow,false) || !number(row[4],job,false) || !number(row[6],extra,false))return false;
            auto& slot=friends_[i];slot.save_type=0;slot.id=id;slot.job=static_cast<std::uint8_t>(job);slot.name=bounded(*row[1],20);slot.memo=bounded(row[5].value_or(""),50);
        }
        return true;
    }
    bool load_blocked(std::span<const storage::Row> rows) {
        for(std::size_t i=0;i<std::min(rows.size(),blocked_.size());++i) {
            const auto& row=rows[i];std::uint32_t id=0;if(row.size()!=3 || !number(row[0],id,true) || !row[1])return false;
            auto& slot=blocked_[i];slot.save_type=0;slot.id=id;slot.name=bounded(*row[1],20);slot.memo=bounded(row[2].value_or(""),50);
        }
        return true;
    }
    const std::vector<std::uint32_t>& deleted_friends() const noexcept {return deleted_friends_;}
    const std::vector<std::uint32_t>& deleted_blocks() const noexcept {return deleted_blocks_;}
    // Call only after the corresponding save transaction has committed.
    void acknowledge_friends() {deleted_friends_.clear();for(auto& slot:friends_)if(slot.id)slot.save_type=0;}
    void acknowledge_blocks() {deleted_blocks_.clear();for(auto& slot:blocked_)if(slot.id)slot.save_type=0;}
    // PacketUserFriend 00406930. The envelope is validated before mutation;
    // original accepts duplicate IDs and acts on the first matching slot.
    EntryCatalogUpdate apply_db(std::span<const std::uint8_t> packet) {
        if(packet.size()<6 || get(packet,2)!=account_)return EntryCatalogUpdate::rejected;
        const auto op=static_cast<unsigned>(packet[0])|(static_cast<unsigned>(packet[1])<<8);
        if(op!=0xc02 && op!=0xc03 && op!=0xc04 && op!=0xc06 && op!=0xc07 && op!=0xc08)return EntryCatalogUpdate::unsupported;
        if(packet.size()<10)return EntryCatalogUpdate::rejected;const auto id=get(packet,6);
        std::string text;
        if(op==0xc02 || op==0xc04 || op==0xc06 || op==0xc08) {
            const auto offset=op==0xc02?13U:10U;const auto maximum=(op==0xc04 || op==0xc08)?50U:20U;
            if(offset>=packet.size())return EntryCatalogUpdate::rejected;
            auto end=std::find(packet.begin()+offset,packet.end(),0);if(end==packet.end() || static_cast<std::size_t>(end-(packet.begin()+offset))>maximum)return EntryCatalogUpdate::rejected;
            text.assign(packet.begin()+offset,end);
        }
        if(op==0xc02) {
            const auto it=std::find_if(friends_.begin(),friends_.end(),[](const auto& row){return row.id==0;});
            if(it!=friends_.end()){it->id=id;it->family=packet[10];it->grow=packet[11];it->job=packet[12];it->name=std::move(text);it->memo.clear();it->save_type=1;}return EntryCatalogUpdate::applied;
        }
        if(op==0xc06) {
            const auto it=std::find_if(blocked_.begin(),blocked_.end(),[](const auto& row){return row.id==0;});
            if(it!=blocked_.end()){it->id=id;it->name=std::move(text);it->memo.clear();it->save_type=1;}return EntryCatalogUpdate::applied;
        }
        if(op==0xc03 || op==0xc04) {
            const auto it=std::find_if(friends_.begin(),friends_.end(),[&](const auto& row){return row.id==id;});
            if(it!=friends_.end()) {if(op==0xc03){if(it->save_type!=1)deleted_friends_.push_back(id);it->id=0;}
                else {std::replace(text.begin(),text.end(),'\'',' ');it->memo=std::move(text);if(it->save_type==0)it->save_type=2;}}return EntryCatalogUpdate::applied;
        }
        const auto it=std::find_if(blocked_.begin(),blocked_.end(),[&](const auto& row){return row.id==id;});
        if(it!=blocked_.end()) {if(op==0xc07){if(it->save_type!=1)deleted_blocks_.push_back(id);it->id=0;}
            else {std::replace(text.begin(),text.end(),'\'',' ');it->memo=std::move(text);if(it->save_type==0)it->save_type=2;}}return EntryCatalogUpdate::applied;
    }
    // SendCharacterFriend 0040a7f0 scans all 100 retained slots, filters ID 0
    // and emits batches of 25; game-side apply_db preserves original order.
    SocialCatalog game_catalog(std::uint32_t character) const {
        SocialCatalog result(account_,character);net::Packet packet;
        const auto start=[&](std::uint8_t op){packet={op,0x0c};put(packet,account_);packet.push_back(0);};
        const auto flush=[&](){if(packet[6] && result.apply_db(packet)!=EntryCatalogUpdate::applied)throw std::logic_error("Social DB packet rejected");};
        start(1);for(const auto& row:friends_)if(row.id){put(packet,row.id);packet.insert(packet.end(),{row.family,row.grow,row.job});fixed(packet,row.name,21);fixed(packet,row.memo,51);if(++packet[6]==25){flush();start(1);}}flush();
        start(5);for(const auto& row:blocked_)if(row.id){put(packet,row.id);fixed(packet,row.name,21);fixed(packet,row.memo,51);if(++packet[6]==25){flush();start(5);}}flush();return result;
    }
private:
    static bool number(const storage::Cell& cell,std::uint32_t& out,bool dword) {
        if(!cell)return false;std::int64_t value=0;const auto parsed=std::from_chars(cell->data(),cell->data()+cell->size(),value);
        if(parsed.ec!=std::errc{} || parsed.ptr!=cell->data()+cell->size() || value<(dword?std::numeric_limits<std::int32_t>::min():0) || value>(dword?static_cast<std::int64_t>(std::numeric_limits<std::uint32_t>::max()):255))return false;
        out=static_cast<std::uint32_t>(value);return true;
    }
    static std::string bounded(std::string value,std::size_t limit) {const auto end=value.find('\0');if(end!=std::string::npos)value.resize(end);if(value.size()>limit)value.resize(limit);return value;}
    static std::uint32_t get(std::span<const std::uint8_t> p,std::size_t at){std::uint32_t v=0;for(unsigned i=0;i<4;++i)v|=std::uint32_t(p[at+i])<<(8*i);return v;}
    static void put(net::Packet& p,std::uint32_t value){for(unsigned i=0;i<4;++i)p.push_back(static_cast<std::uint8_t>(value>>(8*i)));}
    static void fixed(net::Packet& p,const std::string& value,std::size_t size){const auto end=std::min(value.find('\0'),value.size());const auto count=std::min(end,size-1);p.insert(p.end(),value.begin(),value.begin()+count);p.resize(p.size()+size-count,0);}
    std::uint32_t account_;std::array<FriendSlot,100> friends_{};std::array<BlockSlot,100> blocked_{};std::vector<std::uint32_t> deleted_friends_,deleted_blocks_;
};
}

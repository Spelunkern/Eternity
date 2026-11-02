#include "eternity/game/maps.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <fstream>
#include <limits>
#include <stdexcept>

namespace eternity::game {
bool CellViewBounds::contains(float x,float z) const noexcept {
    // Original x87 status tests allow unordered comparisons through both bounds.
    return !(x<minimum_x || x>maximum_x || z<minimum_z || z>maximum_z);
}
MapGeometry::MapGeometry(std::int32_t size):size_(size) {if(size<0) throw std::invalid_argument("Negative original map size");}
bool MapGeometry::safe_position(float x,float z) const noexcept {
    // CMap::IsSafePos 00415360 uses float(size), inclusive zero and exclusive size.
    const auto limit=static_cast<float>(size_);return !(x<0 || x>=limit || z<0 || z>=limit);
}
std::optional<CellCoordinates> MapGeometry::cell(float x,float z) const noexcept {
    if(!safe_position(x,z) || !std::isfinite(x) || !std::isfinite(z)) return {};
    // CZone::EnterUser 00415fa8: divide at 53-bit precision, truncate toward zero.
    const CellCoordinates result{static_cast<std::int32_t>(static_cast<double>(x)/40.0),static_cast<std::int32_t>(static_cast<double>(z)/40.0)};
    if(result.x<0 || result.z<0 || result.x>=cell_count() || result.z>=cell_count()) return {};
    return result;
}
std::optional<CellCoordinates> MapGeometry::cell_after_move(CellCoordinates previous,float x,float z) const {
    // MoveUserLocal 00416370 retains membership while the position remains inside the old view window.
    if(!safe_position(x,z)) return {};
    if(view(previous).contains(x,z)) return previous;
    return cell(x,z);
}
CellViewBounds MapGeometry::view(CellCoordinates cell) const {
    if(cell.x<0 || cell.z<0 || cell.x>=cell_count() || cell.z>=cell_count()) throw std::out_of_range("Cell outside original map");
    // CZone::Create 00415d31: a 60-unit view window overlapping the 40-unit cell.
    const auto x=static_cast<std::int64_t>(cell.x)*40,z=static_cast<std::int64_t>(cell.z)*40;
    return {static_cast<float>(x-10),static_cast<float>(x+50),static_cast<float>(z-10),static_cast<float>(z+50)};
}
namespace {
class Reader {
public:
    explicit Reader(std::span<const std::uint8_t> bytes):bytes_(bytes) {}
    std::span<const std::uint8_t> take(std::size_t count) {
        if(count>bytes_.size()-at_) throw std::runtime_error("Truncated original map file");
        const auto result=bytes_.subspan(at_,count);at_+=count;return result;
    }
    std::uint32_t number(unsigned int width=4) {const auto bytes=take(width);std::uint32_t value=0;for(unsigned int i=0;i<width;++i) value|=static_cast<std::uint32_t>(bytes[i])<<(8*i);return value;}
    std::int32_t integer() {return std::bit_cast<std::int32_t>(number());}
    float real() {return std::bit_cast<float>(number());}
    UserPosition position() {return {real(),real(),real()};}
    MapBox box() {return {position(),position()};}
    std::size_t count(std::size_t minimum_width) {
        const auto value=integer();
        if(value<0 || static_cast<std::size_t>(value)>(bytes_.size()-at_)/minimum_width) throw std::runtime_error("Invalid original map record count");
        return static_cast<std::size_t>(value);
    }
    std::size_t consumed() const noexcept {return at_;}
private:
    std::span<const std::uint8_t> bytes_;std::size_t at_=0;
};
std::int32_t extent_delta(float end,float start) {
    const auto value=std::trunc(static_cast<double>(end)-static_cast<double>(start));
    // x87 FISTP produces the integer-indefinite DWORD on overflow/nonfinite; the loader then clamps to 1.
    if(!std::isfinite(value) || value>INT32_MAX || value<INT32_MIN) return 1;
    return std::max(static_cast<std::int32_t>(value),1);
}
}
MapDefinition MapDefinition::decode(std::span<const std::uint8_t> bytes) {
    // CMap::LoadMap 00425780, CBitArray::Create 00425250. Runtime object pointers are resolved separately.
    Reader reader(bytes);MapDefinition result;result.size=reader.integer();
    if(result.size<0 || static_cast<std::uint64_t>(result.size)*static_cast<std::uint64_t>(result.size)>INT32_MAX) throw std::runtime_error("Invalid original map extent");
    const auto tile_bytes=(static_cast<std::size_t>(result.size)*static_cast<std::size_t>(result.size)+7)/8;
    const auto tile_data=reader.take(tile_bytes);result.tiles.assign(tile_data.begin(),tile_data.end());
    result.doors.resize(reader.count(20));for(auto& door:result.doors) {door.position=reader.position();door.auto_close=reader.integer();door.close_time=reader.integer();}
    result.ladders.resize(reader.count(12));for(auto& ladder:result.ladders) ladder=reader.position();
    result.mobs.resize(reader.count(28));for(auto& area:result.mobs) {
        area.box=reader.box();area.spawns.resize(reader.count(8));area.delta_x=extent_delta(area.box.maximum.x,area.box.minimum.x);area.delta_z=extent_delta(area.box.maximum.z,area.box.minimum.z);
        for(auto& spawn:area.spawns) {spawn.mob_id=reader.integer();spawn.count=reader.integer();}
    }
    result.npcs.resize(reader.count(12));for(auto& npc:result.npcs) {
        npc.type=reader.integer();npc.type_id=reader.integer();npc.positions.resize(reader.count(16));
        for(auto& position:npc.positions) {
            position.position=reader.position();const auto angle=reader.real();
            position.angle=static_cast<float>(-static_cast<double>(angle)*static_cast<double>(std::bit_cast<float>(UINT32_C(0x42652ee1))));
            if(position.angle<0) position.angle+=360.0F;
        }
    }
    result.portals.resize(reader.count(36));for(auto& portal:result.portals) {
        portal.position=reader.position();portal.country=reader.integer();portal.base_country=portal.country;portal.id=portal.country<1001?-1:portal.country;
        portal.minimum_level=static_cast<std::int32_t>(reader.number(2));portal.maximum_level=static_cast<std::int32_t>(reader.number(2));
        portal.map_id=reader.integer();portal.destination=reader.position();
    }
    result.binds.resize(reader.count(12));for(auto& bind:result.binds) {
        const auto count=reader.integer();bind.country=reader.integer();bind.attribute=reader.integer();
        if(count<0 || static_cast<std::size_t>(count)>(bytes.size()-reader.consumed())/24) throw std::runtime_error("Invalid original map bind count");
        bind.boxes.resize(static_cast<std::size_t>(count));for(auto& box:bind.boxes) box=reader.box();
    }
    result.others.resize(reader.count(32));for(auto& other:result.others) {
        other.box=reader.box();other.type=reader.integer();other.attribute1=static_cast<std::uint8_t>(reader.number(1));other.attribute2=static_cast<std::uint8_t>(reader.number(1));other.attribute3=static_cast<std::uint16_t>(reader.number(2));
    }
    result.consumed=reader.consumed();return result;
}
MapDefinition MapDefinition::load(const std::filesystem::path& path) {
    std::ifstream input(path,std::ios::binary);if(!input) throw std::runtime_error("Cannot open original map file");
    const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(input)),std::istreambuf_iterator<char>());
    return decode(bytes);
}
}

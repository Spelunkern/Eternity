#pragma once
#include "eternity/game/maps.hpp"
#include "eternity/net/framing.hpp"
#include <array>
#include <memory>

namespace eternity::game {
class Actor;
class ZoneCorpses;
struct Corpse {
    UserPosition position;
    std::uint32_t id=0;
    ZoneCorpses* zone=nullptr;
    std::optional<CellCoordinates> cell;
    std::uint16_t direction=0;
    std::array<std::uint8_t,7> appearance{}; // country, family, hair, face, size, job, sex.
    std::array<std::uint8_t,8> equipment_type{},equipment_id{};
    std::array<std::uint8_t,21> name{};
};
// Original CDead heap is LIFO and does not clear reused objects.
class CorpsePool {
public:
    Corpse* allocate();
    void release(Corpse&);
private:
    std::vector<std::unique_ptr<Corpse>> objects_;
    std::vector<Corpse*> available_;
};
void capture_corpse(Corpse&,const Actor&,UserPosition,std::uint16_t direction);
net::Packet corpse_appearance(const Corpse&);
net::Packet corpse_removed(std::uint32_t id);
enum class CorpseIssue {duplicate,missing,wrong_zone};
class CorpseViewer {
public:
    virtual ~CorpseViewer() = default;
    virtual void send_self(const net::Packet&) = 0;
};
class ZoneCorpseServices {
public:
    virtual ~ZoneCorpseServices() = default;
    virtual Corpse* allocate() = 0;
    virtual void release(Corpse&) = 0;
    virtual void diagnostic(CorpseIssue,std::uint32_t id) = 0;
    virtual void send_cell_view(CellCoordinates,const net::Packet&) = 0;
};
class WorldCorpses {
public:
    Corpse* find(std::uint32_t id) const noexcept;
    bool leave(std::uint32_t id);
    std::size_t size() const noexcept {return index_.size();}
private:
    friend class ZoneCorpses;
    std::map<std::uint32_t,Corpse*> index_;
};
// Serialized world executor. The geometry, global registry, services and allocated
// bodies must outlive this cell view. Its owner releases bodies before zone reuse.
class ZoneCorpses {
public:
    ZoneCorpses(const MapGeometry&,WorldCorpses&,ZoneCorpseServices&);
    ~ZoneCorpses();
    ZoneCorpses(const ZoneCorpses&) = delete;
    ZoneCorpses& operator=(const ZoneCorpses&) = delete;
    Corpse* create(const Actor&,UserPosition,std::uint16_t direction);
    bool enter(Corpse&);
    bool leave(Corpse&);
    Corpse* find(std::uint32_t id);
    Corpse* find_cell(CellCoordinates,std::uint32_t id);
    void send_enter(CellCoordinates,CorpseViewer&);
    void send_leave(CellCoordinates,CorpseViewer&);
    std::size_t size() const noexcept {return members_.size();}
private:
    struct Cell {std::map<std::uint64_t,Corpse*> members;std::uint64_t cursor=0;};
    std::size_t key(CellCoordinates) const;
    void send(CellCoordinates,CorpseViewer&,bool entering);
    const MapGeometry& geometry_;WorldCorpses& world_;ZoneCorpseServices& services_;
    std::map<std::size_t,Cell> cells_;
    std::map<Corpse*,std::uint64_t> members_;
    std::uint64_t sequence_=0;
};
}

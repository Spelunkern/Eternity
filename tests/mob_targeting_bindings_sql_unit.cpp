#include "eternity/game/mob_targeting_bindings.hpp"
#include <charconv>
#include <fstream>
#include <iostream>
#include <sstream>
using namespace eternity;
void require(bool b,const char* message) {if(!b) throw std::runtime_error(message);}
storage::ResultSet query(storage::SqlServer& db,const char* sql) {
    auto tables=db.query(sql);require(tables.size()==1,"Original binding procedure result count changed");return std::move(tables.front());
}
std::uint16_t maximum(const storage::ResultSet& table) {
    require(table.rows.size()==1 && table.rows[0].size()==1 && table.rows[0][0],"Original binding MaxID scalar missing");
    const auto& text=*table.rows[0][0];unsigned value=0;const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
    require(parsed.ec==std::errc{} && parsed.ptr==text.data()+text.size() && value<=65535,"Original binding MaxID outside supported domain");return static_cast<std::uint16_t>(value);
}
void rows(std::ostream& out,char tag,const storage::ResultSet& table,std::size_t width,std::size_t name_column) {
    require(table.columns.size()==width,"Restored binding procedure width changed");
    for(const auto& row:table.rows) {
        require(row.size()==width,"Restored binding row width changed");out<<tag;
        for(std::size_t i=0;i<row.size();++i) {
            // Names are not part of the numeric projection; original oracle's
            // SQL_C_CHAR port supplies a fixed harmless name instead.
            if(i==name_column) {out<<" 0";continue;}
            out<<' ';if(row[i]) out<<*row[i];else out<<'N';
        }
        out<<'\n';
    }
}
std::vector<std::string> projection(const game::MobTargetingBindingCatalog& data) {
    std::vector<std::string> result;
    for(std::size_t i=1;i<data.skill_slots();++i) {
        std::ostringstream line;const auto* s=data.skill(static_cast<std::uint32_t>(i));
        line<<"P "<<i<<' '<<(s?s->id:0)<<' '<<(s?unsigned(s->effect):0)<<' '<<(s?s->detail:0)<<' '<<(s?s->cooldown_seconds:0)<<' '<<(s?unsigned(s->level):0);result.push_back(line.str());
    }
    for(std::size_t i=1;i<data.mob_slots();++i) {
        const auto* d=data.mob(static_cast<std::uint32_t>(i));if(!d) continue;std::ostringstream line;
        line<<"D "<<d->id<<' '<<unsigned(d->ai)<<' '<<unsigned(d->chase_range)<<' '<<d->country;
        for(const auto& a:d->attacks) {
            const auto* s=a.skill;line<<' '<<a.type<<' '<<unsigned(a.enabled)<<' '<<a.time<<' '<<unsigned(a.range)<<' '<<a.damage<<' '<<a.damage_add<<' '<<unsigned(a.attribute)<<' '<<unsigned(a.special)<<' '<<(s?s->id:0)<<' '<<(s?s->cooldown_seconds:0)<<' '<<(s?unsigned(s->level):0);
        }
        result.push_back(line.str());
    }
    return result;
}
// Read-only integration binary; intentionally NOT an ordinary credential-free
// CTest. Use tools/mob_targeting_bindings_sql_oracle.py for the shared SQL lock.
int main(int argc,char** argv) {
    try {
        require(argc==2 || argc==3,"Expected ignored SQL snapshot path and optional original reference");
        storage::SqlServer db(storage::SqlServerConfig::from_environment());
        const auto skill_max=maximum(query(db,"EXEC dbo.usp_Read_MobSkillMaxID_R;"));
        const auto skills=query(db,"EXEC dbo.usp_Read_MobSkills_R;");
        const auto mob_max=maximum(query(db,"EXEC dbo.usp_Read_MobMaxID_R;"));
        const auto mobs=query(db,"EXEC dbo.usp_Read_Mobs_R;");
        require(!skills.rows.empty() && !mobs.rows.empty(),"Restored binding catalog unexpectedly empty");
        const auto data=game::MobTargetingBindingCatalog::load(mob_max,skill_max,mobs,skills);
        std::ofstream snapshot(argv[1]);require(snapshot.good(),"Cannot write ignored binding snapshot");
        snapshot<<"# Read-only original procedure numeric rows; names intentionally replaced.\nS 0 "<<mob_max<<' '<<skill_max<<'\n';
        rows(snapshot,'K',skills,82,2);rows(snapshot,'M',mobs,69,1);snapshot<<"E\n";snapshot.close();require(snapshot.good(),"Cannot finish binding snapshot");
        const auto native=projection(data);std::size_t records=0,pointers=0,disabled=0;
        for(std::size_t i=1;i<data.mob_slots();++i) if(const auto* d=data.mob(static_cast<std::uint32_t>(i))) {
            ++records;for(const auto& a:d->attacks) if(a.skill) {++pointers;if(!a.enabled) ++disabled;}
        }
        if(argc==3) {
            std::ifstream reference(argv[2]);require(reference.good(),"Cannot read original binding reference");std::vector<std::string> original;
            for(std::string line;std::getline(reference,line);) if(line.starts_with("P ") || line.starts_with("D ")) {
                std::istringstream row(line);std::string token,normalized;while(row>>token) {if(!normalized.empty()) normalized+=' ';normalized+=token;}original.push_back(std::move(normalized));
            }
            require(original.size()==native.size(),"Original complete SQL binding record count differs");
            for(std::size_t i=0;i<native.size();++i) if(native[i]!=original[i]) throw std::runtime_error("Original restored SQL binding differs at record "+std::to_string(i));
        }
        std::cout<<skills.rows.size()<<" restored MobSkills rows, "<<data.skill_slots()<<" skill slots; "<<mobs.rows.size()<<" Mobs rows, "<<data.mob_slots()<<" mob slots, "<<records<<" populated definitions; "<<pointers<<" skill links ("<<disabled<<" disabled)"<<(argc==3?" matched original execution":" exported for original execution")<<"\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}

#pragma once
#include "eternity/game/equipment.hpp"

inline eternity::storage::ResultSet item_definitions() {
    using namespace eternity::storage;
    ResultSet table;table.columns={"ItemID","Type","TypeID","Range","ReqWis","AttackTime","Speed","Attrib","Special","Exp","Server",
        "Quality","Effect1","Effect2","Effect3","Effect4","ConstStr","ConstDex","ConstRec","ConstInt","ConstWis","ConstLuc","ConstHP","ConstMP","ConstSP"};
    for(unsigned int type:{5U,16U,21U,45U}) table.rows.push_back({std::to_string(type*1000+1),std::to_string(type),"1","4","50","2","1","1","0","0","1",
        "1000","1001","1002","1003","1004","1","2","3","4","5","6","101","303","202"});
    table.rows.push_back({"30001","30","1","2","0","1","3","1","4","5","0","0","31","32","33","34","11","12","13","14","15","16","21","23","22"});
    table.rows.push_back({"30002","30","2","0","0","0","0","0","16","0","0","0","0","0","0","0","1","2","3","4","5","6","7","9","8"});
    table.rows.push_back({"30003","30","3","0","0","0","0","2","1","0","0","0","0","0","0","0","0","0","0","0","0","0","0","0","0"});
    table.rows.push_back({"30004","30","4","0","0","0","0","1","0","0","0","0","0","0","0","0","0","0","0","0","0","0","0","0","0"});
    return table;
}

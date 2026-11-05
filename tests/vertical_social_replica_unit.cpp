#include "eternity/game/character_entry.hpp"
#include "eternity/game/vertical_social_db_route.hpp"
#include "eternity/game/vertical_social_world_release.hpp"
#include "eternity/game/vertical_social_dispatch.hpp"
#include "eternity/game/vertical_social_owner.hpp"
#include "eternity/game/social_friend_request.hpp"
#include "eternity/game/social_block_add.hpp"
#include "eternity/game/vertical_social_replica.hpp"
#include "eternity/game/vertical_social_save.hpp"
#include "eternity/game/vertical_social_lifecycle.hpp"
#include <iostream>
#include <tuple>
using namespace eternity;using namespace eternity::game;
static void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
struct FakeDb {
 bool active=false,rolled_back=false,committed=false;std::string fail;std::vector<std::string> calls;std::vector<std::vector<storage::Parameter>> args;
 bool transaction_active() const{return active;}
 void begin(){active=true;} void commit(){active=false;committed=true;} void rollback(){active=false;rolled_back=true;}
 void query(std::string sql,std::span<const storage::Parameter> p){calls.push_back(sql);args.emplace_back(p.begin(),p.end());if(!fail.empty() && sql.find(fail)!=std::string::npos)throw storage::SqlError("failure","HY000",1);}
};
struct LifecycleStore {
 UserCharacterSaveResult save(CharacterReplica&,bool,bool,bool,std::string_view,const CharacterSaveCallbacks& cb,const std::function<void(std::string_view)>&){cb.save_friends();cb.save_blocks();return {.legacy_success=true};}
 UserCharacterSaveResult save_exit(CharacterReplica&,bool,std::string_view,const CharacterSaveCallbacks& cb,const std::function<void(std::string_view)>&){cb.save_friends();cb.save_blocks();cb.exit();return {.legacy_success=true};}
};
int main(){try {
 VerticalSocialReplica replica(42);replica.friends()[0].family=7;replica.friends()[0].grow=8;replica.friends()[2]={77,3,4,5,"old","memo"};
 std::vector<storage::Row> friends{{"5"," Alice ","1","2","3",std::nullopt,"0"},{"0","none","0","0","1","m","0"}};
 std::vector<storage::Row> blocks{{"9","Blocked",std::nullopt}};check(replica.load_friends(friends) && replica.load_blocked(blocks),"load failed");
 auto catalog=replica.game_catalog(99);check(catalog.friends().size()==2 && catalog.friends()[0].id==5 && catalog.friends()[0].family==7 && catalog.friends()[0].grow==8 && catalog.friends()[0].job==3 && catalog.friends()[0].name==" Alice " && catalog.friends()[0].memo.empty() && catalog.friends()[1].id==77 && catalog.blocked().size()==1,"retained social slots differ");
 friends[0][1]=std::nullopt;check(!replica.load_friends(friends) && replica.friends()[0].id==5,"NULL name corrupted retained slot");
 for(unsigned i=0;i<100;++i)replica.friends()[i]={i+1,0,0,1,"N",""};catalog=replica.game_catalog(99);check(catalog.friends().size()==100 && catalog.friends()[99].id==100,"25-row social batches lost entries");
 VerticalSocialReplica edits(42);auto command=[](std::uint8_t op,std::uint32_t id){return net::Packet{op,12,42,0,0,0,static_cast<std::uint8_t>(id),static_cast<std::uint8_t>(id>>8),static_cast<std::uint8_t>(id>>16),static_cast<std::uint8_t>(id>>24)};};
 auto add=command(2,10);add.insert(add.end(),{1,2,3,'A',0});check(edits.apply_db(add)==EntryCatalogUpdate::applied && edits.friends()[0].id==10 && edits.friends()[0].save_type==1,"friend insert failed");
 VerticalSocialReplica full(42);for(unsigned i=0;i<100;++i){full.friends()[i].id=i+1;full.blocked()[i].id=i+1;}
 check(full.apply_db(add)==EntryCatalogUpdate::applied && full.friends()[99].id==100,"full friend table changed");
 auto full_block=command(6,200);full_block.insert(full_block.end(),{'B',0});check(full.apply_db(full_block)==EntryCatalogUpdate::applied && full.blocked()[99].id==100,"full block table changed");
 auto memo=command(4,10);memo.insert(memo.end(),{'x','\'','y',0});check(edits.apply_db(memo)==EntryCatalogUpdate::applied && edits.friends()[0].memo=="x y" && edits.friends()[0].save_type==1,"new friend memo marker differs");
 check(edits.apply_db(command(3,10))==EntryCatalogUpdate::applied && edits.deleted_friends().empty(),"unsaved friend deletion journaled");
 edits.friends()[0]={11,0,0,3,"P","",0};memo=command(4,11);memo.insert(memo.end(),{'z',0});check(edits.apply_db(memo)==EntryCatalogUpdate::applied && edits.friends()[0].save_type==2,"persisted friend memo not dirty");
 check(edits.apply_db(command(3,11))==EntryCatalogUpdate::applied && edits.deleted_friends()==std::vector<std::uint32_t>{11},"persisted friend deletion lost");
 auto block=command(6,12);block.insert(block.end(),{'B',0});check(edits.apply_db(block)==EntryCatalogUpdate::applied && edits.blocked()[0].save_type==1,"block add failed");
 check(edits.apply_db(command(7,12))==EntryCatalogUpdate::applied && edits.deleted_blocks().empty(),"unsaved block deletion journaled");
 edits.blocked()[0]={13,"B","",0};memo=command(8,13);memo.insert(memo.end(),{'a','\'','b',0});check(edits.apply_db(memo)==EntryCatalogUpdate::applied && edits.blocked()[0].memo=="a b" && edits.blocked()[0].save_type==2,"block memo differs");
 check(edits.apply_db(command(7,13))==EntryCatalogUpdate::applied && edits.deleted_blocks()==std::vector<std::uint32_t>{13},"block deletion lost");
 edits.friends()[1]={21,4,5,6,"New","ignored",1};edits.friends()[2]={22,0,0,0,"Old","note",2};
 FakeDb db;VerticalSocialSave saver(db,"test.");db.fail="Create_Friend";
 check(!saver.friends(edits,99) && db.rolled_back && edits.deleted_friends().size()==1 && edits.friends()[1].save_type==1,"failed save discarded changes");
 check(db.calls.size()==2 && db.calls[0].find("Delete_Friend")!=std::string::npos && db.calls[1].find("Create_Friend")!=std::string::npos,"save order differs");
 db.calls.clear();db.args.clear();db.fail="FriendMemo";db.rolled_back=false;
 check(saver.friends(edits,99) && db.committed && edits.deleted_friends().empty() && edits.friends()[1].save_type==0 && edits.friends()[2].save_type==0,"memo error handling differs");
 check(db.calls.size()==3 && db.calls[2].find("FriendMemo")!=std::string::npos && std::get<std::int64_t>(db.args[1][3])==4 && std::get<std::int64_t>(db.args[1][4])==5 && std::get<std::int64_t>(db.args[1][5])==6,"friend save arguments differ");
 db.calls.clear();db.fail.clear();edits.blocked()[1]={31,"NewBlock","",1};edits.blocked()[2]={32,"OldBlock","note",2};
 check(saver.blocks(edits,99) && db.calls.size()==3 && db.calls[0].find("Delete_Ban")!=std::string::npos && db.calls[1].find("Create_Ban")!=std::string::npos && db.calls[2].find("BandMemo")!=std::string::npos && edits.deleted_blocks().empty(),"block save differs");
 PlayerCore core{};core.user_id=42;core.character.id=123;InventoryReplicaPool pool;CharacterReplica character(core,pool);std::recursive_mutex mutex;
 VerticalSocialLifecycle bridge(edits,character,db,mutex,"test.");CharacterSaveLifecyclePorts ports;unsigned errors=0;ports.diagnostic=[&](auto){++errors;};ports=bridge.bind(std::move(ports));
 auto bridge_add=command(2,44);bridge_add.insert(bridge_add.end(),{1,2,3,'Z',0});check(bridge.apply(bridge_add)==EntryCatalogUpdate::applied,"social lifecycle rejected packet");
 db.calls.clear();db.args.clear();db.fail="Create_Friend";ports.save_friends();check(errors==1 && edits.friends()[0].save_type==1,"failed lifecycle save lost pending friend");
 db.fail.clear();ports.save_friends();check(edits.friends()[0].save_type==0 && std::get<std::int64_t>(db.args.back()[0])==123,"lifecycle save used wrong character");
 PlayerCore foreign_core{};foreign_core.user_id=43;CharacterReplica foreign(foreign_core,pool);bool mismatch=false;try{VerticalSocialLifecycle bad(edits,foreign,db,mutex,"test.");}catch(const std::invalid_argument&){mismatch=true;}check(mismatch,"foreign social owner accepted");
 auto final_block=command(6,45);final_block.insert(final_block.end(),{'Q',0});check(bridge.apply(final_block)==EntryCatalogUpdate::applied,"lifecycle block packet rejected");
 UserLifecycleState state{42,2,0,true};LifecycleStore lifecycle_store;std::vector<std::uint32_t> jobs;unsigned leaves=0;
 ports.post=[&](auto job){jobs.push_back(job);};ports.send=[](auto){};ports.leave=[&]{++leaves;};ports.reset=[]{};ports.roster=[](bool){};
 CharacterSaveLifecycle<LifecycleStore> lifecycle(state,character,lifecycle_store,mutex,"account",ports);
 db.calls.clear();check(lifecycle.disconnect(vertical_database_disconnect_packet(42)) && jobs==std::vector<std::uint32_t>{9} && lifecycle.dispatch(9),"social disconnect lifecycle failed");
 check(leaves==1 && edits.blocked()[0].save_type==0 && db.calls.size()==1 && db.calls[0].find("Create_Ban")!=std::string::npos,"social save was not run before leave");
 SocialCatalog block_owner(42,123); VerticalSocialReplica block_db(42); std::vector<net::Packet> block_responses; unsigned lookups=0;
 auto block_lookup=[&](std::string_view name)->std::optional<SocialBlockTarget>{++lookups;if(name=="Target")return SocialBlockTarget{88,99,1,"Target",false};return std::nullopt;};
 auto block_send_db=[&](net::Packet p){check(block_db.apply_db(p)==EntryCatalogUpdate::applied,"block add DB rejected");};
 auto block_send_self=[&](net::Packet p){block_responses.push_back(std::move(p));};
 net::Packet block_request{9,0x22,'T','a','r','g','e','t',0};block_request.resize(23,0);
 check(social_block_add(block_owner,1,block_request,block_lookup,block_send_db,block_send_self)==EntryCatalogUpdate::applied && block_owner.blocked().size()==1 && block_owner.blocked()[0].id==88 && block_db.blocked()[0].id==99 && block_db.blocked()[0].save_type==1 && block_responses.size()==2 && block_responses[0]==net::Packet({14,0x22,1}) && block_responses[1].size()==27 && block_responses[1][2]==99,"block add propagation differs");
 check(social_block_add(block_owner,1,block_request,block_lookup,block_send_db,block_send_self)==EntryCatalogUpdate::applied && lookups==1 && block_responses.back()==net::Packet({14,0x22,0}),"duplicate block should fail before lookup");
 SocialCatalog full_blocks(42,123);for(unsigned i=0;i<100;++i)full_blocks.add_block({i+1000,"occupied", ""});
 block_responses.clear();check(social_block_add(full_blocks,1,block_request,block_lookup,block_send_db,block_send_self)==EntryCatalogUpdate::applied && full_blocks.blocked().size()==100 && block_responses[0]==net::Packet({14,0x22,1}),"full list response differs");
 SocialCatalog wrong_country(42,123);block_responses.clear();check(social_block_add(wrong_country,2,block_request,block_lookup,block_send_db,block_send_self)==EntryCatalogUpdate::applied && wrong_country.blocked().empty() && block_responses==std::vector<net::Packet>{{14,0x22,0}},"cross-country block accepted");
 SocialCatalog self_block(42,123);block_responses.clear();auto self_lookup=[](std::string_view)->std::optional<SocialBlockTarget>{return SocialBlockTarget{88,99,1,"Target",true};};
 check(social_block_add(self_block,1,block_request,self_lookup,block_send_db,block_send_self)==EntryCatalogUpdate::applied && self_block.blocked().empty() && block_responses==std::vector<net::Packet>{{14,0x22,0}},"self block accepted");
 SocialCatalog request_catalog(42,123),target_catalog(43,222);std::uint32_t owner_pending=0,owner_deadline=0,target_pending=0,target_deadline=0;
 SocialFriendUser requester{request_catalog,124,123,1,1,2,3,"Owner",owner_pending,owner_deadline},target{target_catalog,223,222,1,4,5,6,"Target",target_pending,target_deadline};
 std::vector<net::Packet> request_db;std::vector<std::pair<SocialFriendUser*,net::Packet>> request_events;
 SocialFriendRequestPorts request_ports{[&](std::string_view n){return n=="Target"?&target:nullptr;},[]{return std::uint32_t{0xfffffff0};},[&](net::Packet p){request_db.push_back(std::move(p));},[&](SocialFriendUser& u,net::Packet p){request_events.emplace_back(&u,std::move(p));}};
 net::Packet request{2,0x22,'T','a','r','g','e','t',0};request.resize(23,0);
 check(social_friend_request(requester,request,request_ports)==EntryCatalogUpdate::applied && target_pending==123 && target_deadline==29984 && request_events.size()==1 && request_events[0].first==&target && request_events[0].second.size()==23 && request_events[0].second[0]==2,"friend request pending differs");
 request_events.clear();check(social_friend_request(requester,request,request_ports)==EntryCatalogUpdate::applied && request_events[0].second==net::Packet({3,0x22,2}),"busy friend request accepted");
 target_pending=0;target_catalog.add_friend({124,1,2,3,"Owner",""});request_events.clear();
 check(social_friend_request(requester,request,request_ports)==EntryCatalogUpdate::applied && request_catalog.find(223) && request_db.size()==1 && request_db[0].size()==34 && request_db[0][6]==222 && request_events.size()==3 && request_events[0].second==net::Packet({3,0x22,1}) && request_events[1].second[0]==4 && request_events[2].first==&target && request_events[2].second==net::Packet({7,0x22,123,0,0,0,1}),"one-sided friend request differs");
 VerticalSocialReplica request_replica(42);check(request_replica.apply_db(request_db[0])==EntryCatalogUpdate::applied && request_replica.friends()[0].id==222,"friend request DB propagation differs");
 SocialCatalog accepted_owner(42,123),accepted_peer(43,222);std::uint32_t ap=222,ad=555,bp=0,bd=0;
 SocialFriendUser accepting{accepted_owner,124,123,1,7,8,9,"Owner",ap,ad},inviter{accepted_peer,223,222,1,4,5,6,"Target",bp,bd};
 std::vector<net::Packet> accept_db;std::vector<std::pair<SocialFriendUser*,net::Packet>> accept_events;bool cleared_before_zone=false;
 SocialFriendResponsePorts accept_ports{[](std::uint32_t id)->std::optional<std::uint32_t>{return id;},[&](std::uint32_t id){cleared_before_zone=ap==0 && ad==0;return id==222?&inviter:nullptr;},[&](net::Packet p){accept_db.push_back(std::move(p));},[&](SocialFriendUser& u,net::Packet p){accept_events.emplace_back(&u,std::move(p));}};
 check(social_friend_response(accepting,net::Packet{3,0x22,1},accept_ports)==EntryCatalogUpdate::applied && cleared_before_zone && accepted_owner.find(223) && accepted_peer.find(124) && accept_db.size()==2 && accept_db[0][6]==222 && accept_db[1][6]==123 && accept_db[1][10]==4 && accept_db[1][11]==5 && accept_db[1][12]==9 && accept_events.size()==3 && accept_events[0].first==&inviter && accept_events[0].second==net::Packet({3,0x22,1}) && accept_events[2].first==&accepting,"friend acceptance differs");
 VerticalSocialReplica accept_db_owner(42),accept_db_peer(43);check(accept_db_owner.apply_db(accept_db[0])==EntryCatalogUpdate::applied && accept_db_peer.apply_db(accept_db[1])==EntryCatalogUpdate::applied && accept_db_peer.friends()[0].family==4 && accept_db_peer.friends()[0].grow==5,"friend acceptance DB propagation differs");
 ap=222;ad=555;accept_events.clear();check(social_friend_response(accepting,net::Packet{3,0x22,0},accept_ports)==EntryCatalogUpdate::applied && ap==0 && ad==0 && accept_events.size()==1 && accept_events[0].second==net::Packet({3,0x22,0}),"friend rejection differs");
 ap=222;ad=100;check(!social_friend_timeout(accepting,99) && ap==222 && ad==100 && social_friend_timeout(accepting,100) && !ap && !ad,"friend timeout boundary differs");
 ap=222;ad=0;check(!social_friend_timeout(accepting,0xffffffff) && ap==222,"zero friend deadline expired");
 ap=0;ad=100;check(!social_friend_timeout(accepting,100) && ad==100,"empty invitation cleared deadline");
 ap=222;ad=10;check(social_friend_timeout(accepting,0xfffffff0),"friend timeout unsigned wrap behavior differs");
 ZoneLeaveRequestState shared_requests;SocialCatalog shared_catalog(51,52);auto shared_user=bind_social_friend_user(shared_catalog,shared_requests,53,52,1,2,3,4,"Shared");
 shared_requests.request_5478=77;shared_requests.deadline_583c=100;check(social_friend_timeout(shared_user,100) && shared_requests.request_5478==0 && shared_requests.deadline_583c==0,"social timeout did not use zone leave state");
 ZoneLeaveRequestState dispatch_a_state,dispatch_b_state;SocialCatalog dispatch_a(71,101),dispatch_b(72,102);VerticalSocialReplica dispatch_a_db(71),dispatch_b_db(72);
 auto dispatch_user_a=bind_social_friend_user(dispatch_a,dispatch_a_state,101,101,1,1,2,3,"Alpha");auto dispatch_user_b=bind_social_friend_user(dispatch_b,dispatch_b_state,102,102,1,4,5,6,"Bravo");
 std::vector<net::Packet> dispatch_events;VerticalSocialClientPorts dispatch_ports;
 dispatch_ports.friend_by_name_in_zone=[&](std::string_view n){return n=="Bravo"?&dispatch_user_b:nullptr;};dispatch_ports.friend_world_id=[](std::uint32_t id)->std::optional<std::uint32_t>{return id;};dispatch_ports.friend_by_id_in_zone=[&](std::uint32_t id){return id==101?&dispatch_user_a:nullptr;};
 PlayerCore dispatch_core_a{},dispatch_core_b{};dispatch_core_a.user_id=71;dispatch_core_a.character.id=101;dispatch_core_b.user_id=72;dispatch_core_b.character.id=102;
 CharacterReplica dispatch_character_a(dispatch_core_a,pool),dispatch_character_b(dispatch_core_b,pool);VerticalSocialLifecycle dispatch_lifecycle_a(dispatch_a_db,dispatch_character_a,db,mutex,"test."),dispatch_lifecycle_b(dispatch_b_db,dispatch_character_b,db,mutex,"test.");
 std::function<VerticalSocialLifecycle<FakeDb>*(std::uint32_t)> find_dispatch_db=[&](std::uint32_t id){return id==71?&dispatch_lifecycle_a:id==72?&dispatch_lifecycle_b:nullptr;};
 check(vertical_social_db_route<FakeDb>(net::Packet{2,12,73,0,0,0,1,0,0,0,1,2,3,'X',0},find_dispatch_db)==EntryCatalogUpdate::rejected,"missing DB owner accepted");
 dispatch_ports.tick=[]{return 100U;};dispatch_ports.send_db=[&](net::Packet p){check(vertical_social_db_route<FakeDb>(p,find_dispatch_db)==EntryCatalogUpdate::applied,"dispatcher DB packet rejected");};dispatch_ports.send_user=[&](SocialFriendUser&,net::Packet p){dispatch_events.push_back(std::move(p));};dispatch_ports.catalog_by_id=[&](std::uint32_t id)->const SocialCatalog*{return id==102?&dispatch_b:nullptr;};dispatch_ports.send_peer=[&](std::uint32_t,net::Packet p){dispatch_events.push_back(std::move(p));};
 net::Packet dispatch_request{2,0x22,'B','r','a','v','o',0};dispatch_request.resize(23,0);
 check(vertical_social_client(dispatch_user_a,dispatch_request,dispatch_ports)==EntryCatalogUpdate::applied && dispatch_b_state.request_5478==101 && dispatch_b_state.deadline_583c==30100,"dispatch request did not reach shared pending state");
 check(vertical_social_client(dispatch_user_b,net::Packet{3,0x22,1},dispatch_ports)==EntryCatalogUpdate::applied && dispatch_a.find(102) && dispatch_b.find(101) && dispatch_a_db.friends()[0].id==102 && dispatch_b_db.friends()[0].id==101 && !dispatch_b_state.request_5478,"dispatch acceptance did not reach catalogs and DB replicas");
 check(vertical_social_client(dispatch_user_a,net::Packet{5,0x22,102,0,0,0},dispatch_ports)==EntryCatalogUpdate::applied && dispatch_a.friends().empty() && dispatch_a_db.friends()[0].id==0,"dispatch removal did not reach retained replica");
 dispatch_ports.block_by_name=[](std::string_view n)->std::optional<SocialBlockTarget>{if(n=="Block")return SocialBlockTarget{103,103,1,"Block",false};return std::nullopt;};net::Packet dispatch_block{9,0x22,'B','l','o','c','k',0};dispatch_block.resize(23,0);
 check(vertical_social_client(dispatch_user_a,dispatch_block,dispatch_ports)==EntryCatalogUpdate::applied && dispatch_a.blocked().size()==1 && dispatch_a_db.blocked()[0].id==103,"dispatch block did not reach retained replica");
 bool missing_transport=false;auto incomplete=dispatch_ports;incomplete.send_db={};try{vertical_social_client(dispatch_user_a,net::Packet{10,0x22,103,0,0,0},incomplete);}catch(const std::invalid_argument&){missing_transport=true;}check(missing_transport && dispatch_a.blocked().size()==1,"dispatcher accepted missing DB transport");
 check(vertical_social_client(dispatch_user_a,net::Packet{0xff,0x22},dispatch_ports)==EntryCatalogUpdate::unsupported && vertical_social_client(dispatch_user_a,net::Packet{2},dispatch_ports)==EntryCatalogUpdate::rejected,"dispatch opcode handling differs");
 SocialCatalog release_owner(90,500),release_peer(91,501);release_owner.add_friend({501,0,0,0,"Friend",""});release_owner.add_friend({502,0,0,0,"Absent",""});
 WorldLifecyclePorts release_ports;std::vector<WorldLifecycleStep> forwarded_steps;std::vector<std::pair<std::uint32_t,net::Packet>> release_packets;
 release_ports.step=[&](WorldLifecycleStep step){forwarded_steps.push_back(step);};release_ports=bind_social_world_release(std::move(release_ports),release_owner,[&](std::uint32_t id)->const SocialCatalog*{return id==501?&release_peer:nullptr;},[&](std::uint32_t id,net::Packet p){release_packets.emplace_back(id,std::move(p));});
 release_ports.admin_enter=[]{return true;};release_ports.close=[](std::uint32_t){};release_ports.local_calendar=[]{return std::chrono::sys_seconds{};};release_ports.game_log=[](net::Packet){};release_ports.manager=[](net::Packet){};
 release_ports.take_money=[](std::uint32_t)->std::optional<std::uint32_t>{return std::nullopt;};release_ports.save_money=[](std::uint32_t,std::uint32_t){};WorldLifecycleState release_state;leave_world(release_state,release_ports);
 check(forwarded_steps==std::vector<WorldLifecycleStep>{WorldLifecycleStep::release_guild,WorldLifecycleStep::leave_dead} && release_packets.size()==1 && release_packets[0].first==501 && release_packets[0].second==net::Packet({7,0x22,0xf4,1,0,0,0}) && release_owner.friends().size()==2,"world friend release differs");
 SocialCatalog notice_owner(90,0x12345678),notice_peer(91,5);for(auto id:{0U,5U,8U,5U,9U,0U})notice_owner.add_friend({id,0,0,0,"N",""});
 std::vector<std::uint32_t> notice_lookups;std::vector<std::pair<std::uint32_t,net::Packet>> notices;
 auto notice_lookup=[&](std::uint32_t id)->const SocialCatalog*{notice_lookups.push_back(id);return id==5 || id==9?&notice_peer:nullptr;};auto notice_send=[&](std::uint32_t id,net::Packet p){notices.emplace_back(id,std::move(p));};
 for(auto online:{true,false}) {notice_lookups.clear();notices.clear();if(online)notice_owner.notify_online(notice_lookup,notice_send);else notice_owner.notify_offline(notice_lookup,notice_send);
  const net::Packet expected{7,0x22,0x78,0x56,0x34,0x12,static_cast<std::uint8_t>(online)};check(notice_lookups==std::vector<std::uint32_t>{5,8,5,9} && notices==std::vector<std::pair<std::uint32_t,net::Packet>>{{5,expected},{5,expected},{9,expected}} && notice_owner.friends().size()==6,"social presence ordering differs");}
 CharacterEntryState identity_entry{};identity_entry.character_id=0x87654321;identity_entry.runtime_id=77;std::vector<CharacterEntryStep> identity_steps;
 CharacterEntryPorts identity_ports;identity_ports.step=[&](CharacterEntryStep step){if(step==CharacterEntryStep::wait_user || step==CharacterEntryStep::init_friends){check(identity_entry.runtime_id==identity_entry.character_id,"entry published mismatched identity");identity_steps.push_back(step);}};
 identity_ports.send_client=[](net::Packet){};identity_ports.send_manager=[&](net::Packet){check(identity_entry.runtime_id==identity_entry.character_id,"entry manager send preceded identity copy");};identity_ports.world_day=[]{return 0U;};identity_ports.system_time=[]{return 0U;};identity_ports.lock_guild=[]{return false;};identity_ports.unlock_guild=[]{};
 complete_character_selection(identity_entry,identity_ports);check(identity_steps==std::vector<CharacterEntryStep>{CharacterEntryStep::wait_user,CharacterEntryStep::init_friends},"entry identity stages differ");
 SocialCatalog game_social(42,123);game_social.add_friend({55,1,2,3,"First",""});game_social.add_friend({56,4,5,6,"Second",""});game_social.add_block({57,"Block",""});
 VerticalSocialReplica db_side(42);db_side.friends()[0]={55,1,2,3,"First","",0};db_side.friends()[1]={56,4,5,6,"Second","",0};db_side.blocked()[0]={57,"Block","",0};
 std::vector<net::Packet> responses;std::vector<std::uint32_t> peers;auto send_db=[&](net::Packet p){check(db_side.apply_db(p)==EntryCatalogUpdate::applied,"game social update rejected by DB replica");};
 auto send_self=[&](net::Packet p){responses.push_back(std::move(p));};auto lookup=[&](std::uint32_t)->const SocialCatalog*{return &game_social;};auto peer=[&](std::uint32_t id,net::Packet){peers.push_back(id);};
 check(game_social.apply_client({net::Packet{6,0x22,55,0,0,0,3,'x','\'','y',0}},send_db,send_self,lookup,peer)==EntryCatalogUpdate::applied && game_social.friends()[0].memo=="x'y" && db_side.friends()[0].memo=="x y", "client-to-DB memo path differs");
 check(game_social.apply_client({net::Packet{5,0x22,55,0,0,0}},send_db,send_self,lookup,peer)==EntryCatalogUpdate::applied && game_social.friends()[0].id==56 && game_social.friends()[0].family==1 && game_social.friends()[0].grow==2 && db_side.deleted_friends()==std::vector<std::uint32_t>{55} && peers==std::vector<std::uint32_t>{55},"friend removal propagation differs");
 check(game_social.apply_client({net::Packet{0x0a,0x22,57,0,0,0}},send_db,send_self,lookup,peer)==EntryCatalogUpdate::applied && game_social.blocked().empty() && db_side.deleted_blocks()==std::vector<std::uint32_t>{57},"block removal propagation differs");
 SocialCatalog boundaries(42,123);boundaries.add_friend({60,0,0,0,"N",""});boundaries.add_block({61,"B",""});std::vector<net::Packet> db_packets,client_packets;
 auto record_db=[&](net::Packet p){db_packets.push_back(std::move(p));};auto record_client=[&](net::Packet p){client_packets.push_back(std::move(p));};
 for(const auto [op,id]:{std::pair{6,60},std::pair{11,61}})for(unsigned length:{0U,3U,50U}) {
  net::Packet p{static_cast<std::uint8_t>(op),0x22,static_cast<std::uint8_t>(id),0,0,0,static_cast<std::uint8_t>(length)};p.insert(p.end(),length,'Z');p.push_back(0);
  check(boundaries.apply_client(p,record_db,record_client,lookup,peer)==EntryCatalogUpdate::applied && db_packets.back().size()==61 && client_packets.back().size()==length+8 && client_packets.back()[6]==length,"memo boundary packet differs");
 }
 net::Packet longest_friend{6,0x22,60,0,0,0,51};longest_friend.insert(longest_friend.end(),55,'Z');longest_friend.push_back(0);
 check(boundaries.apply_client(longest_friend,record_db,record_client,lookup,peer)==EntryCatalogUpdate::applied && boundaries.find(60)->memo==std::string(50,'Z') && client_packets.back().size()==58 && client_packets.back()[6]==50,"friend declared-51 truncation differs");
 for(const auto [op,id,declared]:{std::tuple{6,60,52},std::tuple{11,61,51}}) {
  auto previous_db=db_packets.size(),previous_client=client_packets.size();auto previous_friend=boundaries.find(60)->memo;auto previous_block=boundaries.blocked()[0].memo;
  net::Packet bad{static_cast<std::uint8_t>(op),0x22,static_cast<std::uint8_t>(id),0,0,0,static_cast<std::uint8_t>(declared),'X',0};
  check(boundaries.apply_client(bad,record_db,record_client,lookup,peer)==EntryCatalogUpdate::applied && db_packets.size()==previous_db && client_packets.size()==previous_client && boundaries.find(60)->memo==previous_friend && boundaries.blocked()[0].memo==previous_block,"oversized memo was not silently ignored");
 }
 for(unsigned count:{0U,1U,24U,25U,26U,49U,50U,51U,99U,100U})for(bool sparse:{false,true}) {
  VerticalSocialReplica check_replica(42);for(unsigned i=0;i<count;++i){auto slot=sparse?(i*37)%100:i;check_replica.friends()[slot]={slot+1,7,8,3,"Name","Memo"};check_replica.blocked()[slot]={slot+101,"Block","Note"};}
  auto loaded=check_replica.game_catalog(99);check(loaded.friends().size()==count && loaded.blocked().size()==count,"original batch count differs");unsigned at=0;
  for(unsigned slot=0;slot<100;++slot)if(check_replica.friends()[slot].id){check(loaded.friends()[at].id==slot+1 && loaded.friends()[at].family==7 && loaded.friends()[at].grow==8 && loaded.friends()[at].job==3 && loaded.blocked()[at].id==slot+101,"original batch order differs");++at;}
 }
 std::cout<<"retained DB social slots and packet batches passed\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

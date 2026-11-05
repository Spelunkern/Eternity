#include "eternity/game/vertical_admission_handoff.hpp"
#include <asio/io_context.hpp>
#include <iostream>
#include <vector>
using namespace eternity;using namespace eternity::game;
static void check(bool v,const char* msg){if(!v)throw std::runtime_error(msg);}
int main(){try{
 asio::io_context io;VerticalAdmissionHandoff::Strand strand(asio::make_strand(io));WorldAdmissionUser user;RuntimeZone zone;
 std::vector<std::string> events;std::function<void(bool)> completion;net::WorldChannel channel;
 channel.pause_input=[&](auto cb){events.push_back("pause");completion=std::move(cb);};channel.set_admission_dispatch=[&](bool b){check(b,"dispatch disabled");events.push_back("dispatch");};channel.resume_input=[&]{events.push_back("resume");};channel.disconnect=[&]{events.push_back("disconnect");};
 std::shared_ptr<VerticalAdmissionHandoff> handoff;asio::post(strand,[&]{handoff=std::make_shared<VerticalAdmissionHandoff>(strand,channel,user,[&]{events.push_back("paused");},[&](std::exception_ptr){events.push_back("failure");});handoff->start();});io.poll();check(events==std::vector<std::string>{"pause"},"pause blocked or fired early");
 completion(true);check(events.size()==1,"socket callback entered world synchronously");io.restart();io.poll();check(events==std::vector<std::string>({"pause","paused"}),"pause completion not dispatched");
 asio::post(strand,[&]{bool rejected=false;try{handoff->zone_ready();}catch(const std::logic_error&){rejected=true;}check(rejected,"premature release accepted");user.location=3;user.zone=&zone;handoff->zone_ready();handoff->cancel();});io.restart();io.poll();check(events==std::vector<std::string>({"pause","paused","dispatch","resume"}),"release ordering differs");
 events.clear();asio::post(strand,[&]{handoff=std::make_shared<VerticalAdmissionHandoff>(strand,channel,user,[&]{events.push_back("unexpected");},[&](std::exception_ptr){events.push_back("failure");});handoff->start();handoff->cancel();});io.restart();io.poll();completion(true);io.restart();io.poll();check(events==std::vector<std::string>{"pause"},"cancelled handoff resumed");
 // Dispatch can synchronously close the owning session and release its handoff.
 events.clear();channel.set_admission_dispatch=[&](bool b){check(b,"dispatch disabled");events.push_back("dispatch");handoff->cancel();handoff.reset();};
 asio::post(strand,[&]{handoff=std::make_shared<VerticalAdmissionHandoff>(strand,channel,user,[&]{events.push_back("paused");},[&](std::exception_ptr){events.push_back("failure");});handoff->start();});io.restart();io.poll();completion(true);io.restart();io.poll();
 asio::post(strand,[&]{handoff->zone_ready();});io.restart();io.poll();check(!handoff && events==std::vector<std::string>({"pause","paused","dispatch"}),"cancel during dispatch resumed or lost handoff");
 channel.set_admission_dispatch=[&](bool b){check(b,"dispatch disabled");events.push_back("dispatch");};
 events.clear();asio::post(strand,[&]{handoff=std::make_shared<VerticalAdmissionHandoff>(strand,channel,user,[]{throw std::runtime_error("fixture");},[&](std::exception_ptr e){check(bool(e),"missing failure");events.push_back("failure");});handoff->start();});io.restart();io.poll();completion(true);io.restart();io.poll();check(events==std::vector<std::string>({"pause","failure","disconnect"}),"failure did not close");
 // A selection callback can close, cancel the handoff, and then throw while
 // unwinding. It must not report or disconnect a second time.
 events.clear();asio::post(strand,[&]{handoff=std::make_shared<VerticalAdmissionHandoff>(strand,channel,user,[&]{events.push_back("paused");handoff->cancel();throw std::runtime_error("cancelled selection");},[&](std::exception_ptr){events.push_back("failure");});handoff->start();});io.restart();io.poll();completion(true);io.restart();io.poll();check(events==std::vector<std::string>({"pause","paused"}),"cancelled selection repeated failure or disconnect");
 // The failure service can itself synchronously close the owning session.
 events.clear();asio::post(strand,[&]{handoff=std::make_shared<VerticalAdmissionHandoff>(strand,channel,user,[]{throw std::runtime_error("selection failure");},[&](std::exception_ptr error){check(bool(error),"missing reentrant failure");events.push_back("failure");handoff->cancel();});handoff->start();});io.restart();io.poll();completion(true);io.restart();io.poll();check(events==std::vector<std::string>({"pause","failure"}),"failure close repeated disconnect");
 std::cout<<"asynchronous admission handoff ordering and cancellation passed\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

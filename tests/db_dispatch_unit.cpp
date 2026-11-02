#include "eternity/game/db_dispatch.hpp"
#include <atomic>
#include <deque>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>
using namespace eternity::game;
namespace {
void require(bool value,const std::string& message) {if(!value) throw std::runtime_error(message);}
void workers() {
    for(bool sync:{false,true}) {
        DbJobDispatcher dispatcher(sync,4);std::mutex mutex;std::vector<unsigned int> seen(4096,0);std::vector<std::shared_ptr<DbJobClient>> clients;
        for(unsigned int c=0;c<4;++c) clients.push_back(dispatcher.client([&,c](std::uint32_t job){require(job>=1 && job<=1024,"Bad threaded job status");std::lock_guard lock(mutex);++seen[c*1024+job-1];}));
        std::vector<std::thread> producers;std::atomic_bool rejected=false;
        for(unsigned int p=0;p<8;++p) producers.emplace_back([&,p](){for(unsigned int j=p%2;j<1024;j+=2) if(!clients[p/2]->post(j+1)) rejected=true;});
        for(auto& producer:producers) producer.join();dispatcher.wait_idle();require(!rejected,"Live dispatcher rejected a job");require(!dispatcher.failure(),"Threaded job callback failed");std::size_t completed=0,queued=0;for(auto n:seen) {require(n<=1,"Native completion workers duplicated a job");if(!sync) require(n==1,"Uncounted native workers lost a job");completed+=n;}for(const auto& client:clients) {queued+=client->queued_count();if(!sync) require(!client->pending_count(),"Uncounted mode changed its counter");}require(completed+queued==seen.size(),"Native worker concurrency lost a status");for(const auto& client:clients) client->reset();dispatcher.shutdown();dispatcher.shutdown();require(!clients[0]->post(8),"Shutdown accepted a new DB job");
    }
    // One worker makes completion order deterministic while preserving zero's
    // source behavior: it consumes a node but never decrements the job count.
    std::shared_ptr<DbJobClient> detached;
    {DbJobDispatcher dispatcher(true,1);std::vector<std::uint32_t> seen;auto client=dispatcher.client([&](std::uint32_t job){seen.push_back(job);});require(client->post(0),"Zero status was rejected");dispatcher.wait_idle();require(client->pending_count()==1 && !client->queued_count() && seen.empty(),"Zero status lost the original stuck counter");require(client->post(8),"Counted status was rejected");dispatcher.wait_idle();require(client->pending_count()==2 && client->queued_count()==1 && seen.empty(),"A post after zero unexpectedly woke the counted client");client->reset();require(!client->pending_count() && !client->queued_count(),"Client reset retained statuses or counter");for(auto job:{8U,9U,10U,22U}) require(client->post(job),"Ordered status rejected");dispatcher.wait_idle();require(seen==std::vector<std::uint32_t>{8,9,10,22},"Single-worker DB FIFO changed");detached=client;}
    require(!detached->post(9),"Detached client posted through a destroyed dispatcher");
    // Native boundary reports a violated no-throw handler contract without
    // silently terminating the worker or the entire process.
    {DbJobDispatcher dispatcher(false,1);auto client=dispatcher.client([](std::uint32_t){throw std::runtime_error("Handler fault fixture");});require(client->post(8),"Fault fixture rejected");dispatcher.wait_idle();require(bool(dispatcher.failure()),"Worker exception was silently lost");}
}
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected original DB dispatch oracle");std::ifstream input(argv[1]);require(input.good(),"Cannot open DB dispatch oracle");std::string line;std::deque<std::uint32_t> queue;std::vector<std::string> events;std::size_t cases=0,steps=0,next=0,races=0;std::uint32_t count=0;bool fail=false,injected=false;unsigned int inject=0;DbDispatchPort port;
        port.push=[&](std::uint32_t job){events.push_back("PUSH "+std::to_string(job));if(!fail) queue.push_back(job);};port.pop=[&](){const auto job=queue.empty()?0:queue.front();if(!queue.empty()) queue.pop_front();events.push_back("POP "+std::to_string(job));return job;};port.count=[&](){return count;};port.increment=[&](){++count;events.push_back("INC "+std::to_string(count));return count;};port.subtract=[&](std::uint32_t done){const auto old=count;count-=done;events.push_back("SUB "+std::to_string(done)+' '+std::to_string(old));return old;};port.wake=[&](){events.push_back("WAKE");};port.dispatch=[&](std::uint32_t job){events.push_back("JOB "+std::to_string(job));if(injected || !inject) return;injected=true;events.push_back("INJECT "+std::to_string(inject));switch(inject){case 1:count=0;break;case 2:count=0xffffffff;break;case 3:queue.push_back(8);++count;break;case 4:queue.clear();count=0;break;case 5:queue.push_back(0);++count;break;default:throw std::runtime_error("Bad injection fixture");}};
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;std::istringstream row(line);std::string tag;row>>tag;
            if(tag=="CASE") {std::size_t n=0;row>>port.attached>>port.synchronous>>count>>fail>>inject>>n;queue.clear();for(std::size_t i=0;i<n;++i){std::uint32_t job=0;row>>job;queue.push_back(job);}++cases;}
            else if(tag=="RACE") {queue={8};count=1;port.attached=port.synchronous=true;fail=false;inject=0;events.clear();next=0;const auto push=port.push,dispatch=port.dispatch;port.push=[&](std::uint32_t job){push(job);dispatch_db_jobs(port);};port.dispatch=[&](std::uint32_t job){dispatch(job);if(job==8) dispatch_db_jobs(port);};post_db_job(9,port);dispatch_db_jobs(port);port.push=push;port.dispatch=dispatch;++races;}
            else if(tag=="STEP") {std::string kind;std::uint32_t job=0;row>>kind>>job;events.clear();next=0;injected=false;if(kind=="P") post_db_job(job,port);else if(kind=="D") dispatch_db_jobs(port);else if(kind=="R") {queue.clear();count=0;events={"CLEAR","EXCHANGE 0"};}else throw std::runtime_error("Unknown dispatch step");++steps;}
            else if(tag=="EVENT") {require(next<events.size() && events[next]==line.substr(6),"DB dispatch source effects differ at case "+std::to_string(cases)+" step "+std::to_string(steps)+": "+(next<events.size()?events[next]:"<none>")+" vs "+line.substr(6));++next;}
            else if(tag=="STATE") {std::uint32_t expected=0;std::size_t n=0;row>>expected>>n;require(count==expected && queue.size()==n && next==events.size(),"DB dispatch source state/effect count differs");for(auto job:queue){std::uint32_t value=0;row>>value;require(job==value,"Original dispatch FIFO differs");}}
            else if(tag!="END") throw std::runtime_error("Unknown dispatch oracle row");
        }
        require(cases==2304 && steps==20736 && races==1,"DB dispatch oracle coverage changed");DbDispatchPort detached;detached.attached=false;post_db_job(8,detached);dispatch_db_jobs(detached);workers();
        std::cout<<cases<<" original dispatcher sequences / "<<steps<<" operations, 1 original interleaved counter race and native completion-worker concurrency passed\n";
    }catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}

#include "eternity/game/pathfinding.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <stdexcept>

namespace eternity::game {
namespace {
std::int32_t add(std::int32_t a,std::int32_t b) {return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(a)+static_cast<std::uint32_t>(b));}
std::int32_t sub(std::int32_t a,std::int32_t b) {return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(a)-static_cast<std::uint32_t>(b));}
std::int32_t margin(std::int32_t a,std::int32_t b) {const auto delta=sub(a,b);const auto magnitude=delta<0?sub(0,delta):delta;return std::min(magnitude/2,20);}
float cost(MapTile a,MapTile b) {const auto x=static_cast<float>(sub(a.x,b.x)),z=static_cast<float>(sub(a.z,b.z));return static_cast<float>(static_cast<double>(x)*x+static_cast<double>(z)*z);}
bool equal(MapTile a,MapTile b) {return a.x==b.x && a.z==b.z;}
}
struct AStarPath::Impl {
    struct Node {
        MapTile position;std::int32_t key=0;float g=0,h=0,total=0;
        Node *parent=nullptr,*child=nullptr,*tree_parent=nullptr,*left=nullptr,*right=nullptr;
    };
    std::vector<std::unique_ptr<Node>> storage;std::vector<Node*> pool;
    Node *open=nullptr,*closed=nullptr,*start=nullptr,*goal=nullptr,*cursor=nullptr;
    PathScanBounds scan;bool found=false;
    Node* allocate(MapTile point) {
        Node* node=nullptr;if(pool.empty()) {storage.push_back(std::make_unique<Node>());node=storage.back().get();}else {node=pool.back();pool.pop_back();}
        node->position=point;node->key=std::bit_cast<std::int32_t>((static_cast<std::uint32_t>(point.x)<<16)+static_cast<std::uint32_t>(point.z));return node;
    }
    void release(Node* node) {pool.push_back(node);}
    static void push(Node*& root,Node* node,bool priority) {
        if(!root) {root=node;node->tree_parent=nullptr;return;}
        auto* current=root;
        while(true) {
            auto*& next=(priority?node->total<current->total:node->key<current->key)?current->left:current->right;
            if(!next) {next=node;node->tree_parent=current;return;}current=next;
        }
    }
    static Node* pop(Node*& root) {
        auto* node=root;if(!node) return nullptr;
        while(node->left) node=node->left;
        if(!node->tree_parent) root=node->right;else node->tree_parent->left=node->right;
        if(node->right) node->right->tree_parent=node->tree_parent;
        return node;
    }
    static Node* find(Node* root,std::int32_t key,bool priority) {
        if(!priority) {while(root && root->key!=key) root=key<root->key?root->left:root->right;return root;}
        std::vector<Node*> pending;if(root) pending.push_back(root);
        while(!pending.empty()) {auto* node=pending.back();pending.pop_back();if(node->key==key) return node;if(node->right) pending.push_back(node->right);if(node->left) pending.push_back(node->left);}return nullptr;
    }
    static void remove(Node*& root,Node* node,bool priority) {
        if(!node->tree_parent) root=nullptr;else if(node->tree_parent->left==node) node->tree_parent->left=nullptr;else node->tree_parent->right=nullptr;
        if(node->left) push(root,node->left,priority);
        if(node->right) push(root,node->right,priority);
    }
    void clear_tree(Node* root,bool preserve_path) {
        // Match original postorder recycling, including the order visible to its LIFO heap.
        std::vector<std::pair<Node*,bool>> pending;if(root) pending.emplace_back(root,false);
        while(!pending.empty()) {const auto [node,visited]=pending.back();pending.pop_back();if(visited) {if(!preserve_path || !node->child) release(node);}else {pending.emplace_back(node,true);if(node->right) pending.emplace_back(node->right,false);if(node->left) pending.emplace_back(node->left,false);}}
    }
    void reset_path() {auto* node=start;while(node) {auto* next=node->child;release(node);node=next;}start=goal=cursor=nullptr;}
    void compress() {
        if(!start || !start->child || !start->child->child) return;
        auto* node=start->child;auto* next=node->child;auto dx=sub(start->position.x,node->position.x),dz=sub(start->position.z,node->position.z);
        while(next) {
            const auto nx=sub(node->position.x,next->position.x),nz=sub(node->position.z,next->position.z);
            if(nx==dx && nz==dz) {node->parent->child=next;next->parent=node->parent;release(node);}
            dx=nx;dz=nz;node=next;next=next->child;
        }
    }
};
AStarPath::AStarPath():impl_(std::make_unique<Impl>()) {}
AStarPath::~AStarPath()=default;
AStarPath::AStarPath(AStarPath&&) noexcept=default;
AStarPath& AStarPath::operator=(AStarPath&&) noexcept=default;
void AStarPath::reset_path() {impl_->reset_path();}
void AStarPath::reset() {reset_path();impl_->found=false;}
void AStarPath::set_path(MapTile point) {auto& state=*impl_;auto* node=state.allocate(point);node->child=nullptr;state.start=state.goal=state.cursor=node;state.found=true;}
bool AStarPath::found() const noexcept {return impl_->found;}
bool AStarPath::last() const noexcept {return !impl_->cursor || !impl_->cursor->child;}
std::optional<MapTile> AStarPath::current() const noexcept {return impl_->cursor?std::optional<MapTile>(impl_->cursor->position):std::nullopt;}
void AStarPath::advance() {if(impl_->cursor) {impl_->cursor=impl_->cursor->child;if(!impl_->cursor) reset();}}
PathScanBounds AStarPath::bounds() const noexcept {return impl_->scan;}
std::vector<MapTile> AStarPath::nodes() const {std::vector<MapTile> result;for(auto* node=impl_->start;node;node=node->child) {if(result.size()>impl_->storage.size()) throw std::logic_error("Cyclic original A* parent chain");result.push_back(node->position);}return result;}
bool AStarPath::find(const MapDefinition& map,MapTile from,MapTile to) {
    auto& state=*impl_;if(equal(from,to)) return true;
    const auto mx=margin(to.x,from.x),mz=margin(to.z,from.z);
    state.scan={std::max(sub(std::min(from.x,to.x),mx),0),std::max(sub(std::min(from.z,to.z),mz),0),std::min(add(std::max(from.x,to.x),mx),sub(map.size,1)),std::min(add(std::max(from.z,to.z),mz),sub(map.size,1))};
    state.reset_path();state.start=state.allocate(from);state.goal=state.allocate(to);
    for(auto* node:{state.start,state.goal}) {node->left=node->right=nullptr;node->parent=node->child=nullptr;}
    state.start->tree_parent=nullptr;state.start->g=0;state.start->h=cost(from,to);state.start->total=state.start->h;Impl::push(state.open,state.start,true);
    constexpr std::array<MapTile,8> offsets{{{-1,-1},{0,-1},{1,-1},{1,0},{1,1},{0,1},{-1,1},{-1,0}}};
    while(auto* current=Impl::pop(state.open)) {
        if(equal(current->position,to)) {
            state.goal->parent=current->parent;auto* node=state.goal;
            do {node->parent->child=node;node=node->parent;}while(node!=state.start);
            state.release(current);state.clear_tree(state.open,true);state.clear_tree(state.closed,true);state.open=state.closed=nullptr;
            state.compress();state.cursor=state.start->child;state.found=true;return true;
        }
        const auto at=current->position;
        if(at.x>=state.scan.minimum_x && at.x<=state.scan.maximum_x && at.z>=state.scan.minimum_z && at.z<=state.scan.maximum_z) {
            // Original GetSuccessorList allocates ALL candidates before inspecting any.
            std::vector<Impl::Node*> successors;
            for(auto delta:offsets) {const MapTile point{add(at.x,delta.x),add(at.z,delta.z)};if(!map.blocked(point)) successors.push_back(state.allocate(point));}
            for(auto* candidate:successors) {
                const auto g=static_cast<float>(static_cast<double>(cost(at,candidate->position))+current->g);
                if(auto* existing=Impl::find(state.open,candidate->key,true)) {if(existing->g<=g) {state.release(candidate);continue;}Impl::remove(state.open,existing,true);state.release(existing);}
                if(auto* existing=Impl::find(state.closed,candidate->key,false)) {if(existing->g<=g) {state.release(candidate);continue;}Impl::remove(state.closed,existing,false);state.release(existing);}
                candidate->g=g;candidate->h=cost(candidate->position,to);candidate->total=static_cast<float>(static_cast<double>(candidate->h)+g);
                candidate->parent=current;candidate->child=nullptr;candidate->tree_parent=candidate->left=candidate->right=nullptr;Impl::push(state.open,candidate,true);
            }
        }
        current->tree_parent=current->left=current->right=nullptr;Impl::push(state.closed,current,false);
    }
    state.release(state.goal);state.clear_tree(state.open,false);state.clear_tree(state.closed,false);state.open=state.closed=state.start=state.goal=state.cursor=nullptr;state.found=false;return false;
}
void ChaseTrail::add(const MapDefinition& map,MapTile point) {
    // ChasePathList::AddPath 0048f660 checks the node AFTER the head.
    if(nodes_.size()>1 && map.enable_line(point,nodes_[nodes_.size()-2])) {nodes_.back()=point;return;}
    if(!nodes_.empty() && equal(nodes_.back(),point)) return;
    nodes_.push_back(point);
}
std::optional<MapTile> ChaseTrail::pop() {const auto point=current();if(point) nodes_.pop_back();return point;}
}

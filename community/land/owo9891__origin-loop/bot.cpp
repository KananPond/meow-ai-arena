// Independent territory-game bot, designed from engine/game.py (2026-10-23).
// Single-file C++20. No file/network access, threads, or cross-game persistence.
#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <numeric>
#include <string>
#include <unordered_set>
#include <vector>

namespace origin {
constexpr int CELLS=48*48, FAR=10000;
constexpr int dx[4]={0,0,-1,1}, dy[4]={-1,1,0,0};
constexpr char directions[]="UDLR";
using Rows=std::array<uint64_t,48>;
using Route=std::vector<int>;
struct Player { int pos=0,dir=0,spawn=0,initialDir=0,deaths=0,kills=0; bool out=false; };
struct Board {
    int w=40,h=40,n=2,turn=0,turns=600,me=0,moveMs=50;
    std::array<int,CELLS> owner{},trail{},protect{};
    std::array<Player,4> p{};
    Board(){owner.fill(-1);trail.fill(-1);protect.fill(-1);}
    int size()const{return w*h;}
    int next(int k,int d)const {
        int x=k%w+dx[d],y=k/w+dy[d];
        return x<0||y<0||x>=w||y>=h?-1:y*w+x;
    }
    int distance(int a,int b)const{return std::abs(a%w-b%w)+std::abs(a/w-b/w);}
    std::array<int,4> areas()const {
        std::array<int,4>a{};for(int k=0;k<size();++k)if(owner[k]>=0)++a[owner[k]];return a;
    }
    Rows walls(int e)const {
        Rows r{};for(int k=0;k<size();++k)if(owner[k]==e||trail[k]==e)r[k/w]|=uint64_t{1}<<(k%w);return r;
    }
    Rows enclosed(const Rows& wall)const {
        Rows reached{};const uint64_t full=(uint64_t{1}<<w)-1,edge=1|(uint64_t{1}<<(w-1));
        for(int y=0;y<h;++y)reached[y]=((y==0||y==h-1)?full:edge)&~wall[y];
        bool change=true;
        while(change){
            change=false;
            for(int sweep=0;sweep<2;++sweep)for(int j=0;j<h;++j){
                int y=sweep?h-1-j:j;uint64_t open=full&~wall[y],seed=reached[y];
                if(y)seed|=reached[y-1];
                if(y+1<h)seed|=reached[y+1];
                seed&=open;
                uint64_t left=seed,right=seed,mask=open;
                for(int s=1;s<64;s*=2){left|=(left<<s)&mask;mask&=mask<<s;}
                mask=open;for(int s=1;s<64;s*=2){right|=(right>>s)&mask;mask&=mask>>s;}
                uint64_t value=left|right;
                if(value!=reached[y]){change=true;reached[y]=value;}
            }
        }
        for(int y=0;y<h;++y)reached[y]=full&~wall[y]&~reached[y];
        return reached;
    }
    // Exactly the official event order. Eliminated players never move.
    void step(const std::array<int,4>& action){
        std::array<int,4> target{},heading{},cause{},by{};
        target.fill(-1);cause.fill(0);by.fill(-1);
        for(int e=0;e<n;++e){
            heading[e]=p[e].dir;if(p[e].out)continue;
            int d=action[e];if(d>=0&&d<4&&d!=(p[e].dir^1))heading[e]=d;
            target[e]=next(p[e].pos,heading[e]);if(target[e]<0)cause[e]=1;
        }
        for(int a=0;a<n;++a)for(int b=a+1;b<n;++b)
            if(target[a]>=0&&target[b]>=0&&(target[a]==target[b]||
               (target[a]==p[b].pos&&target[b]==p[a].pos))){
                if(!cause[a])cause[a]=2,by[a]=b;
                if(!cause[b])cause[b]=2,by[b]=a;
            }
        for(int e=0;e<n;++e)if(target[e]>=0){
            int victim=trail[target[e]];
            if(victim>=0&&!cause[victim])cause[victim]=victim==e?3:4,by[victim]=e;
        }
        for(int e=0;e<n;++e)if(cause[e]){
            for(int k=0;k<size();++k)if(trail[k]==e)trail[k]=-1;
            p[e].pos=p[e].spawn;p[e].dir=p[e].initialDir;++p[e].deaths;
            if(cause[e]==4)++p[by[e]].kills;
        }
        std::array<bool,4> has{},closing{};
        for(int k=0;k<size();++k)if(trail[k]>=0)has[trail[k]]=true;
        for(int e=0;e<n;++e)if(!p[e].out&&!cause[e]){
            int k=target[e];p[e].pos=k;p[e].dir=heading[e];
            if(owner[k]==e)closing[e]=has[e];else trail[k]=e;
        }
        std::array<Rows,4> inside{};
        for(int e=0;e<n;++e)if(closing[e])inside[e]=enclosed(walls(e));
        for(int k=0;k<size();++k){
            int winner=-1;
            if(trail[k]>=0&&closing[trail[k]])winner=trail[k];
            else{
                int count=0;
                for(int e=0;e<n;++e)if(closing[e]&&((inside[e][k/w]>>(k%w))&1)){winner=e;++count;}
                if(count!=1)winner=-1;
            }
            if(winner>=0&&(protect[k]<0||protect[k]==winner))owner[k]=winner;
        }
        for(int k=0;k<size();++k)if(trail[k]>=0&&closing[trail[k]])trail[k]=-1;
        ++turn;
    }
};

struct Evaluation {
    bool valid=false;
    double score=-1e30,gain=0,survival=0;
    int margin=-FAR,hit=0,victim=-1;
    Route route;
};

#ifndef ORIGIN_RISK
#define ORIGIN_RISK 1.0
#endif
#ifndef ORIGIN_SEARCH_MS
#define ORIGIN_SEARCH_MS 11.0
#endif

class Planner {
public:
    Board b;
    std::array<std::array<int,CELLS>,4> reach{};
    std::array<int,4> homeTime{},idle{},lastPos{{-1,-1,-1,-1}},lastDeaths{},area{};
    std::array<double,4> rivalWeight{},denied{};
    std::array<int,CELLS> home{},nearest{},stamp{};
    std::vector<int> oldTrail;
    Rows baseWall{};
    std::array<Rows,4> valueMask{};
    std::array<Rows,4> contestedMask{};
    Route committed;
    int expected=-1,expectedDeaths=-1,serial=0;
    uint64_t rng=0x4e45574f52494749ULL;
    std::chrono::steady_clock::time_point started;
    double budget=11;
    Evaluation best;
    std::vector<Evaluation> elite;
    std::unordered_set<uint64_t> evaluated;

    double elapsed()const{return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();}
    unsigned random(){rng^=rng<<7;rng^=rng>>9;rng^=rng<<8;return unsigned(rng>>20);}
    bool hasTrail(int e)const {for(int k=0;k<b.size();++k)if(b.trail[k]==e)return true;return false;}
    void enemyField(int e){
        auto& out=reach[e];out.fill(FAR);homeTime[e]=FAR;
        if(b.p[e].out)return;
        std::array<int,CELLS*2> dist{},queue{};dist.fill(FAR);
        int head=0,tail=0,cells=b.size(),start=b.p[e].pos,clear=!hasTrail(e);
        dist[start+clear*cells]=0;queue[tail++]=start+clear*cells;
        if(clear)homeTime[e]=0;
        while(head<tail){
            int state=queue[head++],u=state%cells,mode=state/cells,t=dist[state];
            out[u]=std::min(out[u],t);if(b.owner[u]==e)homeTime[e]=std::min(homeTime[e],t);
            for(int d=0;d<4;++d){
                if(t==0&&d==(b.p[e].dir^1))continue;
                int v=b.next(u,d);if(v<0||(!mode&&b.trail[v]==e))continue;
                int nextState=v+(mode||b.owner[v]==e)*cells;
                if(dist[nextState]!=FAR)continue;
                dist[nextState]=t+1;queue[tail++]=nextState;
            }
        }
    }
    bool collision(int d)const {
        int target=b.next(b.p[b.me].pos,d);if(target<0)return true;
        for(int e=0;e<b.n;++e)if(e!=b.me&&!b.p[e].out)
            for(int q=0;q<4;++q)if(q!=(b.p[e].dir^1)){
                int v=b.next(b.p[e].pos,q);if(v<0)continue;
                if(v==target||(v==b.p[b.me].pos&&target==b.p[e].pos))return true;
            }
        return false;
    }
    // Shortest legal return appended to an arbitrary non-self-intersecting prefix.
    Route close(const Route& prefix)const {
        std::array<bool,CELLS> used{};
        int u=b.p[b.me].pos,heading=b.p[b.me].dir;
        bool exposed=!oldTrail.empty();Route result;
        for(int d:prefix){
            if(d<0||d>=4||d==(heading^1))return {};
            int v=b.next(u,d);if(v<0||used[v]||b.trail[v]==b.me)return {};
            result.push_back(d);u=v;heading=d;
            if(b.owner[u]==b.me&&exposed)return result;
            if(b.owner[u]!=b.me)used[u]=true,exposed=true;
        }
        if(!exposed)return {};
        std::array<int,CELLS> parent{},direction{},queue{};parent.fill(-2);
        int start=u,head=0,tail=0;parent[start]=-1;queue[tail++]=start;int goal=-1;
        while(head<tail){
            u=queue[head++];
            if(u!=start&&b.owner[u]==b.me){goal=u;break;}
            std::array<int,4> dirs{0,1,2,3};
            std::sort(dirs.begin(),dirs.end(),[&](int a,int c){
                int va=b.next(u,a),vc=b.next(u,c);
                return (va<0?-FAR:nearest[va])>(vc<0?-FAR:nearest[vc]);
            });
            for(int d:dirs){
                if(u==start&&d==(heading^1))continue;
                int v=b.next(u,d);if(v<0||used[v]||b.trail[v]==b.me||parent[v]!=-2)continue;
                parent[v]=u;direction[v]=d;queue[tail++]=v;
            }
        }
        if(goal<0)return {};
        Route back;for(int k=goal;k!=start;k=parent[k])back.push_back(direction[k]);
        std::reverse(back.begin(),back.end());result.insert(result.end(),back.begin(),back.end());return result;
    }
    double potential(int e)const {
        if(!hasTrail(e))return 0;
        std::array<int,CELLS> parent{},queue{};parent.fill(-2);
        int start=b.p[e].pos,head=0,tail=0,goal=-1;parent[start]=-1;queue[tail++]=start;
        while(head<tail){
            int u=queue[head++];if(b.owner[u]==e){goal=u;break;}
            for(int d=0;d<4;++d){
                if(u==start&&d==(b.p[e].dir^1))continue;
                int v=b.next(u,d);if(v<0||b.trail[v]==e||parent[v]!=-2)continue;
                parent[v]=u;queue[tail++]=v;
            }
        }
        if(goal<0)return 0;
        Rows wall=b.walls(e);std::array<bool,CELLS> path{};
        for(int u=goal;u!=start;u=parent[u])path[u]=true,wall[u/b.w]|=uint64_t{1}<<(u%b.w);
        Rows inside=b.enclosed(wall);double value=0;
        for(int k=0;k<b.size();++k){
            if(b.owner[k]==e||(b.protect[k]>=0&&b.protect[k]!=e))continue;
            if(path[k]||b.trail[k]==e||((inside[k/b.w]>>(k%b.w))&1))
                value+=b.owner[k]==b.me?1+rivalWeight[e]:rivalWeight[e];
        }
        return value;
    }
    void prepare(){
        started=std::chrono::steady_clock::now();budget=std::min(double(ORIGIN_SEARCH_MS),b.moveMs*0.28);
        area=b.areas();oldTrail.clear();nearest.fill(FAR);elite.clear();evaluated.clear();best=Evaluation{};
        for(int k=0;k<b.size();++k)if(b.trail[k]==b.me)oldTrail.push_back(k);
        for(int e=0;e<b.n;++e)if(e!=b.me){
            auto& p=b.p[e];
            idle[e]=(lastPos[e]==p.pos&&lastDeaths[e]==p.deaths&&p.pos==p.spawn)?idle[e]+1:0;
            p.out=idle[e]>=3;lastPos[e]=p.pos;lastDeaths[e]=p.deaths;
            rivalWeight[e]=b.n==2?1.0:0.5+0.5*std::exp(-std::abs(area[e]-area[b.me])/250.0);
            enemyField(e);for(int k=0;k<b.size();++k)nearest[k]=std::min(nearest[k],reach[e][k]);
        }
        baseWall=b.walls(b.me);for(auto& r:valueMask)r.fill(0);
        for(auto& r:contestedMask)r.fill(0);
        for(int k=0;k<b.size();++k)if(b.owner[k]!=b.me&&(b.protect[k]<0||b.protect[k]==b.me)){
            int category=b.owner[k]<0?b.me:b.owner[k];
            valueMask[category][k/b.w]|=uint64_t{1}<<(k%b.w);
            if(nearest[k]<24)contestedMask[category][k/b.w]|=uint64_t{1}<<(k%b.w);
        }
        home.fill(FAR);std::array<int,CELLS>queue{};int head=0,tail=0;
        for(int k=0;k<b.size();++k)if(b.owner[k]==b.me)home[k]=0,queue[tail++]=k;
        while(head<tail){int u=queue[head++];for(int d=0;d<4;++d){int v=b.next(u,d);
            if(v<0||b.trail[v]==b.me||home[v]!=FAR)continue;
            home[v]=home[u]+1;queue[tail++]=v;}}
        denied.fill(0);for(int e=0;e<b.n;++e)if(e!=b.me&&!b.p[e].out)denied[e]=potential(e);
    }
    Evaluation evaluate(const Route& proposal){
        Evaluation result;if(proposal.empty())return result;
        int u=b.p[b.me].pos,heading=b.p[b.me].dir;
        int steps=0,hit=0,victim=-1;bool outside=!oldTrail.empty(),closed=false;
        ++serial;Rows wall=baseWall,added{};std::vector<int> exposed=oldTrail;uint64_t hash=0x84222325cbf29ce4ULL;
        for(int d:proposal){
            if(d<0||d>=4||d==(heading^1))return result;
            int v=b.next(u,d);if(v<0||b.trail[v]==b.me||stamp[v]==serial)return result;
            u=v;heading=d;++steps;
            if(steps>b.turns-b.turn||steps>180)return result;
            hash=(hash^uint64_t(d+1))*1099511628211ULL;
            if(b.trail[u]>=0&&b.trail[u]!=b.me){
                int enemy=b.trail[u];
                if(victim<0){victim=enemy;hit=steps;if(steps>homeTime[enemy])return result;}
                else if(victim!=enemy)return result;
            }
            if(b.owner[u]==b.me&&outside){closed=true;break;}
            if(b.owner[u]!=b.me){outside=true;stamp[u]=serial;exposed.push_back(u);
                wall[u/b.w]|=uint64_t{1}<<(u%b.w);added[u/b.w]|=uint64_t{1}<<(u%b.w);}
        }
        if(!closed||!evaluated.insert(hash).second)return result;
        int margin=FAR;
        for(int k:exposed)for(int e=0;e<b.n;++e)if(e!=b.me&&!b.p[e].out){
            int m=reach[e][k]-steps;
            if(e==victim){
                // A victim cannot cut its own old trail before its death.
                m=(b.trail[k]==e?FAR:reach[e][k]-hit);
                m=std::min(m,hit+b.distance(b.p[e].spawn,k)-steps);
            }
            margin=std::min(margin,m);
        }
        if(margin<-3)return result;
        double survival=margin>1?1.0:margin==1?0.98:margin==0?0.78:margin==-1?0.42:margin==-2?0.18:0.06;
        survival=std::pow(survival,ORIGIN_RISK);
        if(collision(proposal[0]))survival*=0.03;
        Rows inside=b.enclosed(wall);double gain=0;
        for(int y=0;y<b.h;++y){
            uint64_t claims=inside[y]|added[y];
            for(int k:oldTrail)if(k/b.w==y)claims|=uint64_t{1}<<(k%b.w);
            for(int e=0;e<b.n;++e){
                double value=e==b.me?1.0:1+rivalWeight[e];
                gain+=std::popcount(claims&valueMask[e][y])*value;
                gain+=std::popcount(claims&contestedMask[e][y])*value*(b.n==2?0.65:0.2);
            }
        }
        double attackValue=victim<0?0:0.85*denied[victim]+2*rivalWeight[victim];
        double rate=(gain+attackValue)*survival/(steps+14.0);
        // Lost time and territory are distinct: a death does not erase territory.
        rate-=(1-survival)*(0.6+double(oldTrail.size())/25.0);
        if(b.turn+steps>=b.turns-2)rate+=(gain+attackValue)*survival*0.01;
        result.valid=true;result.score=rate;result.gain=gain;result.margin=margin;result.survival=survival;
        result.hit=hit;result.victim=victim;result.route.assign(proposal.begin(),proposal.begin()+steps);return result;
    }
    void consider(const Route& route){
        if(elapsed()>budget)return;
        Evaluation e=evaluate(route);if(!e.valid)return;
        if(e.score>best.score)best=e;
        if(elite.size()<8)elite.push_back(std::move(e));
        else{auto it=std::min_element(elite.begin(),elite.end(),[](const auto&a,const auto&c){return a.score<c.score;});
            if(e.score>it->score)*it=std::move(e);}
    }
    Route segmented(const Route& prefix,const std::vector<std::pair<int,int>>& legs)const {
        Route result=prefix;for(auto [d,len]:legs)for(int j=0;j<len;++j)result.push_back(d);return result;
    }
    void directSearch(const Route& prefix,int heading,bool dense,double deadline){
        const std::vector<int> lengths=dense?std::vector<int>{1,2,3,4,5,6,7,8,10,12,15,18,22,27}:
                                                std::vector<int>{2,4,7,11,16,22};
        for(int d=0;d<4;++d)if(d!=(heading^1))for(int a:lengths){
            if(elapsed()>deadline)return;
            // An automatically extended leg is simply a real move sequence;
            // evaluation terminates at the first actual return.
            for(int sd=0;sd<4;++sd)if(sd/2!=d/2){
                consider(segmented(prefix,{{d,a},{sd,std::max(b.w,b.h)}}));
                for(int c:lengths){
                    consider(segmented(prefix,{{d,a},{sd,c},{d^1,std::max(b.w,b.h)}}));
                    consider(segmented(prefix,{{d,a},{sd,c},{d^1,a},{sd^1,std::max(b.w,b.h)}}));
                }
            }
        }
    }
    void attacks(){
        std::array<int,CELLS> parent{},direction{},dist{},queue{};parent.fill(-2);dist.fill(FAR);
        int start=b.p[b.me].pos,head=0,tail=0;parent[start]=-1;dist[start]=0;queue[tail++]=start;
        std::array<int,4> tried{};
        while(head<tail&&elapsed()<budget*0.85){
            int u=queue[head++],e=b.trail[u];
            if(u!=start&&e>=0&&e!=b.me&&tried[e]<6&&dist[u]<=homeTime[e]){
                ++tried[e];Route path;for(int k=u;k!=start;k=parent[k])path.push_back(direction[k]);
                std::reverse(path.begin(),path.end());consider(close(path));
            }
            if(dist[u]>=22)continue;
            for(int d=0;d<4;++d){if(u==start&&d==(b.p[b.me].dir^1))continue;
                int v=b.next(u,d);if(v<0||b.trail[v]==b.me||parent[v]!=-2)continue;
                parent[v]=u;direction[v]=d;dist[v]=dist[u]+1;queue[tail++]=v;}
        }
    }
    void frontier(){
        if(!oldTrail.empty())return;
        // Heading-aware navigation remains entirely inside current territory.
        std::array<int,CELLS*4> parent{},queue{},distance{};parent.fill(-2);
        int start=4*b.p[b.me].pos+b.p[b.me].dir,head=0,tail=0;parent[start]=-1;distance[start]=0;queue[tail++]=start;
        std::vector<std::pair<double,int>> candidates;
        while(head<tail){
            int state=queue[head++],u=state/4,hdg=state%4;
            bool edge=false;
            for(int d=0;d<4;++d)if(d!=(hdg^1)){
                int v=b.next(u,d);if(v<0||b.trail[v]==b.me)continue;
                if(b.owner[v]!=b.me){edge=true;continue;}
                int nextState=4*v+d;if(parent[nextState]!=-2)continue;
                parent[nextState]=state;distance[nextState]=distance[state]+1;queue[tail++]=nextState;
            }
            if(edge&&distance[state]>0&&distance[state]<=18)
                candidates.emplace_back(std::min(nearest[u],50)-1.8*distance[state],state);
        }
        std::sort(candidates.rbegin(),candidates.rend());std::vector<int> chosen;
        for(auto [score,state]:candidates){
            (void)score;int u=state/4;bool near=false;for(int k:chosen)if(b.distance(k,u)<5)near=true;
            if(near)continue;
            chosen.push_back(u);
            Route prefix;for(int s=state;s!=start;s=parent[s])prefix.push_back(s%4);
            std::reverse(prefix.begin(),prefix.end());
            directSearch(prefix,state%4,false,std::min(budget*0.68,elapsed()+budget*0.09));
            if(chosen.size()>=3||elapsed()>budget*0.68)break;
        }
    }
    // A small diverse beam explores irregular prefixes, then reconnects them
    // to real territory. Occupancy belongs to the route, not just its endpoint.
    void beam(){
        struct Node {
            int pos,dir,length; double value;
            std::array<uint64_t,36> used{};
            std::array<unsigned char,40> moves{};
        };
        Node start{};start.pos=b.p[b.me].pos;start.dir=b.p[b.me].dir;
        std::vector<Node> layer{start};
        double deadline=budget*0.84;
        for(int depth=1;depth<=32&&elapsed()<deadline;++depth){
            std::vector<Node> expanded;expanded.reserve(layer.size()*3);
            for(const Node& node:layer)for(int d=0;d<4;++d)if(d!=(node.dir^1)){
                int v=b.next(node.pos,d);
                if(v<0||b.trail[v]==b.me||((node.used[v/64]>>(v%64))&1))continue;
                Node child=node;child.pos=v;child.dir=d;child.length=depth;child.moves[depth-1]=static_cast<unsigned char>(d);
                bool wasOutside=!oldTrail.empty();
                for(uint64_t bits:node.used)wasOutside|=bits!=0;
                if(b.owner[v]==b.me&&wasOutside){
                    Route route;for(int j=0;j<depth;++j)route.push_back(child.moves[j]);consider(route);continue;
                }
                if(b.owner[v]!=b.me)child.used[v/64]|=uint64_t{1}<<(v%64);
                int returnDistance=home[v];if(returnDistance>=FAR||depth+returnDistance>b.turns-b.turn)continue;
                double cell=b.owner[v]==b.me?0:b.owner[v]<0?1:1+rivalWeight[b.owner[v]];
                child.value=node.value+cell+0.08*std::min(nearest[v],40)-0.14*returnDistance;
                if(nearest[v]<depth+returnDistance)child.value-=2;
                expanded.push_back(std::move(child));
            }
            if(expanded.empty())break;
            std::sort(expanded.begin(),expanded.end(),[](const Node&a,const Node&c){return a.value>c.value;});
            layer.clear();std::array<unsigned char,CELLS*4> diversity{};
            for(auto& node:expanded){
                int bucket=node.pos*4+node.dir;
                if(diversity[bucket]>=2)continue;
                ++diversity[bucket];layer.push_back(std::move(node));if(layer.size()==32)break;
            }
            if(depth%4==0)for(size_t i=0;i<std::min<size_t>(6,layer.size())&&elapsed()<deadline;++i){
                Route prefix;for(int j=0;j<depth;++j)prefix.push_back(layer[i].moves[j]);consider(close(prefix));
            }
        }
    }
    void refine(){
        for(int iteration=0;iteration<1800&&elapsed()<budget;++iteration){
            Route proposal;
            if(!elite.empty()&&random()%4!=0){
                const auto& source=elite[random()%elite.size()].route;
                std::vector<std::pair<int,int>> legs;
                for(int d:source){if(legs.empty()||legs.back().first!=d)legs.emplace_back(d,1);else ++legs.back().second;}
                int index=random()%legs.size();legs[index].second=std::max(1,legs[index].second+int(random()%7)-3);
                for(auto [d,len]:legs)for(int j=0;j<len;++j)proposal.push_back(d);
                // Half of mutations reconnect with a freshly computed shortest return.
                if(random()%2&&proposal.size()>2){proposal.resize(1+random()%(proposal.size()-1));proposal=close(proposal);}
            }else{
                int heading=b.p[b.me].dir;
                for(int segment=0;segment<3+int(random()%3);++segment){
                    int d=segment?(heading/2?0:2)+int(random()%2):int(random()%4);
                    if(d==(heading^1))d=heading;
                    heading=d;
                    int len=2+int(random()%20);for(int j=0;j<len;++j)proposal.push_back(d);
                }
                proposal=close(proposal);
            }
            consider(proposal);
        }
    }
    int terminal(){
        double bestWorst=-1e30,bestMean=-1e30;int choice=b.p[b.me].dir;
        std::array<int,4> moves{};std::array<std::vector<int>,4> options;int count=1;
        for(int e=0;e<b.n;++e)if(e!=b.me){
            if(b.p[e].out)options[e].push_back(-1);
            else for(int d=0;d<4;++d)if(d!=(b.p[e].dir^1))options[e].push_back(d);
            count*=int(options[e].size());
        }
        for(int d=0;d<4;++d)if(d!=(b.p[b.me].dir^1)){
            double worst=1e30,sum=0;moves[b.me]=d;
            for(int code=0;code<count;++code){int c=code;for(int e=0;e<b.n;++e)if(e!=b.me){moves[e]=options[e][c%options[e].size()];c/=int(options[e].size());}
                Board next=b;next.step(moves);auto a=next.areas();int ahead=0,leader=0;
                for(int e=0;e<b.n;++e)if(e!=b.me&&!b.p[e].out){ahead+=a[e]>a[b.me];leader=std::max(leader,a[e]);}
                double value=-100000.0*ahead+a[b.me]-leader-0.01*(next.p[b.me].deaths-b.p[b.me].deaths);
                worst=std::min(worst,value);sum+=value;
            }
            if(worst>bestWorst||(worst==bestWorst&&sum>bestMean)){bestWorst=worst;bestMean=sum;choice=d;}
        }
        return choice;
    }
    int fallback(){
        double score=-1e30;int choice=b.p[b.me].dir;
        for(int d=0;d<4;++d)if(d!=(b.p[b.me].dir^1)){
            int v=b.next(b.p[b.me].pos,d);if(v<0||b.trail[v]==b.me)continue;
            double s=(collision(d)?-10000:0)+(b.owner[v]==b.me?20:0)-std::min(home[v],100)*2+std::min(nearest[v],60)*0.2;
            if(b.trail[v]>=0&&b.trail[v]!=b.me)s+=25+denied[b.trail[v]]*0.1;
            // Mobility prevents walking into a narrow dead end inside territory.
            for(int q=0;q<4;++q)if(q!=(d^1)){int k=b.next(v,q);if(k>=0&&b.trail[k]!=b.me)s+=b.owner[k]==b.me?3:0.2;}
            if(s>score)score=s,choice=d;
        }
        return choice;
    }
    int decide(){
        prepare();if(b.turn+1>=b.turns)return terminal();
        if(expected==b.p[b.me].pos&&expectedDeaths==b.p[b.me].deaths&&!committed.empty())consider(committed);
        else committed.clear();
        if(!oldTrail.empty())consider(close({}));
        attacks();
        directSearch({},b.p[b.me].dir,true,budget*0.4);
        frontier();beam();refine();
        int choice=fallback();
        if(best.valid&&best.score>0){choice=best.route[0];committed=best.route;committed.erase(committed.begin());}
        else committed.clear();
        expected=b.next(b.p[b.me].pos,choice);expectedDeaths=b.p[b.me].deaths;return choice;
    }
};

bool readTurn(Board& b){
    std::string token,row;if(!(std::cin>>token))return false;if(token!="TURN")return false;std::cin>>b.turn;
    for(int e=0;e<b.n;++e){int id,x,y,length,area,deaths;char d;
        std::cin>>token>>id>>x>>y>>d>>length>>area>>deaths;
        b.p[id].pos=y*b.w+x;b.p[id].dir=int(std::string("UDLR").find(d));b.p[id].deaths=deaths;
        if(b.turn==0)b.p[id].initialDir=b.p[id].dir;
    }
    std::cin>>token;for(int y=0;y<b.h;++y){std::cin>>row;for(int x=0;x<b.w;++x)b.owner[y*b.w+x]=row[x]=='.'?-1:row[x]-'0';}
    std::cin>>token;for(int y=0;y<b.h;++y){std::cin>>row;for(int x=0;x<b.w;++x)b.trail[y*b.w+x]=row[x]=='.'?-1:row[x]-'0';}
    std::cin>>token;return bool(std::cin);
}
} // namespace origin

#ifndef ORIGIN_TEST
int main(){
    std::ios::sync_with_stdio(false);std::cin.tie(nullptr);
    static origin::Planner planner;auto& b=planner.b;std::string token;int initMs;
    if(!(std::cin>>token>>b.w>>b.h>>b.turns>>b.n>>b.me>>b.moveMs>>initMs))return 0;
    for(int e=0;e<b.n;++e){int id,x,y;std::cin>>token>>id>>x>>y;b.p[id].spawn=y*b.w+x;
        for(int yy=y-1;yy<=y+1;++yy)for(int xx=x-1;xx<=x+1;++xx)if(xx>=0&&xx<b.w&&yy>=0&&yy<b.h)b.protect[yy*b.w+xx]=id;
    }
    while(origin::readTurn(b)){int d=planner.decide();std::cout<<b.turn<<' '<<origin::directions[d]<<std::endl;}
}
#endif

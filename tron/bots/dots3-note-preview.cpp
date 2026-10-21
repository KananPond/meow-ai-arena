// Tron bot: 距离变换 + 死胡同检测 + 自适应搜索。
// 1v1 minimax；3~4 人 2 ply；12 人 1 ply + 死胡同检测。
#include "tron.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <queue>
#include <vector>
namespace {
constexpr int INF=1e9,MW=tron::MAXW,MH=tron::MAXH;
struct WD{int d[MH][MW];
    void compute(const tron::State&s){
        for(int y=0;y<s.H;++y)for(int x=0;x<s.W;++x)d[y][x]=(s.cell[y][x]!=tron::EMPTY)?0:INF;
        std::vector<std::pair<int,int>>q;
        for(int y=0;y<s.H;++y)for(int x=0;x<s.W;++x)if(!d[y][x])q.push_back({x,y});
        for(size_t i=0;i<q.size();++i){int x=q[i].first,y=q[i].second;
            for(int dr=0;dr<4;++dr){int nx=x+tron::DX[dr],ny=y+tron::DY[dr];
                if(nx>=0&&nx<s.W&&ny>=0&&ny<s.H&&d[ny][nx]>d[y][x]+1){d[ny][nx]=d[y][x]+1;q.push_back({nx,ny});}}}}
    int get(int x,int y)const{return d[y][x];}
};
inline double eval(const tron::State&s,int me,const WD&wd){
    double sc=0;
    int fn=0;for(int d=0;d<4;++d)if(tron::can_move(s,me,d))++fn;
    sc+=double(fn)*30.0;
    int w=wd.get(s.x[me],s.y[me]);sc+=double(w)*10.0;
    if(fn<=1)sc-=300.0;else if(fn<=2&&w>=3)sc-=100.0;
    for(int k=0;k<s.N;++k){if(k==me||!s.alive[k])continue;
        int dist=std::abs(s.x[me]-s.x[k])+std::abs(s.y[me]-s.y[k]);
        if(dist<=1)sc-=400.0;else if(dist==2)sc-=120.0;else if(dist==3)sc-=50.0;
        else if(dist==4)sc-=20.0;else if(dist<=6)sc-=6.0;}
    if(s.turn<s.max_turns/5){int ed=std::min({s.x[me],s.W-1-s.x[me],s.y[me],s.H-1-s.y[me]});sc+=double(ed)*3.0;}
    if(s.N<=4&&s.turn<s.max_turns/2){int cx=s.W/2,cy=s.H/2;sc+=double(s.max_turns-std::abs(s.x[me]-cx)-std::abs(s.y[me]-cy))*0.05;}
    if(tron::can_move(s,me,s.last_dir[me]))sc+=6.0;
    return sc;
}
double s1v1(tron::State&s,int depth,double alpha,double beta,int me,const WD&wd,int&n){
    if(s.over||depth<=0)return eval(s,me,wd);
    int opp=1-me;
    if(!s.alive[opp]){double best=-1e18;
        for(int d=0;d<4;++d){if(!tron::can_move(s,me,d))continue;
            tron::State ns=s;int mv[tron::MAXN]={0};mv[me]=d;tron::step(ns,mv);++n;
            double v=s1v1(ns,depth-1,alpha,beta,me,wd,n);if(v>best)best=v;
            if(best>alpha)alpha=best;if(alpha>=beta)break;}return best;}
    double best=-1e18;
    for(int d=0;d<4;++d){if(!tron::can_move(s,me,d))continue;
        double worst=1e18;bool any=false;
        for(int od=0;od<4;++od){if(!tron::can_move(s,opp,od))continue;any=true;
            tron::State ns=s;int mv[tron::MAXN]={0};mv[me]=d;mv[opp]=od;tron::step(ns,mv);++n;
            double v=s1v1(ns,depth-1,alpha,beta,me,wd,n);if(v<worst)worst=v;
            if(worst<=alpha){best=-1e18;break;}}
        if(!any){tron::State ns=s;int mv[tron::MAXN]={0};mv[me]=d;tron::step(ns,mv);++n;
            worst=s1v1(ns,depth-1,alpha,beta,me,wd,n);}
        if(worst>best)best=worst;if(best>alpha)alpha=best;if(alpha>=beta)break;}
    return best;
}
double sm(tron::State&s,int depth,double alpha,double beta,int me,const WD&wd,int&n){
    if(s.over||depth<=0)return eval(s,me,wd);
    double best=-1e18;
    for(int d=0;d<4;++d){if(!tron::can_move(s,me,d))continue;
        int mv[tron::MAXN]={0};mv[me]=d;bool ok=true;
        for(int k=0;k<s.N;++k){if(k==me||!s.alive[k])continue;
            int bo=-1;double bov=-1e18;
            for(int od=0;od<4;++od){if(!tron::can_move(s,k,od))continue;
                tron::State ts=s;int tm[tron::MAXN]={0};tm[me]=d;tm[k]=od;tron::step(ts,tm);
                double ov=eval(ts,k,wd);if(ov>bov){bov=ov;bo=od;}}
            if(bo<0){ok=false;break;}mv[k]=bo;}
        if(!ok)continue;tron::State ts=s;tron::step(ts,mv);++n;
        double v=sm(ts,depth-1,alpha,beta,me,wd,n);if(v>best)best=v;
        if(best>alpha)alpha=best;if(alpha>=beta)break;}
    return best;
}
int d12(const tron::State&s,const WD&wd){
    int me=s.me,best_d=-1;double best_sc=-1e18;
    for(int d=0;d<4;++d){if(!tron::can_move(s,me,d))continue;
        tron::State ns=s;int mv[tron::MAXN]={0};mv[me]=d;tron::step(ns,mv);
        int fn2=0;for(int dd=0;dd<4;++dd)if(tron::can_move(ns,me,dd))++fn2;
        double sc=eval(ns,me,wd);
        if(fn2<=1)sc-=500.0;
        if(sc>best_sc){best_sc=sc;best_d=d;}}
    if(best_d<0)for(int d=0;d<4;++d)if(tron::can_move(s,me,d)){best_d=d;break;}
    return best_d>=0?best_d:tron::U;
}
}//namespace
int decide(const tron::State&s){
    const int me=s.me;bool any=false;
    for(int d=0;d<4;++d)if(tron::can_move(s,me,d)){any=true;break;}
    if(!any)return tron::U;
    WD wd;wd.compute(s);
    if(s.N==2){int depth=6;if(s.turn==0)depth=8;
        int n=0,best_d=-1;double best_sc=-1e18;
        for(int d=0;d<4;++d){if(!tron::can_move(s,me,d))continue;
            int opp=1-me;
            if(s.alive[opp]){double worst=1e18;
                for(int od=0;od<4;++od){if(!tron::can_move(s,opp,od))continue;
                    tron::State ns=s;int mv[tron::MAXN]={0};mv[me]=d;mv[opp]=od;tron::step(ns,mv);++n;
                    double v=s1v1(ns,depth-1,-1e18,1e18,me,wd,n);if(v<worst)worst=v;}
                if(worst>best_sc){best_sc=worst;best_d=d;}}
            else{tron::State ns=s;int mv[tron::MAXN]={0};mv[me]=d;tron::step(ns,mv);++n;
                double v=s1v1(ns,depth-1,-1e18,1e18,me,wd,n);if(v>best_sc){best_sc=v;best_d=d;}}}
        return best_d>=0?best_d:tron::U;}
    if(s.N<=4){int depth=(s.turn==0)?3:2;
        int n=0,best_d=-1;double best_sc=-1e18;
        for(int d=0;d<4;++d){if(!tron::can_move(s,me,d))continue;
            int mv[tron::MAXN]={0};mv[me]=d;bool ok=true;
            for(int k=0;k<s.N;++k){if(k==me||!s.alive[k])continue;
                int bo=-1;double bov=-1e18;
                for(int od=0;od<4;++od){if(!tron::can_move(s,k,od))continue;
                    tron::State ts=s;int tm[tron::MAXN]={0};tm[me]=d;tm[k]=od;tron::step(ts,tm);
                    double ov=eval(ts,k,wd);if(ov>bov){bov=ov;bo=od;}}
                if(bo<0){ok=false;break;}mv[k]=bo;}
            if(!ok)continue;tron::State ts=s;tron::step(ts,mv);++n;
            double v=sm(ts,depth-1,-1e18,1e18,me,wd,n);if(v>best_sc){best_sc=v;best_d=d;}}
        if(best_d<0)for(int d=0;d<4;++d)if(tron::can_move(s,me,d)){best_d=d;break;}
        return best_d>=0?best_d:tron::U;}
    return d12(s,wd);
}
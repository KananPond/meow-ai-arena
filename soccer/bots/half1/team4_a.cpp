#include <bits/stdc++.h>
#include "soccer.hpp"
using namespace std;
using soccer::i64;
using soccer::Game;
using soccer::Action;
using soccer::BallPt;
using soccer::PlayerView;

struct V {
    double x=0,y=0;
    V operator+(V b)const{return{x+b.x,y+b.y};}
    V operator-(V b)const{return{x-b.x,y-b.y};}
    V operator*(double s)const{return{x*s,y*s};}
    V operator/(double s)const{return{x/s,y/s};}
    double norm()const{return hypot(x,y);}
};
V pos(const PlayerView&p){return{double(p.x),double(p.y)};}
V vel(const PlayerView&p){return{double(p.vx),double(p.vy)};}
V limited(V v,double r){double d=v.norm();return d>r?v*(r/d):v;}
double bound(double x,double lo,double hi){return max(lo,min(hi,x));}
double inertia[81],radius[81],control[81];
soccer::Config cfg;
struct Track {vector<BallPt>b;int goal=0;};
Track track(i64 x,i64 y,i64 vx,i64 vy,int horizon=40){
    Track z;z.b.push_back({x,y,vx,vy});vector<BallPt>tail;
    z.goal=soccer::predict_ball(cfg,x,y,vx,vy,horizon,tail);
    for(auto p:tail)z.b.push_back(p);
    if(z.goal)z.b.pop_back();
    else while(int(z.b.size())<=horizon)z.b.push_back(z.b.back());
    return z;
}
int eta(const PlayerView&p,const Track&tr,double reach=500,int minT=0){
    for(int n=max(minT,int(p.cd));n<int(tr.b.size());++n){
        V d={tr.b[n].x-p.x-p.vx*inertia[n],tr.b[n].y-p.y-p.vy*inertia[n]};
        if(d.norm()<=radius[n]+reach)return n;
    }return 99;
}
bool cankick(const PlayerView&p,i64 bx,i64 by){
    return p.cd==0&&(p.x-bx)*(p.x-bx)+(p.y-by)*(p.y-by)<=250000;
}
V move_to(const PlayerView&p,V target,V tv={0,0}){
    V d=target-pos(p);
    // Position control with braking; feed forward the desired target speed.
    V desired=limited(d*0.32,330)+tv;
    return limited((desired-vel(p))*0.52,40);
}
V intercept(const PlayerView&p,const Track&tr){
    int n=eta(p,tr,425,1);
    if(n==99)n=min(25,int(tr.b.size())-1);
    n=max(1,n);
    const auto &b=tr.b[min(n,int(tr.b.size())-1)];
    V d={b.x-p.x-p.vx*inertia[n],b.y-p.y-p.vy*inertia[n]};
    V a=limited(d/max(1.,control[n]),40);
    if(hypot(double(b.vx),double(b.vy))<100 && n<6){
        V target={double(b.x)-240,double(b.y)};
        if(p.x>b.x+200)target.x=b.x;
        a=move_to(p,target,{double(b.vx)*0.4,double(b.vy)*0.4});
    }
    return a;
}
V cover(const PlayerView&p,const Track&tr){
    const auto&b=tr.b[0];
    int e=eta(p,tr,450,1);
    bool incoming=tr.goal<0;
    for(int k=1;k<min(13,int(tr.b.size()));++k)
        if(tr.b[k].x<1500&&tr.b[k].y>2600&&tr.b[k].y<5400&&tr.b[k].vx<0)incoming=true;
    if(incoming&&e<99)return intercept(p,tr);
    double x=bound(b.x*0.18,650,1600);
    double y=4000+(b.y-4000)*x/max(1400.,double(b.x));
    y=bound(y,3050,4950);
    if(b.x<1800){x=450;y=bound(double(b.y),3100,4900);}
    return move_to(p,{x,y});
}

struct Agent {
    int idx=0,leader=-1;
    i64 lastMy=-1,lastOpp=-1;
    soccer::Obs o;
    PlayerView p[4]; // Canonical team indices, independent of this process's seat.
    Track tr;
    int times[4];
    void prepare(){
        p[idx]=o.pl[0];p[1-idx]=o.pl[1];p[2]=o.pl[2];p[3]=o.pl[3];
        tr=track(o.bx,o.by,o.bvx,o.bvy);
        for(int j=0;j<4;++j)times[j]=eta(p[j],tr);
        int winner=times[0]<=times[1]?0:1;
        if(leader<0||o.my_score!=lastMy||o.opp_score!=lastOpp)leader=winner;
        else if(times[1-leader]+2<=times[leader])leader=1-leader;
        lastMy=o.my_score;lastOpp=o.opp_score;
    }
    Game state(){
        Game g;g.cfg.stall=0;g.cfg.steps=10000;
        g.bx=o.bx;g.by=o.by;g.bvx=o.bvx;g.bvy=o.bvy;
        for(int i=0;i<4;++i){g.px[i]=p[i].x;g.py[i]=p[i].y;g.pvx[i]=p[i].vx;g.pvy[i]=p[i].vy;g.cd[i]=p[i].cd;}
        return g;
    }
    double value(Game&g,int scored,int horizon=35){
        if(scored==0)return 1000000;
        if(scored==1)return -1000000;
        Track path=track(g.bx,g.by,g.bvx,g.bvy,horizon);
        PlayerView q[4];
        for(int i=0;i<4;++i)q[i]={g.px[i],g.py[i],g.pvx[i],g.pvy[i],g.cd[i]};
        int first[4];for(int i=0;i<4;++i)first[i]=eta(q[i],path,480);
        int our=min(first[0],first[1]),opp=min(first[2],first[3]);
        int stop=min({our,opp,int(path.b.size())-1});
        auto bp=path.b[stop];
        double v=0;
        // Ownership and useful territory are the primary fallback when no clear shot exists.
        if(our<=opp){
            v=2500+0.72*bp.x-0.16*abs(bp.y-4000);
            v+=240*min(8,opp-our);
        }else{
            v=-1200+0.58*bp.x-0.04*abs(bp.y-4000);
            v-=180*min(8,our-opp);
            if(bp.x<4500)v-=2200*(4500-bp.x)/4500.;
        }
        v-=30*min(35,stop);
        // How much room is left after a defender's best possible acceleration?
        double margin=1e9;
        if(path.goal){
            int side=path.goal>0?2:0;
            for(int n=0;n<int(path.b.size());++n){
                for(int j=side;j<side+2;++j){
                    V d={path.b[n].x-q[j].x-q[j].vx*inertia[n],path.b[n].y-q[j].y-q[j].vy*inertia[n]};
                    double reach=radius[n]+(n>=q[j].cd?500:445);
                    margin=min(margin,d.norm()-reach);
                }
            }
            double prob=1.0/(1.0+exp(-bound((margin+80)/260.,-30,30)));
            if(path.goal>0){
                v+=prob*14000;
                v+=max(0.,1800+margin)*0.6;
            }else v-=prob*70000+7000;
        }
        return v;
    }
    Action choose(){
        prepare();
        int mover=leader;
        int back=p[0].x<p[1].x?0:1;
        if(p[1-back].x-p[back].x>1200&&p[back].x<5000&&o.bx>2500&&min(times[2],times[3])<=times[back]+2)mover=1-back;
        V movement=idx==mover?intercept(p[idx],tr):cover(p[idx],tr);
        Action out;out.ax=llround(movement.x);out.ay=llround(movement.y);out.msg=idx==leader?1:2;
        bool allowed=cankick(p[idx],o.bx,o.by) && (idx==leader||!cankick(p[leader],o.bx,o.by));
        if(!allowed)return out;
        Game initial=state();
        Action act[4];
        for(int j=0;j<4;++j){
            V a=j<2?(j==mover?intercept(p[j],tr):cover(p[j],tr)):intercept(p[j],tr);
            act[j].ax=llround(a.x)*(j>=2?-1:1);act[j].ay=llround(a.y);
            if(j>=2&&cankick(p[j],o.bx,o.by)){
                V k=limited(V{-double(o.bx),4000.-o.by},800);
                act[j].kx=-llround(k.x);act[j].ky=llround(k.y);
            }
        }
        act[idx]=out;
        vector<V> candidates;
        for(int power:{800,550,350,220})
            for(int i=0;i<72;++i){double a=i*acos(-1.)/36;candidates.push_back({power*cos(a),power*sin(a)});}
        for(int y=3020;y<=4980;y+=98){
            V target{12100.-o.bx,double(y)-o.by};
            candidates.push_back(limited(target,800));
            candidates.push_back(limited({12100.-o.bx,300.-y-o.by},800));
            candidates.push_back(limited({12100.-o.bx,15700.-y-o.by},800));
        }
        // Passing into the teammate's path is another candidate, with several flight times.
        for(int n:{5,8,12}){
            double ballScale=(1-pow(.97,n))/.03;
            V target=pos(p[1-idx])+vel(p[1-idx])*inertia[n];
            candidates.push_back(limited((target-V{double(o.bx),double(o.by)})/ballScale,800));
        }
        double best=-1e100;V chosen;
        for(V k:candidates){
            act[idx].kx=llround(k.x);act[idx].ky=llround(k.y);
            Game g=initial;int scored=g.step(act);
            double score=value(g,scored);
            // Favour progress at otherwise equivalent possession opportunities.
            score+=0.07*k.x-0.01*abs(k.y);
            if(score>best){best=score;chosen=k;}
        }
        // Refine promising directions and power without multiplying the broad scan.
        for(double angleStep:{2.0,0.6}){
            V center=chosen;
            double angle=atan2(center.y,center.x),power=center.norm();
            for(int da=-3;da<=3;++da)for(int dp=-1;dp<=1;++dp){
                double a=angle+da*angleStep*acos(-1.)/180.;
                double r=bound(power+dp*(angleStep>1?60:20),150,800);
                V k={r*cos(a),r*sin(a)};
                act[idx].kx=llround(k.x);act[idx].ky=llround(k.y);
                Game g=initial;int scored=g.step(act);
                double score=value(g,scored)+0.07*k.x-0.01*abs(k.y);
                if(score>best){best=score;chosen=k;}
            }
        }
        if(o.bvx>200){
            act[idx].kx=act[idx].ky=0;
            Game g=initial;int scored=g.step(act);
            if(value(g,scored)-350>best)chosen={0,0};
        }
        out.kx=llround(chosen.x);out.ky=llround(chosen.y);
        if(out.kx||out.ky)out.msg=3;
        return out;
    }
};

int main(){
    ios::sync_with_stdio(false);cin.tie(nullptr);
    string magic;int version;if(!(cin>>magic>>version))return 0;
    cin>>cfg.w>>cfg.h>>cfg.goal>>cfg.chamfer>>cfg.rp>>cfg.rb;
    cin>>cfg.acc_max>>cfg.pf_n>>cfg.pf_d>>cfg.bf_n>>cfg.bf_d;
    cin>>cfg.kick_max>>cfg.kick_reach>>cfg.kick_cooldown;
    cin>>cfg.e_wall_n>>cfg.e_wall_d>>cfg.e_ball_n>>cfg.e_ball_d>>cfg.e_pp_n>>cfg.e_pp_d;
    cin>>cfg.steps>>cfg.stall>>cfg.msg_max;
    Agent a;int stepMs,firstMs;cin>>a.idx>>stepMs>>firstMs;
    for(int n=0;n<=80;++n){inertia[n]=10*(1-pow(.9,n));radius[n]=400*n-3600*(1-pow(.9,n));control[n]=radius[n]/40;}
    auto&o=a.o;
    while(cin>>o.t>>o.left>>o.my_score>>o.opp_score>>o.mate_msg>>o.bx>>o.by>>o.bvx>>o.bvy){
        for(auto&p:o.pl)cin>>p.x>>p.y>>p.vx>>p.vy>>p.cd;
        Action r=a.choose();
        cout<<r.ax<<' '<<r.ay<<' '<<r.kx<<' '<<r.ky<<' '<<r.msg<<'\n'<<flush;
    }
}


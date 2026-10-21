#include "tron.hpp"
#include <array>
#include <bit>
namespace {
constexpr int SZ=43*43, STR=43, INF=30000;
constexpr int delta[4]={-STR,1,STR,-1};
// Simultaneous adversarial search, with a separate persistent path planner
// once no other live player can enter our connected component.
struct Bot {
 unsigned char wall[SZ]{};
 int pos[12],live[12],N,me,turn,W,H;
 short dist[12][SZ], closest[SZ];
 unsigned short masks[SZ];
 int owner[SZ],tin[SZ],low[SZ],timer,queue[SZ],nodes=0,evals=0,rival=-1;
 bool abort=false;
 double stop=8;
 unsigned long long rng=0x9e3779b97f4a7c15ULL;
 std::vector<int> saved; int savedTurn=-1;
 struct EvalEntry{uint64_t key=0;double value=0;unsigned generation=0;bool sep=false;};
 struct TTEntry{uint64_t key=0;double value=0;unsigned generation=0;short depth=0;char bound=0;};
 std::array<EvalEntry,32768> ec;
 std::array<TTEntry,32768> tt;
 uint64_t zcell[SZ],zhead[12][SZ],zt[12],hash=0;
 unsigned generation=0;bool hashReady=false,separated=false;
 int forecast[16][12]{},rootTurn=0;
 uint64_t mix(uint64_t x){x+=0x9e3779b97f4a7c15ULL;x=(x^(x>>30))*0xbf58476d1ce4e5b9ULL;x=(x^(x>>27))*0x94d049bb133111ebULL;return x^(x>>31);}
 void hashInit(){
  if(!hashReady){for(int p=0;p<SZ;p++){zcell[p]=mix(p+1);for(int k=0;k<12;k++)zhead[k][p]=mix(4096+SZ*k+p);}for(int k=0;k<12;k++)zt[k]=mix(50000+k);hashReady=true;}
  hash=0;for(int p=0;p<SZ;p++)if(wall[p])hash^=zcell[p];for(int k=0;k<N;k++)if(live[k])hash^=zhead[k][pos[k]];generation++;
 }
 int random(int n){rng^=rng<<7;rng^=rng>>9;return int(rng%unsigned(n));}
 bool timeout(){return (++nodes%16==0 && tron::budget_left()<stop)?abort=true:abort;}
 int degree(int p)const{return !wall[p-STR]+!wall[p+1]+!wall[p+STR]+!wall[p-1];}
 int priority(int k,int t)const{return (k-t%N+N)%N;}
 void distances(int k){
  std::fill(dist[k],dist[k]+SZ,short(INF));int b=0,e=0;queue[e++]=pos[k];dist[k][pos[k]]=0;
  while(b<e){int p=queue[b++],nd=dist[k][p]+1;for(int d:delta){int q=p+d;if(!wall[q]&&dist[k][q]==INF){dist[k][q]=short(nd);queue[e++]=q;}}}
 }
 struct Chamber{double a,b,exit;};
 Chamber dfs(int p,int par,int who){
  tin[p]=low[p]=++timer;double weight=1.0;
  Chamber out{(p%STR+p/STR)%2==0?weight:0,(p%STR+p/STR)%2!=0?weight:0,0};
  for(int d:delta){int q=p+d;if(owner[q]!=who||q==par)continue;
   if(tin[q])low[p]=std::min(low[p],tin[q]);
   else{auto c=dfs(q,p,who);low[p]=std::min(low[p],low[q]);
    if(low[q]>=tin[p])out.exit=std::max(out.exit,std::min(c.a+c.b,2*std::min(c.a,c.b)+1)+c.exit);
    else{out.a+=c.a;out.b+=c.b;out.exit=std::max(out.exit,c.exit);}
   }
  }return out;
 }
 // Multi-source geodesic Voronoi, then a rooted block-cut-tree capacity bound.
 // Checkerboard counts account for the parity constraint on simple grid paths.
 void areas(double value[12],bool chambers=true){
  std::fill(closest,closest+SZ,short(INF));
  std::fill(masks,masks+SZ,0);
  int qb=0,qe=0;
  for(int k=0;k<N;k++)if(live[k]){queue[qe++]=pos[k];closest[pos[k]]=0;masks[pos[k]]=1u<<k;}
  bool contact=false;
  while(qb<qe){int p=queue[qb++],nd=closest[p]+1;for(int d:delta){int q=p+d;if(wall[q])continue;
   if(closest[q]<INF && masks[q]!=masks[p])contact=true;
   if(closest[q]==INF){closest[q]=short(nd);masks[q]=masks[p];queue[qe++]=q;}
   else if(closest[q]==nd)masks[q]|=masks[p];
  }}
  separated=N==2&&!contact;
  std::fill(owner,owner+SZ,-1);std::fill(value,value+12,0.0);
  for(int y=1;y<=H;y++)for(int x=1;x<=W;x++){
   int p=y*STR+x;if(wall[p])continue;int kbest=-1;
   unsigned mask=masks[p];
   while(mask){int k=std::countr_zero(mask);mask&=mask-1;int phase=N==2?rootTurn:turn+closest[p]-1;if(kbest<0||priority(k,phase)<priority(kbest,phase))kbest=k;}
   owner[p]=kbest;
   if(kbest>=0)value[kbest]++;
  }
  if(chambers){std::fill(tin,tin+SZ,0);timer=0;
   for(int k=0;k<N;k++)if(live[k])owner[pos[k]]=k;
   for(int k=0;k<N;k++)if(live[k]){std::fill(tin,tin+SZ,0);timer=0;auto c=dfs(pos[k],-1,k);double usable=std::min(c.a+c.b,2*std::min(c.a,c.b)+1)+c.exit-1;value[k]=usable*.9+value[k]*.1;}
  }
 }
 double evaluate(){
  if((++evals&3)==0 && tron::budget_left()<stop){abort=true;return 0;}
  uint64_t key=hash^zt[turn%N];auto &ce=ec[key&32767];
  if(ce.generation==generation&&ce.key==key){separated=ce.sep;return ce.value;}
  separated=false;
  int alive=0;for(int k=0;k<N;k++)alive+=live[k];
  if(!live[me])return alive?(N==2?-100000.0:-100000.0-10000*alive)+10*(turn-rootTurn):0;
  if(alive==1)return 100000-10*(turn-rootTurn);
  double a[12];areas(a);double score=0;
  if(N==2)score=10*(a[me]-a[rival]);
  else{score=3*a[me];for(int k=0;k<N;k++)if(k!=me&&live[k])score+=100.0*(a[me]-a[k])/(30+a[me]+a[k]);score+=100*(N-alive);}
  int deg=degree(pos[me]);score+=deg==1?-3:deg==2?0:1;
  if(N==2){int od=degree(pos[rival]);score-=od==1?-3:od==2?0:1;}
  // A secured lead is worth more than the same amount of contested territory.
  if(N==2)score=90000.0*std::tanh(score/(separated?(60+.3*(a[me]+a[rival])):350.0));
  ce={key,score,generation,separated};return score;
 }
 struct Change{int old[12],dest[12],l[12];uint64_t oldHash;};
 Change stepMoves(const int dirs[],bool cautious=false){
  Change c;c.oldHash=hash;
  for(int k=0;k<N;k++){c.old[k]=pos[k];c.l[k]=live[k];c.dest[k]=dirs[k]>=0?pos[k]+delta[dirs[k]]:pos[k];if(live[k]&&(dirs[k]<0||wall[c.dest[k]]))live[k]=0;}
  int valid[12];std::copy(live,live+N,valid);
  for(int k=0;k<N;k++)if(valid[k])for(int j=0;j<N;j++)if(j!=k&&valid[j]&&c.dest[k]==c.dest[j]&&priority(j,turn)<priority(k,turn))live[k]=0;
  if(cautious && turn==rootTurn && live[me])for(int k=0;k<N;k++)if(k!=me&&k!=rival&&c.l[k]&&priority(k,turn)<priority(me,turn)){
   int dif=std::abs(c.old[k]-c.dest[me]);if(dif==1||dif==STR){live[me]=0;break;}
  }
  for(int k=0;k<N;k++){if(c.l[k])hash^=zhead[k][c.old[k]];if(live[k]){pos[k]=c.dest[k];wall[pos[k]]=1;hash^=zhead[k][pos[k]]^zcell[pos[k]];}}
  turn++;return c;
 }
 Change move(int d,int e){
  int dirs[12];for(int k=0;k<N;k++){
   int z=forecast[std::min(15,turn-rootTurn)][k];
   if(live[k]&&(z<0||wall[pos[k]+delta[z]])){z=-1;int best=-1;for(int a=0;a<4;a++)if(!wall[pos[k]+delta[a]]){int v=degree(pos[k]+delta[a]);if(v>best){z=a;best=v;}}}
   dirs[k]=z;
  }
  dirs[me]=d;if(rival>=0)dirs[rival]=e;
  return stepMoves(dirs,true);
 }
 void undo(const Change& c){turn--;hash=c.oldHash;for(int k=0;k<N;k++){if(live[k])wall[pos[k]]=0;pos[k]=c.old[k];live[k]=c.l[k];}}
 void predict(const tron::State& s){
  for(int t=0;t<16;t++)for(int k=0;k<N;k++)forecast[t][k]=s.last_dir[k];
  if(N==2)return;
  Change history[6];int count=0;
  for(int t=0;t<(N==4?4:6);t++){
   if(tron::budget_left()<stop+24)break;
   int dirs[12];
   for(int k=0;k<N;k++){
    dirs[k]=-1;if(!live[k])continue;double best=-1e20;
    for(int d=0;d<4;d++){int q=pos[k]+delta[d];if(wall[q])continue;int old=pos[k];pos[k]=q;wall[q]=1;
     double a[12];areas(a);double value=a[k]+.02*degree(q)+(d==forecast[t][k]?.005:0);
     pos[k]=old;wall[q]=0;
     if(value>best){best=value;dirs[k]=d;}
    }
    forecast[t][k]=dirs[k];for(int f=t+1;f<16;f++)forecast[f][k]=dirs[k];
   }
   history[count++]=stepMoves(dirs);
  }
  while(count)undo(history[--count]);
 }
 int moves(int k,int a[4]){int n=0;if(k>=0&&live[k])for(int d=0;d<4;d++)if(!wall[pos[k]+delta[d]])a[n++]=d;if(!n)a[n++]=-1;return n;}
 double search(int depth,double alpha,double beta){
  struct RestoreRival{int &value,old;~RestoreRival(){value=old;}} restoreRival{rival,rival};
  if(N>2&&depth>1){distances(me);int nearest=INF,next=-1;for(int k=0;k<N;k++)if(k!=me&&live[k]){int d=INF;for(int dd:delta)d=std::min(d,int(dist[me][pos[k]+dd])+1);if(d<nearest){nearest=d;next=k;}}
   int oldDist=INF;if(rival>=0&&live[rival])for(int dd:delta)oldDist=std::min(oldDist,int(dist[me][pos[rival]+dd])+1);
   if(N==4||nearest+2<oldDist||rival<0||!live[rival])rival=next;
  }
  if(timeout())return 0;if(!depth||!live[me]||(rival>=0&&!live[rival]))return evaluate();
  uint64_t key=hash^zt[turn%N]^mix(60000+rival);auto &te=tt[key&32767];
  double originalAlpha=alpha,originalBeta=beta;
  if(te.generation==generation&&te.key==key&&te.depth>=depth){if(te.bound==0)return te.value;if(te.bound==1)alpha=std::max(alpha,te.value);else beta=std::min(beta,te.value);if(alpha>=beta)return te.value;}
  double stand=evaluate();if(abort)return 0;if(separated)return stand;
  int ma[4],oa[4],nm=moves(me,ma),no=moves(rival,oa);double order[4][4],mv[4];int oi[4][4];
  for(int i=0;i<nm;i++){mv[i]=1e20;for(int j=0;j<no;j++){auto c=move(ma[i],oa[j]);order[i][j]=evaluate();undo(c);if(abort)return 0;oi[i][j]=j;mv[i]=std::min(mv[i],order[i][j]);}std::sort(oi[i],oi[i]+no,[&](int a,int b){return order[i][a]<order[i][b];});}
  int mi[4]={0,1,2,3};std::sort(mi,mi+nm,[&](int a,int b){return mv[a]>mv[b];});
  if(depth==1){
   return mv[mi[0]];
  }
  double best=-1e20;
  for(int ii=0;ii<nm;ii++){int i=mi[ii];double worst=1e20;
   for(int jj=0;jj<no;jj++){int j=oi[i][jj];auto c=move(ma[i],oa[j]);double v=search(depth-1,std::max(alpha,best),std::min(beta,worst));undo(c);if(abort)return 0;worst=std::min(worst,v);if(worst<=std::max(alpha,best))break;}
   best=std::max(best,worst);if(best>=beta)break;
  }te={key,best,generation,short(depth),char(best<=originalAlpha?2:best>=originalBeta?1:0)};return best;
 }
 int duel(int last){
  int ma[4],oa[4],nm=moves(me,ma),no=moves(rival,oa);if(nm==1)return std::max(0,ma[0]);
  int answer=ma[0];double scores[4],reply[4][4];int order[4]={0,1,2,3},oporder[4][4];
  for(int i=0;i<nm;i++){scores[i]=1e20;for(int j=0;j<no;j++){auto c=move(ma[i],oa[j]);reply[i][j]=evaluate();scores[i]=std::min(scores[i],reply[i][j]);undo(c);oporder[i][j]=j;}if(ma[i]==last)scores[i]+=.001;
   std::sort(oporder[i],oporder[i]+no,[&](int a,int b){return reply[i][a]<reply[i][b];});
  }
  std::sort(order,order+nm,[&](int a,int b){return scores[a]>scores[b];});answer=ma[order[0]];
  int maxdepth=N==2?12:N==4?4:6;
  for(int depth=2;depth<=maxdepth;depth++){
   if(tron::budget_left()<stop+2)break;double best=-1e20;int bestmove=answer;double next[4];
   for(int ii=0;ii<nm;ii++){int i=order[ii];double worst=1e20;
    for(int jj=0;jj<no;jj++){int j=oporder[i][jj];auto c=move(ma[i],oa[j]);double v=search(depth-1,best,worst);undo(c);if(abort)break;reply[i][j]=v;worst=std::min(worst,v);if(worst<best)break;}
    if(abort)break;next[i]=worst;if(worst>best){best=worst;bestmove=ma[i];}
   }
   if(abort)break;answer=bestmove;std::copy(next,next+nm,scores);std::sort(order,order+nm,[&](int a,int b){return scores[a]>scores[b];});for(int i=0;i<nm;i++)std::sort(oporder[i],oporder[i]+no,[&](int a,int b){return reply[i][a]<reply[i][b];});if(std::abs(best)>90000)break;
  }return answer;
 }
 // Rooted longest-path improvement: endpoint rotations, square insertions,
 // randomized restarts, and preservation of the best valid path across turns.
 int isolated(int last){
  const int root=pos[me];std::vector<int> best;
  if(savedTurn==turn-1&&saved.size()>1&&saved[1]==root){best.assign(saved.begin()+1,saved.end());for(size_t i=1;i<best.size();i++)if(wall[best[i]]){best.resize(i);break;}}
  if(best.empty())best.push_back(root);
  int maxpossible=0,parity[2]={0,0};distances(me);
  for(int p=0;p<SZ;p++)if(dist[me][p]<INF){maxpossible++;parity[(p%STR+p/STR)&1]++;}
  int color=(root%STR+root/STR)&1;maxpossible=std::min(maxpossible,2*std::min(parity[color]-1,parity[1-color])+1+(parity[1-color]>parity[color]-1));
  std::vector<int> path=best;int index[SZ];
  auto rebuild=[&](){std::fill(index,index+SZ,-1);for(int i=0;i<(int)path.size();i++)index[path[i]]=i;};rebuild();
  for(int iter=0;iter<180000;iter++){
   if((iter&255)==0&&tron::budget_left()<stop)break;
   int p=path.back(),choice=-1,bestdeg=100;
   bool explore=random(5)==0;
   for(int z=0,off=random(4);z<4;z++){int q=p+delta[(z+off)&3];if(wall[q]||index[q]>=0)continue;int deg=0;for(int dd:delta)deg+=!wall[q+dd]&&index[q+dd]<0;int score=explore?random(12):(deg?deg*3+random(4):30);if(score<bestdeg){bestdeg=score;choice=q;}}
   if(choice>=0){index[choice]=int(path.size());path.push_back(choice);}
   else{int ro[4],nr=0;for(int d:delta){int q=p+d,j=index[q];if(j>=0&&j+2<(int)path.size())ro[nr++]=j;}
    if(nr){int j=ro[random(nr)];std::reverse(path.begin()+j+1,path.end());for(int i=j+1;i<(int)path.size();i++)index[path[i]]=i;}
    else if(path.size()>1){int cut=std::max(1,int(path.size())-1-random(std::min(12,int(path.size())-1)));while(int(path.size())>cut){index[path.back()]=-1;path.pop_back();}}
   }
   if(path.size()>best.size())best=path;if(int(best.size())>=maxpossible)break;
   if(iter%256==255){
    for(int i=0;i+1<(int)path.size();i++){int a=path[i],b=path[i+1],diff=b-a,dirs[2];if(std::abs(diff)==1){dirs[0]=-STR;dirs[1]=STR;}else{dirs[0]=-1;dirs[1]=1;}
     for(int d:dirs){int u=a+d,v=b+d;if(!wall[u]&&!wall[v]&&index[u]<0&&index[v]<0){path.insert(path.begin()+i+1,{u,v});rebuild();i+=2;break;}}
    }
    if(path.size()>best.size())best=path;
    if(iter%2048==2047){path.assign(1,root);rebuild();}
    else if(random(4)==0){path=best;int cut=std::max(1,int(path.size())-1-random(std::max(1,int(path.size())/2)));path.resize(cut);rebuild();}
   }
  }
  saved=best;savedTurn=turn;if(best.size()>1)for(int d=0;d<4;d++)if(root+delta[d]==best[1])return d;
  for(int d=0;d<4;d++)if(!wall[root+delta[d]])return d;return last;
 }
 // Before accepting a partition, try to certify it with an actual survival
 // path longer than every cell the opponent could possibly still consume.
 int securePartition(){
  if(N!=2)return -1;
  int root=pos[me],enemy=1-me;
  for(int d=0;d<4;d++){
   int q=root+delta[d];if(wall[q])continue;
   int gap=std::abs(q-pos[enemy]);if((gap==1||gap==STR)&&priority(enemy,turn)<priority(me,turn))continue;
   pos[me]=q;wall[q]=1;turn++;distances(me);
   bool separate=true;for(int dd:delta)if(dist[me][pos[enemy]+dd]<INF)separate=false;
   if(separate){
    int mine=0,theirs=0;for(int p=0;p<SZ;p++)mine+=dist[me][p]<INF;
    distances(enemy);for(int p=0;p<SZ;p++)theirs+=dist[enemy][p]<INF;
    if(mine>theirs*1.12+4 && tron::budget_left()>stop+16){
     double oldstop=stop;stop=std::max(stop,tron::budget_left()-6.0);
     saved.clear();savedTurn=-1;isolated(d);stop=oldstop;
     if(int(saved.size())>theirs+1){
      saved.insert(saved.begin(),root);turn--;savedTurn=turn;pos[me]=root;wall[q]=0;return d;
     }
     saved.clear();savedTurn=-1;
    }
   }
   turn--;pos[me]=root;wall[q]=0;
  }
  return -1;
 }
 int decide(const tron::State& s){
  W=s.W;H=s.H;N=s.N;me=s.me;turn=s.turn;rootTurn=turn;stop=turn==0?1400:8;nodes=evals=0;abort=false;std::fill(wall,wall+SZ,1);
  for(int y=0;y<H;y++)for(int x=0;x<W;x++)wall[(y+1)*STR+x+1]=s.cell[y][x]!=tron::EMPTY;
  for(int k=0;k<N;k++){pos[k]=(s.y[k]+1)*STR+s.x[k]+1;live[k]=s.alive[k];}
  hashInit();
  if(!turn){saved.clear();savedTurn=-1;rng^=pos[me]*1234567ULL+N*37;}
  distances(me);rival=-1;int nearest=INF;
  for(int k=0;k<N;k++)if(k!=me&&live[k]){int d=INF;for(int dd:delta)d=std::min(d,int(dist[me][pos[k]+dd])+1);if(d<nearest){nearest=d;rival=k;}}
  if(rival<0)return isolated(s.last_dir[me]);
  int secure=securePartition();if(secure>=0)return secure;
  predict(s);saved.clear();savedTurn=-1;
  return duel(s.last_dir[me]);
 }
};
}
int decide(const tron::State& s){static Bot bot;return bot.decide(s);}

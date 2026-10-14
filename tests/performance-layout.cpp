#define SCATTER_STANDALONE
#define SCATTER_TEST
#include "../window-scatter.wh.cpp"
#include <chrono>
#include <random>
using namespace scatter;
static double Now(){return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();}
static bool Valid(const std::vector<Box>&b,Box area,double caption){
    if(b.empty())return false;
    for(size_t i=0;i<b.size();++i){auto a=b[i];a.h+=caption;if(!Contains(area,a))return false;
        for(size_t j=0;j<i;++j){auto c=b[j];c.h+=caption;if(Overlap(a,c))return false;}}
    return true;
}
int main(){
    bool passed=true;
    puts("count,monitors,packed_ms,free_ms,total_ms,reopen_ms,add_ms,valid,scale");fflush(stdout);
    for(int count:{8,16,32,64,128,256,512})for(int monitors:{1,2,4}){
        double packed=0,free=0,reopen=0,add=0,scale=0;bool valid=true;
        for(int m=0;m<monitors;++m){
            std::mt19937 random(4721+m);Box area{42,42,m%2?2476.:3756.,m%2?1468.:2028.};
            std::vector<Box>src;for(int n=0;n<count/monitors;++n)src.push_back({double(random()%2000),double(random()%1000),double(500+random()%1800),double(350+random()%1000)});
            double gap=std::min(33.,std::sqrt(area.w*area.h/src.size())*.1);double t=Now();
            auto seed=PackedLayout(src,area,33,39);packed+=Now()-t;if(seed.empty()){valid=false;continue;}
            t=Now();auto fit=FreePositions(src,seed,area,gap,39,seed[0].w/src[0].w);free+=Now()-t;
            auto before=fit.boxes.empty()?seed:fit.boxes;auto slots=SavePositions(src,area);valid&=Valid(before,area,39);scale=before[0].w/src[0].w;
            t=Now();auto repeat=StableLayout(src,before,area,33,39,slots);reopen+=Now()-t;
            src.push_back({600,500,1100,700});before.push_back({});slots.push_back({});
            t=Now();auto added=StableLayout(src,before,area,33,39,slots);add+=Now()-t;valid&=Valid(added,area,39);
        }
        printf("%d,%d,%.3f,%.3f,%.3f,%.3f,%.3f,%d,%.6f\n",count,monitors,packed,free,packed+free,reopen,add,valid,scale);fflush(stdout);
        passed&=valid;
    }
    return passed?0:1;
}

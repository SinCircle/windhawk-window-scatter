#define SCATTER_STANDALONE
#define SCATTER_TEST
#include "../window-scatter.wh.cpp"
#include <random>
using namespace scatter;
static bool passed=true;
static void Check(bool ok,const char*name){printf("%s %s\n",ok?"PASS":"FAIL",name);passed&=ok;}
static bool Valid(const std::vector<Box>&boxes,Box area,double caption){
    for(size_t a=0;a<boxes.size();++a){auto x=boxes[a];x.h+=caption;if(!Contains(area,x))return false;
        for(size_t b=a+1;b<boxes.size();++b){auto y=boxes[b];y.h+=caption;if(Overlap(x,y))return false;}}
    return true;
}
static double Distance(const Box&a,const Box&b){return std::hypot(a.cx()-b.cx(),a.cy()-b.cy());}
static double AreaDisplacement(const std::vector<Box>&src,const std::vector<Box>&boxes){
    double largest=0,cost=0;for(auto b:src)largest=std::max(largest,b.w*b.h);
    for(size_t i=0;i<src.size();++i)cost+=src[i].w*src[i].h/largest*std::pow(Distance(src[i],boxes[i]),2);return cost;
}
int main(int argc,char**argv){
    if(argc>1&&std::string(argv[1])=="--taskbar"){
        CoInitialize(nullptr);
        TaskbarOrder order;ULONGLONG begin=GetTickCount64();order.Start(nullptr);std::vector<TaskbarEntry> rows;
        do{Sleep(10);rows=order.Read();}while(rows.empty()&&GetTickCount64()-begin<3000);
        printf("Taskbar snapshot: %zu running groups in %llu ms (background worker)\n",rows.size(),GetTickCount64()-begin);
        bool valid=!rows.empty();for(const auto&r:rows)valid&=IsWindow(r.window)&&!r.group.empty();
        Check(valid,"live taskbar buttons map to valid source windows and application groups");
        App app;EnumDisplayMonitors(nullptr,nullptr,App::AddMonitor,reinterpret_cast<LPARAM>(&app));EnumWindows(App::AddWindow,reinterpret_cast<LPARAM>(&app));int minimized=0,matched=0;bool sorted=true;
        for(auto&v:app.views){for(auto&i:v->items)i.groupKey=WindowGroupKey(i.source);App::AssignTaskbarOrder(*v,rows);auto layout=App::Arrange(*v);std::vector<int> shelf;
            for(size_t n=0;n<v->items.size();++n)if(v->items[n].minimized){shelf.push_back(int(n));++minimized;if(v->items[n].taskbarRank!=INT_MAX)++matched;}
            std::sort(shelf.begin(),shelf.end(),[&](int a,int b){return layout[a].x<layout[b].x;});
            for(size_t n=1;n<shelf.size();++n)sorted&=v->items[shelf[n-1]].taskbarRank<=v->items[shelf[n]].taskbarRank;
        }
        printf("Live minimized windows: %d, mapped to taskbar groups: %d\n",minimized,matched);Check(sorted,"current minimized-window layout follows the live taskbar snapshot");
        auto snapshot=order.Read();order.Stop();auto stopped=order.Read();
        bool preserved=snapshot.size()==stopped.size();for(size_t n=0;n<stopped.size()&&preserved;++n)preserved&=snapshot[n].window==stopped[n].window&&snapshot[n].group==stopped[n].group;
        Check(!order.Running()&&preserved,"idle taskbar shutdown releases its worker while retaining the last ordering snapshot");
        order.Start(nullptr);Check(order.Running()&&order.Read().size()==snapshot.size(),"restarting the taskbar reader exposes its previous snapshot without a synchronous query");
        order.Stop();CoUninitialize();return passed?0:1;
    }
    Box area{42,42,1836,1080};double caption=39,gap=33;
    std::mt19937 random(71543);double stableDistance=0,compactDistance=0,elapsed=0,maximumMs=0;int crossing=0;bool valid=true,unchanged=true,uniform=true;
    LARGE_INTEGER frequency{},begin{},end{};QueryPerformanceFrequency(&frequency);
    for(int scene=0;scene<80;++scene){
        int count=2+scene%9;std::vector<Box> sources;
        for(int n=0;n<count;++n)sources.push_back({double(random()%1000),double(random()%650),double(500+random()%700),double(330+random()%480)});
        std::vector<LayoutSlot> slots;auto before=Layout(sources,area,gap,caption,&slots);auto repeated=StableLayout(sources,before,area,gap,caption,slots);
        for(int n=0;n<count;++n)unchanged&=Distance(before[n],repeated[n])<.01&&std::abs(before[n].w-repeated[n].w)<.01;
        auto anchors=before;sources.push_back({double(random()%1000),double(random()%650),double(500+random()%700),double(330+random()%480)});anchors.push_back({});slots.push_back({});
        QueryPerformanceCounter(&begin);auto after=StableLayout(sources,anchors,area,gap,caption,slots);QueryPerformanceCounter(&end);double milliseconds=1000.*(end.QuadPart-begin.QuadPart)/frequency.QuadPart;elapsed+=milliseconds;maximumMs=std::max(maximumMs,milliseconds);
        auto compact=Layout(sources,area,gap,caption);valid&=Valid(after,area,caption);
        for(size_t n=0;n<after.size();++n)uniform&=std::abs(after[n].w/sources[n].w-after[0].w/sources[0].w)<1e-6&&std::abs(after[n].h/sources[n].h-after[0].w/sources[0].w)<1e-6;
        for(int n=0;n<count;++n){stableDistance+=Distance(before[n],after[n]);compactDistance+=Distance(before[n],compact[n]);}
        for(int step=0;step<=40;++step){std::vector<Box> frame;for(int n=0;n<count;++n)frame.push_back(Mix(before[n],after[n],step/40.));if(!Valid(frame,area,caption)){crossing++;break;}}
        // Removal must close the vacancy rather than freeze its old rectangle.
        sources.pop_back();anchors.pop_back();slots.pop_back();sources.erase(sources.begin());anchors.erase(anchors.begin());slots.erase(slots.begin());
        for(auto&b:sources){b.x+=1300;b.y-=800;}
        std::vector<LayoutSlot> removedSlots;auto removed=StableLayout(sources,anchors,area,gap,caption,slots,&removedSlots);valid&=Valid(removed,area,caption);
        auto reopened=StableLayout(sources,removed,area,gap,caption,removedSlots);for(size_t n=0;n<removed.size();++n)unchanged&=Distance(removed[n],reopened[n])<.01;
        sources[0].w*=1.3;sources[0].h*=.8;auto resized=StableLayout(sources,anchors,area,gap,caption,slots);valid&=Valid(resized,area,caption);
    }
    printf("80 scene additions: travel %.1f vs %.1f px, crossings %d, mean solver %.3f ms, max %.3f ms\n",stableDistance,compactDistance,crossing,elapsed/80,maximumMs);
    Check(unchanged,"reopening an unchanged scene retains the complete arrangement");
    Check(valid&&uniform,"added and resized windows retain a common scale, caption clearance and monitor bounds");
    Check(crossing==0,"existing previews never intersect during sampled addition reflows");
    Check(stableDistance<compactDistance*.7,"existing-window travel falls by at least 30 percent across deterministic additions");
    std::vector<Box> physical{{150,150,500,420},{1150,150,500,420}};
    std::vector<LayoutSlot> physicalSlots;auto physicalBefore=Layout(physical,area,gap,caption,&physicalSlots);
    std::swap(physical[0].x,physical[1].x);std::vector<LayoutSlot> movedSlots;
    auto moved=StableLayout(physical,physicalBefore,area,gap,caption,physicalSlots,&movedSlots);
    Check(physicalBefore[0].cx()<physicalBefore[1].cx()&&moved[0].cx()>moved[1].cx(),"moving real windows past each other updates their overview left-right order");
    auto movedAgain=StableLayout(physical,moved,area,gap,caption,movedSlots);
    Check(Distance(moved[0],movedAgain[0])<.01&&Distance(moved[1],movedAgain[1])<.01,"an updated real arrangement remains stable on the next opening");
    physical={{400,80,650,300},{400,700,650,300}};
    physicalBefore=Layout(physical,area,gap,caption,&physicalSlots);std::swap(physical[0].y,physical[1].y);
    moved=StableLayout(physical,physicalBefore,area,gap,caption,physicalSlots);
    Check(physicalBefore[0].cy()<physicalBefore[1].cy()&&moved[0].cy()>moved[1].cy(),"moving real windows vertically updates their overview top-bottom order");
    bool insertedDirection=true;
    for(double x:{-500.,2000.}){
        physical={{500,200,500,400},{1000,200,500,400}};physicalBefore=Layout(physical,area,gap,caption,&physicalSlots);
        physical.push_back({x,200,500,400});physicalBefore.push_back({});physicalSlots.push_back({});
        moved=StableLayout(physical,physicalBefore,area,gap,caption,physicalSlots);
        insertedDirection&=x<0?moved[2].cx()<std::min(moved[0].cx(),moved[1].cx()):moved[2].cx()>std::max(moved[0].cx(),moved[1].cx());
    }
    Check(insertedDirection,"a new window's actual left or right position guides where it is inserted");
    Box sparseArea{0,0,2000,1000};
    std::vector<Box> screenshotScene{{20,20,1200,675},{1270,20,1200,675},{180,800,625,375},{1600,800,625,375}};
    std::vector<LayoutSlot> screenshotSlots;auto screenshotLayout=Layout(screenshotScene,sparseArea,26,26,&screenshotSlots);
    Check(Valid(screenshotLayout,sparseArea,26)&&screenshotLayout[2].cx()<screenshotLayout[3].cx()&&screenshotLayout[2].cy()>screenshotLayout[0].cy()&&screenshotLayout[3].cy()>screenshotLayout[1].cy(),
        "widely separated source windows retain their directions without forced center-line compaction");
    std::vector<Box> areaWeightedScene{{0,0,3840,2112},{0,0,3840,2112},{1189,610,2039,1235}};
    Box weightedArea{42,42,3756,2028};auto weighted=Layout(areaWeightedScene,weightedArea,33,39);
    std::vector<Box> reference{{1936.5,336.489451,1861.5,1023.825},{42,544.048578,1861.5,1023.825},{1936.5,1432.314451,988.437109,598.685547}};
    bool referenceMatch=Valid(weighted,weightedArea,39);double maxReferenceError=0;
    for(size_t i=0;i<weighted.size();++i)maxReferenceError=std::max(maxReferenceError,Distance(weighted[i],reference[i]));
    printf("Area-weighted approved reference: max center error %.6f px, scale %.8f\n",maxReferenceError,weighted[0].w/3840);
    Check(referenceMatch&&maxReferenceError<.01&&std::abs(weighted[0].w/3840-.484765625)<1e-8,
        "the native solver reproduces the independent area-weighted reference without shrinking windows");
    Check(std::abs(weighted[1].cy()-areaWeightedScene[1].cy())<.1&&weighted[2].cx()<weighted[0].cx()-300&&weighted[2].x>=weighted[1].right()+33-.01,
        "the left large window settles near its real vertical center while the small window yields without column-center alignment");
    // A former multi-window arrangement must not leave a maximized source
    // off-center after its neighbors disappear. Retain only path separation.
    Box maxArea{42,42,3756,1770};std::vector<Box> maximized(2,Box{0,0,3840,2112});
    std::vector<Box> skewed{{950,42,1500,825},{1060,939,1500,825}};
    auto maxSlots=SavePositions(maximized,maxArea);for(auto&s:maxSlots)s.count=3;
    std::vector<LayoutSlot> alignedSlots;auto aligned=StableLayout(maximized,skewed,maxArea,33,39,maxSlots,&alignedSlots);
    Check(Valid(aligned,maxArea,39)&&ClearReflow(skewed,aligned,39)&&std::abs(aligned[0].cx()-1920)<.001&&std::abs(aligned[1].cx()-1920)<.001,
        "stacked maximized previews lose stale horizontal offsets while retaining separated reflow paths");
    maximized.pop_back();aligned.pop_back();alignedSlots.pop_back();
    auto singleMax=StableLayout(maximized,aligned,maxArea,33,39,alignedSlots);
    Check(Valid(singleMax,maxArea,39)&&std::abs(singleMax[0].cx()-1920)<.001,
        "the last maximized preview returns to the horizontal monitor center after removal");
    maximized.push_back({0,0,3840,2112});skewed={{42,280,1500,825},{1580,510,1500,825}};maxSlots=SavePositions(maximized,maxArea);
    for(auto&s:maxSlots)s.count=3;
    aligned=StableLayout(maximized,skewed,maxArea,33,39,maxSlots);
    Check(Valid(aligned,maxArea,39)&&ClearReflow(skewed,aligned,39)&&std::abs(aligned[0].cy()-aligned[1].cy())<.001,
        "side-by-side maximized previews lose stale vertical offsets without changing their order");
    std::vector<Box> sixSources(3,Box{0,0,3840,2112});for(int i=0;i<3;++i)sixSources.push_back({1189.+i*20,610.+i*20,2039,1235});
    std::vector<LayoutSlot> sixSlots;auto six=Layout(sixSources,weightedArea,33,39,&sixSlots);
    printf("Six-window search: scale %.8f, area-weighted displacement %.3f\n",six[0].w/3840,AreaDisplacement(sixSources,six));
    Check(Valid(six,weightedArea,39)&&std::abs(six[0].w/3840-.3796686909)<1e-8&&AreaDisplacement(sixSources,six)<2.6e6,
        "three maximized and three floating sources escape the old packing seed at the same window sizes");
    auto betweenSources=sixSources,betweenAnchors=six;auto betweenSlots=sixSlots;
    betweenSources.erase(betweenSources.begin()+1);betweenAnchors.erase(betweenAnchors.begin()+1);betweenSlots.erase(betweenSlots.begin()+1);
    std::vector<LayoutSlot> freshSlots;auto freshOpen=StableLayout(betweenSources,betweenAnchors,weightedArea,33,39,betweenSlots,&freshSlots,false);
    auto uncached=Layout(betweenSources,weightedArea,33,39);bool unconstrained=freshOpen.size()==uncached.size();
    for(size_t i=0;i<freshOpen.size();++i)unconstrained&=Distance(freshOpen[i],uncached[i])<.001&&std::abs(freshOpen[i].w-uncached[i].w)<.001;
    Check(unconstrained,"a changed scene reopened from the desktop does not inherit obsolete preview separation directions");
    auto unchangedOpen=StableLayout(betweenSources,freshOpen,weightedArea,33,39,freshSlots,nullptr,false);bool sameOpen=true;
    for(size_t i=0;i<freshOpen.size();++i)sameOpen&=Distance(freshOpen[i],unchangedOpen[i])<.001;
    Check(sameOpen,"reopening the corrected unchanged scene keeps every preview in place");
    std::vector<Box> restoreScene(3,Box{0,0,3840,2112});restoreScene.insert(restoreScene.end(),{{1189,610,2039,1235},{1227,648,2039,1235},{1809,314,2039,1235}});
    auto restoredSix=Layout(restoreScene,weightedArea,33,39);
    Check(Valid(restoredSix,weightedArea,39)&&AreaDisplacement(restoreScene,restoredSix)<2.3e6,
        "the six-window restore-geometry fixture reduces area-weighted displacement without stacking every small window on one line");
    View openingView;openingView.bounds={0,0,3840,2112};openingView.dpi=1.5f;
    auto openingAnchors=PackedLayout(restoreScene,weightedArea,33,39);auto openingSlots=SavePositions(restoreScene,weightedArea);
    for(size_t i=0;i<restoreScene.size();++i){Item item;item.original=restoreScene[i];item.layoutAnchor=openingAnchors[i];item.anchorSlot=openingSlots[i];item.anchorSlot.count=7;openingView.items.push_back(std::move(item));}
    auto openingResult=App::Arrange(openingView,false);
    Check(Valid(openingResult,weightedArea,39)&&AreaDisplacement(restoreScene,openingResult)<2.3e6,
        "the real overview-opening layout entry releases cached packing after a desktop scene change");
    std::vector<Box> observed{{601,188,1297,714},{1935,41,1297,714},{802,975,1099,414},{1935,829,1297,714},{1214,1466,688,414},{1935,1609,688,414}};
    std::vector<Box> reconstructed;for(auto b:observed){double w=b.w/.337,h=b.h/.337;reconstructed.push_back({b.cx()-w*.5,b.cy()-h*.5,w,h});}
    Box approvedArea{42,42,3835-84,2100-84};std::vector<LayoutSlot> approvedSlots;auto approved=Layout(reconstructed,approvedArea,33,39,&approvedSlots);
    double approvedBody=0;for(auto b:approved)approvedBody+=b.w*b.h;
    printf("Approved six-window shape: scale %.5f, screen coverage %.3f, mixed %d\n",approved[0].w/reconstructed[0].w,approvedBody/(3835*2100),int(approvedSlots[0].mixed));
    Check(Valid(approved,approvedArea,39)&&approved[0].w/reconstructed[0].w>.385,"the approved mixed-size scene fits substantially larger than the old center-spine layout");
    Check(approved[4].cx()<approved[5].cx()&&std::abs(approved[4].cy()-approved[5].cy())<1&&approved[5].x-approved[4].right()<40,
        "the approved layout pairs the two small windows without pulling all previews onto one center line");
    bool approvedRelations=true;int clearRelations=0;
    for(size_t a=0;a<observed.size();++a)for(size_t b=0;b<a;++b){double dx=(observed[a].cx()-observed[b].cx())/3835,dy=(observed[a].cy()-observed[b].cy())/2100;
        if(std::abs(dx)>.08&&std::abs(dx)>1.25*std::abs(dy)){++clearRelations;approvedRelations&=(approved[a].cx()-approved[b].cx())*dx>0;}
        else if(std::abs(dy)>.08&&std::abs(dy)>1.25*std::abs(dx)){++clearRelations;approvedRelations&=(approved[a].cy()-approved[b].cy())*dy>0;}}
    Check(approvedRelations&&clearRelations==11,"the approved mixed layout retains all eleven dominant source directions");
    auto evolvingSources=reconstructed,evolvingAnchors=approved;auto evolvingSlots=approvedSlots;bool evolution=true;
    for(int cycle=0;cycle<3;++cycle){
        if(cycle==0){for(auto b:std::vector<Box>{{0,0,740,400},{2300,1350,600,420}}){evolvingSources.push_back(b);evolvingAnchors.push_back({});evolvingSlots.push_back({});}}
        else if(cycle==1){evolvingSources[4].w*=1.4;evolvingSources[4].h*=.7;}
        else{for(int id:{3,1}){evolvingSources.erase(evolvingSources.begin()+id);evolvingAnchors.erase(evolvingAnchors.begin()+id);evolvingSlots.erase(evolvingSlots.begin()+id);}}
        std::vector<LayoutSlot> updatedSlots;auto updated=StableLayout(evolvingSources,evolvingAnchors,approvedArea,33,39,evolvingSlots,&updatedSlots);
        evolution&=Valid(updated,approvedArea,39)&&ClearReflow(evolvingAnchors,updated,39);
        for(size_t i=0;i<updated.size();++i)evolution&=std::abs(updated[i].w/evolvingSources[i].w-updated[0].w/evolvingSources[0].w)<1e-6&&std::abs(updated[i].h/evolvingSources[i].h-updated[0].w/evolvingSources[0].w)<1e-6;
        auto same=StableLayout(evolvingSources,updated,approvedArea,33,39,updatedSlots);for(size_t i=0;i<same.size();++i)evolution&=Distance(same[i],updated[i])<.001;
        evolvingAnchors=std::move(updated);evolvingSlots=std::move(updatedSlots);
    }
    Check(evolution,"mixed layout history survives simultaneous additions, resize and multiple removals without crossing or drifting");
    bool screensValid=true,screensUniform=true;double screenElapsed=0;
    for(auto screen:std::vector<Box>{{0,0,2560,1552},{0,0,3840,2112},{0,0,3440,1440},{0,0,1440,2560},{0,0,1920,1080}}){
        std::vector<Box> physical;for(size_t i=0;i<observed.size();++i){auto b=reconstructed[i];b.x=observed[i].cx()/3835*screen.w-b.w*.5;b.y=observed[i].cy()/2100*screen.h-b.h*.5;physical.push_back(b);}
        auto target=Box{42,42,screen.w-84,screen.h-84};QueryPerformanceCounter(&begin);auto packed=Layout(physical,target,33,39);QueryPerformanceCounter(&end);screenElapsed+=1000.*(end.QuadPart-begin.QuadPart)/frequency.QuadPart;
        double left=1e100,top=1e100,right=-1e100,bottom=-1e100;for(size_t i=0;i<packed.size();++i){auto b=packed[i];screensUniform&=std::abs(b.w/physical[i].w-packed[0].w/physical[0].w)<1e-6&&std::abs(b.h/physical[i].h-packed[0].w/physical[0].w)<1e-6;
            left=std::min(left,b.x);top=std::min(top,b.y);right=std::max(right,b.right());bottom=std::max(bottom,b.bottom()+39);}
        screensValid&=Valid(packed,target,39);
        printf("Screen %.0fx%.0f: scale %.4f, footprint %.3fx%.3f\n",screen.w,screen.h,packed[0].w/physical[0].w,(right-left)/screen.w,(bottom-top)/screen.h);
    }
    Check(screensValid&&screensUniform,"normal, wide and portrait screens keep uniform aspect ratios and all captions inside bounds");
    printf("Five screen shapes: mean initial solver %.3f ms\n",screenElapsed/5);
    std::vector<Box> neighbors(6,Box{0,0,800,450});LayoutStacks twoRows{{0,1,2},{3,4,5}};
    auto full=PackStacks(neighbors,twoRows,false,{},area,gap,caption).boxes;auto retained=full;
    auto slots=SaveSlots(neighbors,full,twoRows,false,area);neighbors.erase(neighbors.begin()+1);retained.erase(retained.begin()+1);slots.erase(slots.begin()+1);
    auto filled=StableLayout(neighbors,retained,area,gap,caption,slots);
    Check(filled[1].x<full[2].x-10&&filled[1].x-filled[0].right()<gap+1,"neighbors move inward to fill a removed middle window's vacancy");
    Check(ClearReflow(retained,filled,caption)&&filled[0].x<filled[1].x&&filled[2].x<filled[3].x&&filled[3].x<filled[4].x,"local compaction preserves separated paths and left-to-right order without locking rows");
    bool crowdedValid=true,crowdedStable=true,crowdedReflow=true;double crowdedMax=0;
    for(int count:{1,4,12,24,40}){
        std::vector<Box> crowded;for(int i=0;i<count;++i)crowded.push_back({double(random()%2600)-400,double(random()%1300)-200,double(480+random()%1400),double(260+random()%900)});
        Box monitor{-1920,120,1864,1024};for(auto&b:crowded){b.x+=monitor.x;b.y+=monitor.y;}
        std::vector<LayoutSlot> remembered;QueryPerformanceCounter(&begin);auto fitted=Layout(crowded,monitor,22,26,&remembered);QueryPerformanceCounter(&end);
        double ms=1000.*(end.QuadPart-begin.QuadPart)/frequency.QuadPart;crowdedMax=std::max(crowdedMax,ms);
        double clearance=std::min(22.,std::sqrt(monitor.w*monitor.h/count)*.10);
        crowdedValid&=fitted.size()==crowded.size()&&Valid(fitted,monitor,26);
        for(size_t i=0;i<fitted.size();++i){crowdedValid&=std::abs(fitted[i].w/crowded[i].w-fitted[0].w/crowded[0].w)<1e-6;
            for(size_t j=0;j<i;++j)crowdedValid&=SeparationDirections(fitted[i],fitted[j],clearance,26)!=0;}
        auto repeated=StableLayout(crowded,fitted,monitor,22,26,remembered);for(size_t i=0;i<fitted.size();++i)crowdedStable&=Distance(fitted[i],repeated[i])<.001;
        crowded.push_back({monitor.cx()-350,monitor.cy()-220,700,440});fitted.push_back({});remembered.push_back({});
        QueryPerformanceCounter(&begin);auto inserted=StableLayout(crowded,fitted,monitor,22,26,remembered);QueryPerformanceCounter(&end);
        double updateMs=1000.*(end.QuadPart-begin.QuadPart)/frequency.QuadPart;crowdedMax=std::max(crowdedMax,updateMs);
        crowdedReflow&=Valid(inserted,monitor,26)&&ClearReflow(fitted,inserted,26);
        printf("Crowded negative-origin screen: %d windows, initial %.3f ms, insertion %.3f ms\n",count,ms,updateMs);
    }
    Check(crowdedValid&&crowdedStable,"one through forty windows retain caption spacing, uniform scale and stable reopening on a negative-origin monitor");
    Check(crowdedReflow,"crowded-scene insertions retain separated motion paths for existing windows");
    Check(crowdedMax<100,"bounded search keeps the crowded-scene geometry pass below 100 ms");
    // Hundreds of sources exercise the optimized relation scorer and redundant
    // constraint reduction, rather than only the small-scene search path.
    bool largeValid=true,largeStable=true,largePaths=true,cacheExact=true;
    for(int count:{64,128,256,512}){
        Box monitor{-3840,-550,3756,2028};std::vector<Box> sources;std::mt19937 generator(4721);
        for(int n=0;n<count;++n)sources.push_back({monitor.x+double(generator()%2000),monitor.y+double(generator()%1000),double(500+generator()%1800),double(350+generator()%1000)});
        std::vector<LayoutSlot> saved;auto fitted=Layout(sources,monitor,33,39,&saved);largeValid&=fitted.size()==sources.size()&&Valid(fitted,monitor,39);
        if(fitted.size()!=sources.size())continue;
        double scale=fitted[0].w/sources[0].w;SourceRelationCache cached(sources,monitor);
        cacheExact&=std::abs(cached.Cost(fitted,monitor,scale,1e100)-SourceRelationCost(sources,fitted,monitor,scale))<1e-10;
        for(size_t n=0;n<fitted.size();++n)largeValid&=std::abs(fitted[n].w/sources[n].w-scale)<1e-8&&std::abs(fitted[n].h/sources[n].h-scale)<1e-8;
        auto repeated=StableLayout(sources,fitted,monitor,33,39,saved);for(size_t n=0;n<fitted.size();++n)largeStable&=Distance(fitted[n],repeated[n])<1e-8;
        sources.push_back({monitor.x+600,monitor.y+500,1100,700});fitted.push_back({});saved.push_back({});
        auto added=StableLayout(sources,fitted,monitor,33,39,saved);largeValid&=added.size()==sources.size()&&Valid(added,monitor,39);largePaths&=ClearReflow(fitted,added,39);
    }
    Check(largeValid,"64 through 512 mixed-size windows keep common scale, caption clearance and negative-origin monitor bounds");
    Check(largeStable&&largePaths,"large-scene reopening is exact and additions retain separated motion paths");
    Check(cacheExact,"cached source-relation scores agree with the original formula to 1e-10");
    std::vector<PositionEdge> redundant;for(int a=0;a<64;++a)for(int b=a+1;b<64;++b)redundant.push_back({a,b,2.+(a+b)%7});
    auto reduced=redundant;ReducePositionEdges(64,reduced);bool implied=true;
    for(int a=0;a<64;++a){std::vector<double> reach(64,-1e100);reach[a]=0;
        for(int n=a;n<64;++n)for(auto edge:reduced)if(edge.first==n)reach[edge.second]=std::max(reach[edge.second],reach[n]+edge.distance);
        for(auto edge:redundant)if(edge.first==a)implied&=reach[edge.second]>=edge.distance-1e-9;}
    Check(implied&&reduced.size()<redundant.size()/4,"reduced constraints still imply every original weighted separation constraint");
    auto cycle=redundant;cycle.push_back({63,0,1});auto originalCycle=cycle;ReducePositionEdges(64,cycle);
    Check(cycle.size()==originalCycle.size(),"cyclic constraint graphs retain the existing infeasibility checks");
    std::vector<Box> example{{43,42,788,507},{227,622,1337,508},{1996,42,1589,870},{2219,984,1580,870},{475,1275,838,508},{1348,1276,838,508}};
    for(auto&b:example){b.w/=.65;b.h/=.65;}
    Box wideArea{42,42,3754,1858};auto compactExample=Layout(example,wideArea,33,39);
    double used=0,left=1e9,top=1e9,right=0,bottom=0;for(auto b:compactExample){used+=b.w*(b.h+39);left=std::min(left,b.x);right=std::max(right,b.right());top=std::min(top,b.y);bottom=std::max(bottom,b.bottom()+39);}
    double coverage=used/((right-left)*(bottom-top));printf("Six-window example coverage: %.3f\n",coverage);
    Check(Valid(compactExample,wideArea,39),"the earlier mixed-size scene retains caption clearance and monitor bounds");
    std::vector<Box> reported{{675,178,550,355},{1260,166,578,375},{16,700,478,250},{529,700,478,250},{1040,612,710,430},{1784,612,710,430}};
    for(auto&b:reported){b.w/=.45;b.h/=.45;}
    Box reportedArea{28,28,2458,1200};auto restored=Layout(reported,reportedArea,22,26);
    double restoredUsed=0;for(auto b:restored)restoredUsed+=b.w*(b.h+26);
    printf("Reported screenshot: full-area coverage %.3f, scale %.4f\n",restoredUsed/(reportedArea.w*reportedArea.h),restored[0].w/reported[0].w);
    Check(Valid(restored,reportedArea,26)&&restoredUsed/(reportedArea.w*reportedArea.h)>.65,"the reported uneven scene uses most of the full available area");
    if(argc>2&&std::string(argv[1])=="--diagram"){FILE*out=fopen(argv[2],"w");if(out){fprintf(out,"{\"width\":2514,\"height\":1256,\"boxes\":[");for(size_t n=0;n<restored.size();++n){auto b=restored[n];fprintf(out,"%s{\"x\":%.3f,\"y\":%.3f,\"w\":%.3f,\"h\":%.3f}",n?",":"",b.x,b.y,b.w,b.h);}fprintf(out,"]}");fclose(out);}}
    View view;view.bounds={0,0,1920,1200};view.dpi=1.5f;view.monitor=reinterpret_cast<HMONITOR>(1);
    const wchar_t*keys[]{L"B",L"A",L"C",L"B",L"A"};for(int n=0;n<5;++n){Item i;i.original={0,0,800,450};i.minimized=true;i.groupKey=keys[n];i.source=reinterpret_cast<HWND>(UINT_PTR(n+10));view.items.push_back(std::move(i));}
    std::vector<TaskbarEntry> order{{view.items[2].source,view.monitor,L"C",20,0},{view.items[1].source,view.monitor,L"A",70,0},{view.items[0].source,view.monitor,L"B",120,0}};
    App::AssignTaskbarOrder(view,order);auto shelf=App::Arrange(view);
    Check(shelf[2].x<shelf[1].x&&shelf[1].x<shelf[4].x&&shelf[4].x<shelf[0].x&&shelf[0].x<shelf[3].x,"shelf follows taskbar groups and keeps multiple windows of each application adjacent");
    App app;for(auto&i:view.items){i.target=shelf[&i-view.items.data()];i.sourceProcess=20;}
    auto saved=std::make_unique<View>();saved->monitor=view.monitor;saved->bounds=view.bounds;saved->dpi=view.dpi;saved->items=std::move(view.items);app.views.push_back(std::move(saved));app.RememberLayout();
    View reopened;reopened.monitor=view.monitor;reopened.bounds=view.bounds;reopened.dpi=view.dpi;Item item;item.source=app.views[0]->items[0].source;item.sourceProcess=20;reopened.items.push_back(std::move(item));app.RecallLayout(reopened);
    Check(reopened.items[0].layoutAnchor.w>0,"geometry history survives destruction and recreation of overview items");
    reopened.items[0].layoutAnchor={};reopened.items[0].sourceProcess=21;app.RecallLayout(reopened);
    Check(reopened.items[0].layoutAnchor.w==0,"a recycled window handle from another process does not inherit old placement");
    return passed?0:1;
}

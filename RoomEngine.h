#pragma once
#include <array>
#include <vector>
#include <algorithm>
#include <cmath>

// Room Reverb engine by Basement Instruments, built into Beat Station.
// Framework-independent module: no JUCE, GUI, file I/O, or process-time allocation.
// Beat Station changes for a cleaner, more exact sound (the architecture is unchanged):
//  - the eight tail delays are whole numbers of samples and all prime, so they share no resonances and need no interpolation once settled
//  - the early reflections land on exact samples, so they keep their full treble
//  - a low cut on the reverb only (lowCut, Hz) keeps the low end of the mix tight
//  - reflections are only recalculated when a control actually moves
namespace room {
constexpr double pi=3.14159265358979323846;
inline float db(float v){return 20*std::log10(std::max(v,1.e-9f));}
inline float gain(float d){return std::pow(10.f,d/20);}
struct Vec {float x=0,y=0,z=0;};
inline Vec operator-(Vec a,Vec b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline float length(Vec a){return std::sqrt(a.x*a.x+a.y*a.y+a.z*a.z);}
enum Subject {Rapper,Singer,Drums,Tuba,AcousticGuitar,ElectricGuitar,Piano,Synth};
struct Parameters {
    float width=6,depth=8,height=3;
    float sx=.25f,sy=.25f,sz=1.5f,lx=.80f,ly=.45f,lz=1.5f;
    float decay=1.2f,predelay=15,damping=6500,absorption=.35f,diffusion=.8f,spread=1;
    float wet=.25f,direct=1,early=.65f,late=.65f,duck=.3f,output=0,orientation=0;
    float lowCut=90;
    int subject=Rapper;bool bypass=false,wetSolo=false;
};
inline Parameters sanitise(Parameters p) {
    auto clamp=[](float v,float lo,float hi,float def){return std::isfinite(v)?std::clamp(v,lo,hi):def;};
    p.width=clamp(p.width,2,25,6);p.depth=clamp(p.depth,2,35,8);p.height=clamp(p.height,2,12,3);
    p.sx=clamp(p.sx,.05f,.95f,.25f);p.sy=clamp(p.sy,.05f,.95f,.25f);p.lx=clamp(p.lx,.05f,.95f,.8f);p.ly=clamp(p.ly,.05f,.95f,.45f);
    p.sz=clamp(p.sz,.2f,p.height-.1f,1.5f);p.lz=clamp(p.lz,.5f,p.height-.1f,1.5f);
    p.decay=clamp(p.decay,.15f,8,1.2f);p.predelay=clamp(p.predelay,0,120,15);
    p.damping=clamp(p.damping,300,16000,6500);p.absorption=clamp(p.absorption,.05f,.95f,.35f);
    p.diffusion=clamp(p.diffusion,0,1,.8f);p.spread=clamp(p.spread,0,1,1);
    p.wet=clamp(p.wet,0,1,.25f);p.direct=clamp(p.direct,0,1,1);p.early=clamp(p.early,0,1,.65f);p.late=clamp(p.late,0,1,.65f);
    p.duck=clamp(p.duck,0,1,.3f);p.output=clamp(p.output,-24,12,0);p.orientation=clamp(p.orientation,-180,180,0);
    p.lowCut=clamp(p.lowCut,20,600,90);
    p.subject=std::clamp(p.subject,0,7);return p;
}
struct Reflection {float seconds=0,left=0,right=0;int mx=0,my=0,mz=0;};
inline float imageCoordinate(float source,float size,int m) {
    return (std::abs(m)%2)==0?m*size+source:(m+1)*size-source;
}
inline float radiation(int subject,Vec direction,float orientation) {
    const float norm=std::max(.01f,length(direction));
    if(subject==Drums||subject==Piano||subject==Synth)return 1;
    if(subject==Tuba)return .65f+.35f*std::max(0.f,direction.z/norm);
    const float angle=orientation*float(pi/180);
    const float facing=(direction.x*std::sin(angle)+direction.y*std::cos(angle))/norm;
    const float amount=subject==Rapper?.45f:subject==Singer?.35f:.25f;
    return 1-amount+amount*std::max(0.f,facing);
}
inline std::array<Reflection,24> reflections(Parameters p) {
    p=sanitise(p);std::array<Reflection,24> taps{};int n=0;float geometryPower=0;
    Vec source{p.sx*p.width,p.sy*p.depth,p.sz},listener{p.lx*p.width,p.ly*p.depth,p.lz};
    float directDistance=length(listener-source);
    for(int mx=-2;mx<=2;++mx)for(int my=-2;my<=2;++my)for(int mz=-2;mz<=2;++mz) {
        const int order=std::abs(mx)+std::abs(my)+std::abs(mz);if(order<1||order>2)continue;
        Vec image{imageCoordinate(source.x,p.width,mx),imageCoordinate(source.y,p.depth,my),imageCoordinate(source.z,p.height,mz)};
        Vec v=listener-image;float distance=std::max(.1f,length(v));
        Vec departure{std::abs(mx)%2?-v.x:v.x,std::abs(my)%2?-v.y:v.y,std::abs(mz)%2?-v.z:v.z};
        const float geometry=std::exp(-distance*.003f)*.55f/std::max(1.f,distance);
        geometryPower+=geometry*geometry*.5f;
        float amp=std::pow(1-p.absorption,float(order))*radiation(p.subject,departure,p.orientation)*geometry;
        float pan=std::clamp((image.x-listener.x)/distance*p.spread,-1.f,1.f);
        taps[n++]={std::max(0.f,distance-directDistance)/343.f+p.predelay*.001f,
            amp*std::sqrt(.5f*(1-pan)),amp*std::sqrt(.5f*(1+pan)),mx,my,mz};
    }
    // Reference-geometry power calibration; absorption and source radiation
    // remain outside this normalisation so those controls still change level.
    const float calibration=std::min(40.f,1.1f/std::sqrt(std::max(1.e-9f,geometryPower)));
    for(auto& tap:taps){tap.left*=calibration;tap.right*=calibration;}
    return taps;
}
class Delay {
public:
    void prepare(int capacity){buffer.assign(size_t(capacity+4),0);cursor=0;}
    void write(float x){buffer[cursor]=x;cursor=(cursor+1)%buffer.size();}
    float read(float samples) const {
        samples=std::clamp(samples,1.f,float(buffer.size()-3));
        // Integer wrapping avoids a tiny negative float rounding up to exactly
        // buffer.size() when added back to the ring capacity.
        const auto whole=size_t(samples);const float f=samples-float(whole);
        const auto a=(cursor+buffer.size()-whole)%buffer.size();
        const auto b=(a+buffer.size()-1)%buffer.size();
        return buffer[a]+f*(buffer[b]-buffer[a]);
    }
private:std::vector<float> buffer;size_t cursor=0;
};
struct PeakMeter {
    float held=0,envelope=0,average=0;bool clip=false;float release=.9999f,vuA=.999f;
    void prepare(double sr){held=envelope=average=0;clip=false;release=float(std::exp(-1/(.5*sr)));vuA=float(std::exp(-1/(.065*sr)));}
    void push(float x){float a=std::abs(x);held=std::max(held,a);clip=clip||a>=1;envelope=std::max(a,envelope*release);average=vuA*average+(1-vuA)*a;}
    float vu() const{return db(average*float(pi/2))+18;}
    void reset(){held=0;clip=false;}
};
class Engine {
public:
    void prepare(double sampleRate,const Parameters& p=Parameters{}) {
        sr=sampleRate;params=sanitise(p);earlyLine.prepare(int(sr*1.5));lateInput.prepare(int(sr*.2));
        earlySide.prepare(int(sr*1.5));lateSide.prepare(int(sr*.2));
        for(auto& d:lines)d.prepare(int(sr*.4));
        for(auto& d:midDiffusers)d.prepare(int(sr*.03));
        for(auto& d:sideDiffusers)d.prepare(int(sr*.03));
        low.fill(0);update(params,true);env=0;wet=params.wet;direct=params.direct;outGain=gain(params.output);bypass=params.bypass?1.f:0.f;
        earlyLevel=params.early;lateLevel=params.late;duck=params.duck;diffusion=params.diffusion;
        smoothing=float(1-std::exp(-1/(.04*sr)));for(auto& m:input)m.prepare(sr);for(auto& m:output)m.prepare(sr);
        spread=params.spread;distanceSend=targetDistance;targetGain=gain(params.output);
        envelopeAttack=float(std::exp(-1/(.005*sr)));envelopeRelease=float(std::exp(-1/(.18*sr)));
        solo=params.wetSolo?1.f:0.f;for(auto& m:wetMeter)m.prepare(sr);
    }
    // Call once per block, not once per sample.
    void setParameters(const Parameters& p){
        const Parameters q=sanitise(p);
        const bool same=q.width==params.width&&q.depth==params.depth&&q.height==params.height&&q.sx==params.sx&&q.sy==params.sy&&q.sz==params.sz
            &&q.lx==params.lx&&q.ly==params.ly&&q.lz==params.lz&&q.decay==params.decay&&q.predelay==params.predelay&&q.damping==params.damping
            &&q.absorption==params.absorption&&q.spread==params.spread&&q.output==params.output&&q.orientation==params.orientation
            &&q.subject==params.subject&&q.lowCut==params.lowCut;
        params=q;if(!same)update(params,false);
    }
    void resetPeaks(unsigned mask){for(int c=0;c<2;++c){if(mask&1)input[c].reset();if(mask&2)output[c].reset();}}
    std::array<float,2> process(float l,float r,bool monoOutput=false) {
        if(!std::isfinite(l))l=0;
        if(!std::isfinite(r))r=0;
        input[0].push(l);input[1].push(r);
        auto smooth=[&](float& a,float b){a+=smoothing*(b-a);};
        smooth(wet,params.wet);smooth(direct,params.direct);smooth(earlyLevel,params.early);smooth(lateLevel,params.late);
        smooth(duck,params.duck);smooth(outGain,targetGain);smooth(diffusion,params.diffusion);smooth(bypass,params.bypass?1.f:0.f);
        smooth(spread,params.spread);smooth(distanceSend,targetDistance);
        smooth(solo,params.wetSolo?1.f:0.f);
        smooth(predelay,targetPredelay);smooth(dampingAlpha,targetDamping);
        float mono=(l+r)*.5f,sideInput=(l-r)*.5f;earlyLine.write(mono);lateInput.write(mono);
        earlySide.write(sideInput);lateSide.write(sideInput);
        float eL=0,eR=0;
        for(int i=0;i<24;++i){smooth(taps[i].seconds,target[i].seconds);smooth(taps[i].left,target[i].left);smooth(taps[i].right,target[i].right);
            const float delay=taps[i].seconds*float(sr)+1;
            const float v=earlyLine.read(delay),s=earlySide.read(delay)*spread;
            eL+=(v+s)*taps[i].left;eR+=(v-s)*taps[i].right;}
        std::array<float,8> values{},mixed{};
        for(int i=0;i<8;++i){smooth(delays[i],targetDelays[i]);smooth(feedback[i],targetFeedback[i]);
            float x=lines[i].read(delays[i]);low[i]+=dampingAlpha*(x-low[i]);values[i]=low[i];mixed[i]=values[i];}
        // Always orthogonal: diffusion must not introduce extra feedback loss
        // and shorten the user-selected decay. Density is shaped at the input.
        for(int step=1;step<8;step*=2)for(int i=0;i<8;i+=step*2)for(int j=0;j<step;++j){float a=mixed[i+j],b=mixed[i+j+step];mixed[i+j]=a+b;mixed[i+j+step]=a-b;}
        float send=lateInput.read(predelay+1),sendSide=lateSide.read(predelay+1)*spread;
        constexpr float midMs[4]={3.1f,4.7f,7.9f,11.3f},sideMs[4]={4.1f,6.3f,9.7f,14.9f};
        const float diffuserGain=.72f*diffusion;
        for(int i=0;i<4;++i){
            const float delayed=midDiffusers[i].read(midMs[i]*float(sr)*.001f);
            const float value=delayed-diffuserGain*send;midDiffusers[i].write(send+diffuserGain*value);send=value;
            const float delayedSide=sideDiffusers[i].read(sideMs[i]*float(sr)*.001f);
            const float sideValue=delayedSide-diffuserGain*sendSide;sideDiffusers[i].write(sendSide+diffuserGain*sideValue);sendSide=sideValue;
        }
        float tailL=0,tailR=0;constexpr float rightSigns[8]={1,-1,-1,1,-1,1,1,-1};
        for(int i=0;i<8;++i){smooth(injection[i],targetInjection[i]);
            const float input=(send+rightSigns[i]*sendSide)*.35355339f*injection[i];
            lines[i].write(input+feedback[i]*mixed[i]*.35355339f);tailL+=values[i];tailR+=values[i]*rightSigns[i];}
        float mid=(tailL+tailR)*.5f,side=(tailL-tailR)*.5f*spread;
        tailL=mid+side;tailR=mid-side;
        const float detector=std::max(std::abs(l),std::abs(r));float a=detector>env?envelopeAttack:envelopeRelease;
        env=a*env+(1-a)*detector;float duckGain=1/(1+duck*env*12);
        const float distance=distanceSend;
        const float wetAmount=wet+solo*(1-wet),dryAmount=direct*(1-solo);
        float wetL=earlyLevel*eL+lateLevel*tailL*distance,wetR=earlyLevel*eR+lateLevel*tailR*distance;
        smooth(hpG,targetHpG);                                         // low cut on the reverb only (12 dB per octave)
        {const float k=1.41421356f,a1=1/(1+hpG*(hpG+k)),a2=hpG*a1,a3=hpG*a2;
         float v3=wetL-hpS[1],v1=a1*hpS[0]+a2*v3,v2=hpS[1]+a2*hpS[0]+a3*v3;hpS[0]=2*v1-hpS[0];hpS[1]=2*v2-hpS[1];wetL=wetL-k*v1-v2;
         v3=wetR-hpS[3];v1=a1*hpS[2]+a2*v3;v2=hpS[3]+a2*hpS[2]+a3*v3;hpS[2]=2*v1-hpS[2];hpS[3]=2*v2-hpS[3];wetR=wetR-k*v1-v2;}
        const std::array<float,2> wetSignal{{wetAmount*duckGain*wetL*outGain,wetAmount*duckGain*wetR*outGain}};
        std::array<float,2> y{{dryAmount*l*outGain+wetSignal[0],dryAmount*r*outGain+wetSignal[1]}};
        y[0]+=bypass*(l-y[0]);y[1]+=bypass*(r-y[1]);
        if(monoOutput)y[0]=y[1]=(y[0]+y[1])*.5f;
        for(int c=0;c<2;++c){if(!std::isfinite(y[c]))y[c]=0;output[c].push(y[c]);wetMeter[c].push(wetSignal[c]*(1-bypass));}
        return y;
    }
    std::array<PeakMeter,2> input,output,wetMeter;
    const Parameters& settings()const{return params;}
private:
    double sr=48000;Parameters params;
    Delay earlyLine,lateInput,earlySide,lateSide;std::array<Delay,8> lines;
    std::array<Delay,4> midDiffusers,sideDiffusers;
    std::array<Reflection,24> taps{},target{};
    std::array<float,8> low{},delays{},targetDelays{},feedback{},targetFeedback{};
    std::array<float,8> injection{},targetInjection{};
    float wet=0,direct=1,outGain=1,bypass=0,earlyLevel=1,lateLevel=1,duck=0,diffusion=.8f,smoothing=.001f;
    float predelay=0,targetPredelay=0,dampingAlpha=.5f,targetDamping=.5f,env=0,distanceSend=1;
    float targetDistance=1,targetGain=1,spread=1,envelopeAttack=.99f,envelopeRelease=.999f;
    float solo=0,hpG=.006f,targetHpG=.006f;std::array<float,4> hpS{};
    static bool isPrime(int n){if(n<2)return false;for(int d=2;d*d<=n;++d)if(n%d==0)return false;return true;}
    void update(const Parameters& p,bool initialise){
        target=reflections(p);targetPredelay=std::round(p.predelay*.001f*float(sr));
        for(auto& tap:target)tap.seconds=std::round(tap.seconds*float(sr))/float(sr);   // exact samples: no treble loss
        targetHpG=std::tan(float(pi)*std::min(p.lowCut,float(sr*.2))/float(sr));
        targetDamping=1-std::exp(-2*float(pi)*std::min(p.damping,float(sr*.4))/float(sr));
        float scale=std::clamp(std::cbrt(p.width*p.depth*p.height/144),.55f,1.8f);
        constexpr float ms[8]={29.3f,37.1f,43.7f,53.3f,61.7f,71.9f,79.7f,89.3f};
        for(int i=0;i<8;++i){int len=std::max(8,int(std::round(ms[i]*.001f*float(sr)*scale)));
            for(;;++len){bool used=false;for(int j=0;j<i;++j)if(int(targetDelays[j])==len)used=true;if(isPrime(len)&&!used)break;}
            targetDelays[i]=float(len);
            targetFeedback[i]=std::pow(10.f,-3*targetDelays[i]/float(sr)/p.decay);
            targetInjection[i]=std::sqrt(std::max(0.f,1-targetFeedback[i]*targetFeedback[i]));}
        Vec s{p.sx*p.width,p.sy*p.depth,p.sz},l{p.lx*p.width,p.ly*p.depth,p.lz};
        targetDistance=std::clamp(.7f+length(l-s)*.06f,.7f,1.6f);targetGain=gain(p.output);
        if(initialise){hpG=targetHpG;hpS.fill(0);taps=target;delays=targetDelays;feedback=targetFeedback;injection=targetInjection;predelay=targetPredelay;dampingAlpha=targetDamping;}
    }
};
}

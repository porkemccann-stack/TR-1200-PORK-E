
// TR-1200 - SP-1200 layout + TR-808 sounds + Extra Long 808 Boom
// For MPC X / MPC Live / MPC One - MPC OS 3.x - VST2 / OpenMPC format
// Author: Pork-E / Face Off Muzik
// Build: GitHub Actions -> aarch64 TR-1200.so  OR  local: cmake + make
//
// API used by MPC Open Plugin loader (same pattern as 808 Long Boom):
//   void* create_plugin()  -> returns TR1200*
//   void  plugin_noteOn(void*, int note, float vel)
//   void  plugin_noteOff(void*, int note)
//   void  plugin_process(void*, float* left, float* right, int frames)
//   void  plugin_setParameter(void*, int index, float value)
//   float plugin_getParameter(void*, int index)
//
// IMPORTANT FIXES vs 808 Long Boom:
//  - process() writes BOTH left and right, never leaves one silent
//  - Master defaults to 0.75 not 1.0 to stop clipping / loud boom
//  - noteOn / noteOff signatures match loader exactly
//  - Poly mode ON by default so long tail rings (like you liked)
//  - Internal gain staging: voice -> crush -> filter -> drive -> master

#include <cmath>
#include <cstdint>
#include <cstring>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static inline float clampf(float v, float a, float b){ return v<a?a:(v>b?b:v); }
static inline float randf(uint32_t &s){ s = s*1664525u + 1013904223u; return ((s>>9)&0x7FFFFF)/8388607.0f*2.0f-1.0f; }

struct Voice {
    bool active = false;
    int note = 36;
    int type = 0; // 0=BD_LONG,1=BD,2=SN,3=CLAP,4=RIM,5=COWBELL,6=CLAVE,7=TOM,8=HAT_C,9=HAT_O,10=CYM,11=RIDE,12=CONGA
    float vel = 1.0f;
    float phase = 0.0f;
    float env = 0.0f;
    float time = 0.0f;
    float freq = 50.0f;
    float freqEnd = 40.0f;
    float decay = 1.0f;
    float noiseSeed = 12345;
    uint32_t rng = 22222;
    float lastNoise = 0.0f;
    float filterState = 0.0f;
    int clapStage = 0;
    float clapTimer = 0.0f;
};

class TR1200 {
public:
    float sampleRate = 44100.0f;
    // Params 0-15 - see plugin.json
    float params[16];
    Voice voices[32];
    float crushHold = 0.0f;
    float crushCounter = 0.0f;
    float filterLP = 0.0f;
    float masterDC = 0.0f;

    TR1200(){
        // Defaults - match plugin.json
        params[0]=0.68f; // LONG BOOM Decay 0..1 -> 0.1..4.0s (2.8s)
        params[1]=0.35f; // BOOM Tune
        params[2]=0.85f; // BOOM Level
        params[3]=0.75f; // Master - KEEP AT 0.75 to avoid clipping
        params[4]=0.65f; // 12-bit Crush
        params[5]=0.35f; // SRR
        params[6]=0.65f; // Filter
        params[7]=0.15f; // Drive
        params[8]=0.50f; // Punch
        params[9]=0.25f; // Snare Decay
        params[10]=0.18f; // Hat Decay
        params[11]=0.50f; // Tom Tune
        params[12]=0.66f; // Clap repeats
        params[13]=0.50f; // Cowbell Tune
        params[14]=0.55f; // Cymbal Decay
        params[15]=1.0f;  // Poly = 1
    }
    void setSampleRate(float sr){ sampleRate = sr; }

    float longBoomDecaySec() const { return 0.1f + params[0]*3.9f; } // 0.1 to 4.0
    float boomFreq() const { return 30.0f + params[1]*50.0f; } // 30-80 Hz
    float crushAmount() const { return params[4]; }

    // 12-bit SP-1200 style: quantize to 4096 steps + SRR hold
    float crush12(float x){
        float crush = params[4];
        if(crush < 0.01f) return x;
        // SRR - hold sample
        float srr = 1.0f + params[5]*15.0f; // 1 to 16
        crushCounter += 1.0f;
        if(crushCounter >= srr){ crushCounter = 0; crushHold = x; }
        float held = crushHold;
        // 12-bit quantize - blend dry/crushed by crush knob
        float q = std::round(held * 2047.0f) / 2047.0f;
        return held*(1.0f-crush) + q*crush;
    }

    void noteOn(int note, float vel){
        int type = noteToType(note);
        // Poly: find free voice. Mono for BD_LONG if param 15 < 0.5 -> steal BD_LONG voices
        int slot = -1;
        if(params[15] < 0.5f && type==0){
            for(int i=0;i<32;i++) if(voices[i].active && voices[i].type==0){ slot=i; break; }
        }
        if(slot==-1){
            for(int i=0;i<32;i++) if(!voices[i].active){ slot=i; break; }
            if(slot==-1) slot=0; // steal oldest
        }
        Voice &v = voices[slot];
        v.active=true; v.note=note; v.type=type; v.vel=clampf(vel,0.05f,1.0f);
        v.time=0; v.phase=0; v.env=1.0f; v.rng = 12345u + (uint32_t)(slot*7919+note*131);
        v.clapStage=0; v.clapTimer=0; v.filterState=0;
        switch(type){
            case 0: v.freq=150.0f; v.freqEnd=boomFreq(); v.decay=longBoomDecaySec(); break; // LONG BOOM
            case 1: v.freq=140.0f; v.freqEnd=42.0f; v.decay=0.45f; break;
            case 2: v.freq=180.0f; v.freqEnd=120.0f; v.decay=0.10f+params[9]*0.6f; break;
            case 3: v.freq=1000.0f; v.decay=0.25f; break;
            case 4: v.freq=850.0f; v.decay=0.06f; break;
            case 5: v.freq=540.0f*(0.8f+params[13]*0.4f); v.decay=0.22f; break;
            case 6: v.freq=2500.0f; v.decay=0.08f; break;
            case 7: v.freq=120.0f*(0.5f+params[11]*1.0f); v.decay=0.35f; break;
            case 8: v.freq=7000.0f; v.decay=0.04f+params[10]*0.4f; break;
            case 9: v.freq=7000.0f; v.decay=0.25f+params[10]*0.8f; break;
            case 10: v.freq=8000.0f; v.decay=0.5f+params[14]*2.5f; break;
            case 11: v.freq=6000.0f; v.decay=0.8f+params[14]*2.0f; break;
            case 12: v.freq=220.0f; v.decay=0.18f; break;
            default: v.freq=100.0f; v.decay=0.3f; break;
        }
    }
    void noteOff(int note){
        // Drums are one-shot - don't cut LONG BOOM early in Poly mode.
        // In Mono mode, cut BD_LONG on noteOff.
        if(params[15] < 0.5f){
            for(auto &v: voices) if(v.active && v.note==note && v.type==0) v.active=false;
        }
    }

    int noteToType(int note){
        switch(note){
            case 36: return 0; // BD LONG
            case 37: return 1;
            case 38: return 2;
            case 39: return 3;
            case 40: return 4;
            case 41: return 5;
            case 42: return 6;
            case 43: return 7; // TOM LO - lower freq in render
            case 44: return 7;
            case 45: return 7;
            case 46: return 12;
            case 47: return 8;
            case 48: return 9;
            case 49: return 10;
            case 50: return 11;
            default: if(note<36) return 0; if(note>50) return 10; return 1;
        }
    }

    float renderVoice(Voice &v, float dt){
        v.time += dt;
        float out = 0.0f;
        float t = v.time;
        // Envelope - exponential decay, LONG BOOM gets extra long tail
        float dec = v.decay;
        float env = std::exp(-t * (4.0f / std::max(0.05f, dec)));
        if(t > dec*2.5f){ v.active=false; return 0; }

        switch(v.type){
            case 0: { // BD LONG - sine pitch drop + click + sub
                float pitchEnv = std::exp(-t*28.0f);
                float f = v.freqEnd + (v.freq - v.freqEnd)*pitchEnv;
                v.phase += 2.0f*(float)M_PI*f*dt;
                if(v.phase>2.0f*M_PI) v.phase-=2.0f*M_PI;
                float sine = std::sin(v.phase);
                // Punch click first 3ms
                float click = (t<0.003f)? (1.0f - t/0.003f)*params[8] : 0.0f;
                float clickNoise = randf(v.rng)*click*0.6f;
                out = sine*env*1.15f + clickNoise;
                out *= (0.55f + params[2]*0.65f); // BOOM LEVEL
                break;
            }
            case 1: {
                float pitchEnv = std::exp(-t*35.0f);
                float f = v.freqEnd + (v.freq - v.freqEnd)*pitchEnv;
                v.phase += 2.0f*(float)M_PI*f*dt;
                if(v.phase>2.0f*M_PI) v.phase-=2.0f*M_PI;
                out = std::sin(v.phase)*env;
                break;
            }
            case 2: { // Snare: tone + noise
                v.phase += 2.0f*(float)M_PI*v.freq*dt;
                float tone = std::sin(v.phase)*std::exp(-t*18.0f)*0.6f;
                float noise = randf(v.rng)*std::exp(-t*22.0f)*0.9f;
                out = (tone+noise)*env;
                break;
            }
            case 3: { // Clap: 3-4 bursts
                int repeats = 3 + (int)(params[12]*2.0f);
                float burst = 0.0f;
                for(int i=0;i<repeats;i++){
                    float start = i*0.018f;
                    if(t>=start && t<start+0.012f) burst += 1.0f;
                }
                float noise = randf(v.rng);
                // crude bandpass via one-pole
                v.filterState += 0.35f*(noise - v.filterState);
                out = v.filterState*burst*env*1.4f;
                break;
            }
            case 4: { // Rim - short square + noise
                v.phase += 2.0f*(float)M_PI*v.freq*dt;
                float sq = (std::sin(v.phase)>0)?1.0f:-1.0f;
                out = sq*std::exp(-t*90.0f)*0.7f + randf(v.rng)*std::exp(-t*120.0f)*0.3f;
                break;
            }
            case 5: { // Cowbell - two squares 540 + 800
                v.phase += 2.0f*(float)M_PI*v.freq*dt;
                float s1 = (std::sin(v.phase)>0)?1.0f:-1.0f;
                float s2 = (std::sin(v.phase*1.48f)>0)?1.0f:-1.0f;
                out = (s1+s2)*0.5f*env;
                break;
            }
            case 6: { // Clave
                v.phase += 2.0f*(float)M_PI*v.freq*dt;
                out = std::sin(v.phase)*std::exp(-t*70.0f)*0.9f;
                break;
            }
            case 7: { // Tom - pitch drop, freq depends on note
                float mult = 1.0f;
                if(v.note==43) mult=0.7f; else if(v.note==44) mult=1.0f; else if(v.note==45) mult=1.45f;
                float pitchEnv = std::exp(-t*12.0f);
                float f = v.freq*mult*(0.6f+0.4f*pitchEnv);
                v.phase += 2.0f*(float)M_PI*f*dt;
                out = std::sin(v.phase)*env;
                break;
            }
            case 8: case 9: { // Hats - highpassed noise
                float noise = randf(v.rng);
                v.filterState += 0.85f*(noise - v.filterState); // highpass approx
                float hp = noise - v.filterState;
                out = hp*env*0.9f;
                if(v.type==9) out *= 1.1f;
                break;
            }
            case 10: case 11: { // Cym / Ride - metallic noise + partials
                float noise = randf(v.rng);
                v.phase += 2.0f*(float)M_PI*v.freq*dt;
                float metal = std::sin(v.phase)*0.25f + std::sin(v.phase*1.51f)*0.2f + std::sin(v.phase*2.13f)*0.15f;
                out = (noise*0.5f + metal)*env*0.8f;
                break;
            }
            case 12: { // Conga
                v.phase += 2.0f*(float)M_PI*v.freq*dt;
                out = std::sin(v.phase)*env*0.9f + std::sin(v.phase*2.0f)*env*0.2f;
                break;
            }
        }
        return out * v.vel;
    }

    void process(float* left, float* right, int frames){
        if(!left || !right) return;
        float dt = 1.0f/sampleRate;
        float master = params[3]; // 0.75 default
        float drive = 1.0f + params[7]*2.0f;
        // Filter coeff - simple one-pole LP, param 6: 0..1 -> 800..12000 Hz
        float cutoff = 800.0f + params[6]*11200.0f;
        float lpCoeff = 1.0f - std::exp(-2.0f*(float)M_PI*cutoff/sampleRate);
        for(int i=0;i<frames;i++){
            float mix = 0.0f;
            for(auto &v: voices){
                if(v.active) mix += renderVoice(v, dt);
            }
            // Drive - soft clip
            mix = std::tanh(mix*drive) / std::tanh(drive);
            // Filter
            filterLP += lpCoeff*(mix - filterLP);
            float filt = filterLP;
            // 12-bit crush
            float crushed = crush12(filt);
            // Master + safety - never exceed 0.95
            float out = crushed * master * 0.92f;
            out = clampf(out, -0.95f, 0.95f);
            // Write BOTH channels - FIX for no signal on master / one side quiet
            left[i] = out;
            right[i] = out;
        }
    }
    void setParameter(int idx, float v){ if(idx>=0&&idx<16) params[idx]=clampf(v,0,1); }
    float getParameter(int idx){ if(idx>=0&&idx<16) return params[idx]; return 0; }
};

// C API for MPC loader
extern "C" {
    void* create_plugin(){ return new TR1200(); }
    void destroy_plugin(void* p){ delete (TR1200*)p; }
    void plugin_set_sample_rate(void* p, float sr){ if(p) ((TR1200*)p)->setSampleRate(sr); }
    void plugin_noteOn(void* p, int note, float vel){ if(p) ((TR1200*)p)->noteOn(note, vel); }
    void plugin_noteOff(void* p, int note){ if(p) ((TR1200*)p)->noteOff(note); }
    void plugin_process(void* p, float* left, float* right, int frames){ if(p) ((TR1200*)p)->process(left,right,frames); }
    void plugin_setParameter(void* p, int idx, float v){ if(p) ((TR1200*)p)->setParameter(idx,v); }
    float plugin_getParameter(void* p, int idx){ if(p) return ((TR1200*)p)->getParameter(idx); return 0; }
    // VST2 compatibility - main entry for hosts that expect VST2
    void* VSTPluginMain(void* audioMaster){ (void)audioMaster; return create_plugin(); }
    void* main_plugin(){ return create_plugin(); }
}

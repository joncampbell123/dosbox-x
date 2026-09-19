#ifndef H_MIXER_H
#define H_MIXER_H
#include "dosbox.h"
#include <vector>
typedef void (*MIXER_Handler)(Bitu len);
class MixerChannel { public:
  Bitu freq = 0; bool on = false;
  std::vector<int16_t> got;      // every sample handed to the mixer, in order
  Bitu silence_calls = 0;
  void SetFreq(Bitu f,Bitu=1){ freq=f; }
  void FillUp(void){}
  void Enable(bool y){ on=y; }
  void AddSilence(void){ silence_calls++; }
  void AddSamples_m8 (Bitu n,const uint8_t* d){ for(Bitu i=0;i<n;i++) got.push_back((int16_t)((int)d[i]-128)<<8); }
  void AddSamples_s8 (Bitu n,const uint8_t* d){ for(Bitu i=0;i<2*n;i++) got.push_back((int16_t)((int)d[i]-128)<<8); }
  void AddSamples_m16(Bitu n,const int16_t* d){ for(Bitu i=0;i<n;i++)   got.push_back(d[i]); }
  void AddSamples_s16(Bitu n,const int16_t* d){ for(Bitu i=0;i<2*n;i++) got.push_back(d[i]); }
};
MixerChannel* MIXER_AddChannel(MIXER_Handler,Bitu,const char*);
extern MIXER_Handler TheMixerHandler;
#endif

#pragma once
#include "thru_fm_parameters.h"
namespace s3g_thru_fm {
inline constexpr const char* startNames[]{"KEEP","BASS","BELL","KEYS","PAD","KICK","SNARE","TOM","METAL","HAT"};
inline Config startingSound(int which,const Config& previous){
 Config c;auto& p=c.v;p[level]=previous.v[level];p[width]=previous.v[width];
 p[attack1]=p[attack2]=0;p[end1]=p[end2]=0;
 switch(which){
 case 1:p[tune]=-12;p[index1]=3;p[feedback]=15;p[harm]=-25;p[cutoff]=6000;p[decay1]=350;p[end1]=35;break;
 case 2:p[mode]=1;p[algorithm]=1;p[ratio1]=2;p[ratio2]=3.5f;p[index1]=4;p[index2]=3;p[bend]=0;p[decay]=3000;p[decay1]=2200;p[decay2]=1000;p[harm]=15;p[impact]=10;break;
 case 3:p[algorithm]=2;p[ratio1]=1;p[ratio2]=7;p[index1]=2.3f;p[index2]=1.2f;p[decay1]=800;p[end1]=15;p[decay2]=120;p[harm]=-10;break;
 case 4:p[algorithm]=1;p[ratio2]=.5f;p[index1]=2;p[index2]=1;p[attack1]=800;p[attack2]=1500;p[decay1]=1200;p[decay2]=1800;p[end1]=80;p[end2]=50;p[detune]=9;p[harm]=20;break;
 case 5:p[mode]=1;p[tune]=-24;p[index1]=1.5f;p[index2]=0;p[bend]=36;p[sweep_time]=22;p[decay]=350;p[decay1]=45;p[decay2]=25;p[impact]=20;p[hardness]=85;p[drive]=15;break;
 case 6:p[mode]=1;p[tune]=-12;p[algorithm]=2;p[ratio1]=1.5f;p[ratio2]=2.75f;p[index1]=4;p[index2]=2;p[bend]=12;p[sweep_time]=8;p[decay]=180;p[decay1]=70;p[decay2]=45;p[noise]=75;p[noise_decay]=160;p[impact]=35;p[hardness]=75;p[harm]=20;break;
 case 7:p[mode]=1;p[tune]=-12;p[ratio1]=1.5f;p[index1]=2.5f;p[bend]=12;p[sweep_time]=40;p[decay]=600;p[decay1]=120;p[impact]=15;p[noise]=5;p[noise_decay]=50;p[harm]=-15;break;
 case 8:p[mode]=1;p[algorithm]=1;p[ratio1]=1.4142f;p[ratio2]=3.17f;p[index1]=5;p[index2]=3;p[feedback]=30;p[bend]=0;p[decay]=900;p[decay1]=500;p[end1]=20;p[decay2]=80;p[noise]=20;p[noise_decay]=500;p[impact]=40;p[hardness]=65;p[harm]=35;break;
 case 9:p[mode]=1;p[tune]=12;p[algorithm]=2;p[ratio1]=6;p[ratio2]=9.5f;p[index1]=4;p[index2]=3;p[bend]=0;p[decay]=70;p[decay1]=30;p[decay2]=25;p[noise]=90;p[noise_decay]=55;p[impact]=30;p[harm]=50;p[cutoff]=17000;p[drive]=10;break;
 default:return previous;
 }
 return c;
}
}

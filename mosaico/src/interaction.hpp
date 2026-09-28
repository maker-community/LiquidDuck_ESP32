#pragma once
#include <stdint.h>
// Approximate exp(-rate * dt) without a libm import; accurate for these short steps.
inline float timeDecay(float rate,float dt){float x=rate*dt;return 1.0f/(1+x+x*x*.5f+x*x*x/6+x*x*x*x/24);}
inline float frameSeconds(uint64_t elapsed){return elapsed>100000?1.f/30.f:(elapsed>50000?.05f:float(elapsed)*.000001f);}
struct TouchGesture {
 int action=-1;uint32_t contact=0;
 static int region(int x,int y){return x<0||x>=240||y<0||y>=240?-1:(y<210?3:x/80);}
 void down(uint32_t id,int x,int y){if(action<0){contact=id;action=region(x,y);}}
 void cancel(uint32_t id){if(id==contact)action=-1;}
 void move(uint32_t id,int x,int y){if(id==contact&&region(x,y)!=action)action=-1;}
 int up(uint32_t id,int x,int y){if(id!=contact)return -1;int result=region(x,y)==action?action:-1;action=-1;return result;}
};

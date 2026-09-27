#include <cassert>
#include <initializer_list>
#include "interaction.hpp"
int main(){
 float a=1,b=1;for(int i=0;i<30;++i)a*=timeDecay(.606081f,1.f/30);for(int i=0;i<60;++i)b*=timeDecay(.606081f,1.f/60);assert(a-b<.00001f&&b-a<.00001f);
 assert(frameSeconds(5000000)==1.f/30.f);assert(frameSeconds(80000)==.05f);
 TouchGesture g;g.down(1,20,220);assert(g.up(2,20,220)==-1);assert(g.up(1,20,220)==0);
 g.down(1,20,220);g.move(1,100,220);assert(g.up(1,20,220)==-1);
 g.down(1,40,70);assert(g.up(1,60,80)==3);
 g.down(1,-1,30);assert(g.action==-1);g.down(1,240,220);assert(g.action==-1);
 g.down(1,170,220);g.cancel(1);assert(g.up(1,170,220)==-1);
}

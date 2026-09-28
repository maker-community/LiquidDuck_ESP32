#pragma once
#include "sdk/micropixel.hpp"
struct Perf {
 uint64_t start=0,physics=0,render=0,submit=0;unsigned frames=0;
 static char* number(char*p,uint64_t n){char tmp[24];int i=0;do{tmp[i++]=char('0'+n%10);n/=10;}while(n);while(i)*p++=tmp[--i];return p;}
 static char* text(char*p,const char*s){while(*s)*p++=*s++;return p;}
 void frame(micropixel::Application&app,uint64_t a,uint64_t b,uint64_t c,uint64_t d,int mode){if(!start)start=a;physics+=b-a;render+=c-b;submit+=d-c;++frames;if(d-start<5000000)return;char line[200];char*p=line;p=text(p,"PERF mode=");p=number(p,mode);p=text(p," fps10=");p=number(p,frames*10000000ULL/(d-start));p=text(p," physics_us=");p=number(p,physics/frames);p=text(p," render_us=");p=number(p,render/frames);p=text(p," submit_us=");p=number(p,submit/frames);*p=0;app.log().Info(line);start=d;frames=0;physics=render=submit=0;}
};

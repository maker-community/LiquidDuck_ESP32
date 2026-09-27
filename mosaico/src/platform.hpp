#pragma once
#include "port_memory.hpp"
#include "sdk/math.hpp"
#include "sdk/micropixel.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>
using std::min;
using std::max;
static float tiltX=0,tiltY=1,tiltZ=0;
static uint32_t rngState=0x8e125ab3;
static int random(int bound) { rngState^=rngState<<13; rngState^=rngState>>17; rngState^=rngState<<5; return bound>0?int(rngState%uint32_t(bound)):0; }
constexpr uint16_t TFT_WHITE=0xffff;
class Canvas {
 int w_=0,h_=0,px_=0,py_=0;
 bool dirty_=true,cached_=false,rowMajor_=false;uint8_t slot_=0;
 public:
 micropixel::RasterDrawList* draw=nullptr;
 std::vector<uint16_t> pixels;
 int width()const{return w_;} int height()const{return h_;}
 void createSprite(int w,int h){w_=w;h_=h;pixels.resize(w*h);dirty_=true;}
 void deleteSprite(){pixels.clear();w_=h_=0;dirty_=true;cached_=false;}
 void setPivot(int x,int y){px_=x;py_=y;}
 void fillSprite(uint16_t c){if(draw){(void)draw->FillRect({0,0,w_,h_},micropixel::Color::FromRgb565(c));return;}std::fill(pixels.begin(),pixels.end(),c);dirty_=true;}
 void drawPixel(int x,int y,uint16_t c){if(x>=0&&y>=0&&x<w_&&y<h_){if(draw)(void)draw->FillRect({x,y,1,1},micropixel::Color::FromRgb565(c));else{pixels[y*w_+x]=c;dirty_=true;}}}
 void fillRect(int x,int y,int w,int h,uint16_t c){int l=max(0,x),t=max(0,y),r=min(w_,x+w),b=min(h_,y+h);if(l>=r||t>=b)return;if(draw){(void)draw->FillRect({l,t,r-l,b-t},micropixel::Color::FromRgb565(c));return;}for(int yy=t;yy<b;++yy)std::fill(pixels.begin()+yy*w_+l,pixels.begin()+yy*w_+r,c);dirty_=true;}
 void fillCircle(int x,int y,int r,uint16_t c){for(int dy=-r;dy<=r;++dy){int dx=int(micropixel::math::Sqrt(float(r*r-dy*dy)));fillRect(x-dx,y+dy,dx*2+1,1,c);}}
 void fillRoundRect(int x,int y,int w,int h,int r,uint16_t c){r=min(r,min(w,h)/2);fillRect(x+r,y,w-2*r,h,c);fillRect(x,y+r,w,h-2*r,c);fillCircle(x+r,y+r,r,c);fillCircle(x+w-r-1,y+r,r,c);fillCircle(x+r,y+h-r-1,r,c);fillCircle(x+w-r-1,y+h-r-1,r,c);}
 // Small ball textures have only a handful of colours. Upload once per theme.
 void prepare(const micropixel::RasterResources& resources,uint8_t slot,uint16_t transparent,bool rowMajor=false){
  if(!dirty_&&slot_==slot&&rowMajor_==rowMajor)return;
  slot_=slot;rowMajor_=rowMajor;dirty_=false;
  int tw=w_,th=h_;if(rowMajor){tw=th=1;while(tw<w_)tw*=2;while(th<h_)th*=2;}
  uint16_t palette[256]{};unsigned count=1;std::vector<uint8_t> indices(tw*th);
  for(int x=0;x<w_;++x)for(int y=0;y<h_;++y){uint16_t c=pixels[y*w_+x];unsigned i=0;if(c!=transparent){for(i=1;i<count;++i)if(palette[i]==c)break;if(i==count){if(count==256){cached_=false;return;}palette[count++]=c;}}indices[rowMajor?y*tw+x:x*h_+y]=uint8_t(i);}
  resources.UploadLitPalette(slot,1,palette).value();resources.UploadTexture(slot,tw,th,rowMajor?micropixel::RasterLayout::kRowMajor:micropixel::RasterLayout::kColumnMajor,indices).value();slot_=slot;dirty_=false;cached_=true;
 }
 void pushSprite(Canvas* dst,int x,int y,uint16_t transparent){
  if(dst->draw&&cached_&&!dirty_){dst->draw->SetPalette(slot_);(void)dst->draw->Sprite({x,y,w_,h_},slot_,0,0,0,w_,h_,true);return;}
  for(int yy=0;yy<h_;++yy)for(int xx=0;xx<w_;++xx){auto c=pixels[yy*w_+xx];if(c!=transparent)dst->drawPixel(x+xx,y+yy,c);}
 }
 void pushRotateZoom(Canvas* dst,float cx,float cy,float degrees,float sx,float sy,uint16_t transparent){
  float a=degrees*0.01745329252f,co=micropixel::math::Cos(a),si=micropixel::math::Sin(a);
  if(dst->draw&&cached_&&!dirty_&&rowMajor_){
   micropixel::RasterVertex corners[4];float us[4]={0,float(w_),float(w_),0},vs[4]={0,0,float(h_),float(h_)};
   for(int i=0;i<4;++i){float x=(us[i]-px_)*sx,y=(vs[i]-py_)*sy;corners[i]=micropixel::RasterVertex::At(cx+co*x-si*y,cy+si*x+co*y,us[i],vs[i],0);}
   dst->draw->SetPalette(slot_);(void)dst->draw->Quad(corners,slot_,true);return;
  }
  int rx=int(__builtin_ceilf(micropixel::math::Abs(co)*w_*sx*0.5f+micropixel::math::Abs(si)*h_*sy*0.5f))+1;
  int ry=int(__builtin_ceilf(micropixel::math::Abs(si)*w_*sx*0.5f+micropixel::math::Abs(co)*h_*sy*0.5f))+1;
  int originX=int(cx),originY=int(cy);
  for(int y=max(-ry,-originY);y<=min(ry,dst->height()-1-originY);++y){
   int first=max(-rx,-originX),last=min(rx,dst->width()-1-originX);int run=first;uint16_t previous=transparent;
   for(int x=first;x<=last+1;++x){uint16_t c=transparent;if(x<=last){int u=int(micropixel::math::Floor((co*x+si*y)/sx+px_)),v=int(micropixel::math::Floor((-si*x+co*y)/sy+py_));if(u>=0&&v>=0&&u<w_&&v<h_)c=pixels[v*w_+u];}
    if(c!=previous){if(previous!=transparent)dst->fillRect(originX+run,originY+y,x-run,1,previous);run=x;previous=c;}
   }
  }
 }
};

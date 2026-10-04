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
// True only when the Host has the polygon rotation path. Otherwise the duck is
// drawn upright: a software rotation of a large sprite emits thousands of
// raster records per frame and starves the USB task.
static bool g_duckRotate=true;
static uint32_t rngState=0x8e125ab3;
static int random(int bound) { rngState^=rngState<<13; rngState^=rngState>>17; rngState^=rngState<<5; return bound>0?int(rngState%uint32_t(bound)):0; }
constexpr uint16_t TFT_WHITE=0xffff;
class Canvas {
 int w_=0,h_=0,px_=0,py_=0,outputW_=0,outputH_=0,offsetY_=0;
 bool dirty_=true,cached_=false,rowMajor_=false;uint8_t slot_=0;
 int outputX(int value) const { return draw ? value*outputW_/240 : value; }
 // The fluid canvas is 240x210 but the panel is square, so the scene sat at the
 // top with an empty band below. Shift it down by half the leftover to centre
 // it. A pure translation keeps every coordinate inside the buffer - stretching
 // the axis instead pushed rows past the buffer edge and the Host watchdog
 // reset the device.
 int outputY(int value) const { return draw ? (value+offsetY_)*outputH_/240 : value; }
 int outputWidth(int value) const { return draw ? max(1,value*outputW_/240) : value; }
 int outputHeight(int value) const { return draw ? max(1,value*outputH_/240) : value; }
 public:
 micropixel::RasterDrawList* draw=nullptr;
 std::vector<uint16_t> pixels;
 int width()const{return w_;} int height()const{return h_;}
 // Deliberately does not touch outputW_/outputH_: those describe the Host
 // buffer (set by setOutputSize) and must not be overwritten by the sprite's
 // own size, or every coordinate would be drawn 1:1 and clipped.
 void createSprite(int w,int h){w_=w;h_=h;offsetY_=(240-h)/2;pixels.resize(w*h);dirty_=true;}
 void setOutputSize(int width,int height){outputW_=width;outputH_=height;}
 void deleteSprite(){pixels.clear();w_=h_=0;dirty_=true;cached_=false;}
 void setPivot(int x,int y){px_=x;py_=y;}
 void fillSprite(uint16_t c){if(draw){if(outputW_>0&&outputH_>0)(void)draw->FillRect({0,0,outputW_,outputH_},micropixel::Color::FromRgb565(c));return;}std::fill(pixels.begin(),pixels.end(),c);dirty_=true;}
 void drawPixel(int x,int y,uint16_t c){if(x>=0&&y>=0&&x<w_&&y<h_){if(draw)(void)draw->FillRect({outputX(x),outputY(y),1,1},micropixel::Color::FromRgb565(c));else{pixels[y*w_+x]=c;dirty_=true;}}}
 void fillRect(int x,int y,int w,int h,uint16_t c){int l=max(0,x),t=max(0,y),r=min(w_,x+w),b=min(h_,y+h);if(l>=r||t>=b)return;if(draw){int ox=outputX(l),oy=outputY(t),ow=outputX(r)-ox,oh=outputY(b)-oy;if(ow>0&&oh>0)(void)draw->FillRect({ox,oy,ow,oh},micropixel::Color::FromRgb565(c));return;}for(int yy=t;yy<b;++yy)std::fill(pixels.begin()+yy*w_+l,pixels.begin()+yy*w_+r,c);dirty_=true;}
 void fillCircle(int x,int y,int r,uint16_t c){for(int dy=-r;dy<=r;++dy){int dx=int(micropixel::math::Sqrt(float(r*r-dy*dy)));fillRect(x-dx,y+dy,dx*2+1,1,c);}}
 void fillRoundRect(int x,int y,int w,int h,int r,uint16_t c){r=min(r,min(w,h)/2);fillRect(x+r,y,w-2*r,h,c);fillRect(x,y+r,w,h-2*r,c);fillCircle(x+r,y+r,r,c);fillCircle(x+w-r-1,y+r,r,c);fillCircle(x+r,y+h-r-1,r,c);fillCircle(x+w-r-1,y+h-r-1,r,c);}
 // Small ball textures have only a handful of colours. Upload once per theme.
 void prepare(const micropixel::RasterResources& resources,uint8_t slot,uint16_t transparent,bool rowMajor=false){
  if(!dirty_&&slot_==slot&&rowMajor_==rowMajor)return;
  if(w_<=0||h_<=0||!resources.valid()){cached_=false;return;}
  slot_=slot;rowMajor_=rowMajor;
  int tw=w_,th=h_;if(rowMajor){tw=th=1;while(tw<w_)tw*=2;while(th<h_)th*=2;}
  uint16_t palette[256]{};unsigned count=1;std::vector<uint8_t> indices(tw*th);
  // Pixel art here has up to ~380 colours, more than the 256-entry INDEX8
  // palette. Falling back to per-pixel drawing would emit ~1000 raster records
  // per frame and starve the Host, so instead drop colour precision until the
  // sprite fits: full RGB565, then 3 bits/channel, then 2 bits/channel.
  static const uint16_t kMasks[3]={0xFFFFu,0xE71Cu,0xC618u};
  bool built=false;
  for(int pass=0;pass<3&&!built;++pass){
   const uint16_t mask=kMasks[pass];
   count=1;std::fill(indices.begin(),indices.end(),uint8_t(0));
   bool fits=true;
   for(int x=0;x<w_&&fits;++x)for(int y=0;y<h_;++y){
    uint16_t c=pixels[y*w_+x];unsigned i=0;
    if(c!=transparent){
     c=uint16_t(c&mask);
     for(i=1;i<count;++i)if(palette[i]==c)break;
     if(i==count){if(count==255){fits=false;break;}palette[count++]=c;}
    }
    indices[rowMajor?y*tw+x:x*h_+y]=uint8_t(i);
   }
   built=fits;
  }
  if(!built){cached_=false;return;}
  if(resources.UploadLitPalette(slot,1,palette)&&resources.UploadTexture(slot,tw,th,rowMajor?micropixel::RasterLayout::kRowMajor:micropixel::RasterLayout::kColumnMajor,indices)){slot_=slot;dirty_=false;cached_=true;}else{cached_=false;}
 }
 void pushSprite(Canvas* dst,int x,int y,uint16_t transparent){
    if(dst->draw&&cached_&&!dirty_){dst->draw->SetPalette(slot_);(void)dst->draw->Sprite({dst->outputX(x),dst->outputY(y),dst->outputWidth(w_),dst->outputHeight(h_)},slot_,0,0,0,w_,h_,true);return;}
  for(int yy=0;yy<h_;++yy)for(int xx=0;xx<w_;++xx){auto c=pixels[yy*w_+xx];if(c!=transparent)dst->drawPixel(x+xx,y+yy,c);}
 }
 void pushRotateZoom(Canvas* dst,float cx,float cy,float degrees,float sx,float sy,uint16_t transparent){
  float a=degrees*0.01745329252f,co=micropixel::math::Cos(a),si=micropixel::math::Sin(a);
  if(dst->draw&&cached_&&!dirty_&&rowMajor_){
   micropixel::RasterVertex corners[4];float us[4]={0,float(w_),float(w_),0},vs[4]={0,0,float(h_),float(h_)};
    for(int i=0;i<4;++i){float x=(us[i]-px_)*sx,y=(vs[i]-py_)*sy;float px=cx+co*x-si*y,py=cy+si*x+co*y;corners[i]=micropixel::RasterVertex::At(dst->outputX(int(px)),dst->outputY(int(py)),us[i],vs[i],0);}
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

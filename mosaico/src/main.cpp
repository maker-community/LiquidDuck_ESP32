// MicroPixel port of nongxl/LiquidDuck_ESP32, branch StickS3.
// Upstream declares PolyForm Noncommercial 1.0.0; retain attribution.
#include "liquidduck.cpp"
#include "perf.hpp"
#include "interaction.hpp"
using namespace micropixel;
using namespace micropixel::literals;

static void changeMode() {
 currentMode=PhysicsMode((int(currentMode)+1)%4);
 if(fluid){flip_destroy(fluid);fluid=nullptr;}
 initFluidForMode(currentMode);
 if(currentMode==MODE_FLIP)initParticlesFLIP();
 frameEnergy=0;
}
static void changeTheme(){if(currentMode==MODE_FLIP)switchTheme();else currentPixelPaletteIndex=(currentPixelPaletteIndex+1)%PIXEL_PALETTE_COUNT;}
static void advance(float dt){
 float gx=micropixel::math::Abs(tiltX)<0.08f?0:tiltX,gy=micropixel::math::Abs(tiltY)<0.08f?0:tiltY;
 imuGX+=(gx*GRAVITY_STRENGTH-imuGX)*(1-timeDecay(12.92349f,dt));imuGY+=(gy*GRAVITY_STRENGTH-imuGY)*(1-timeDecay(12.92349f,dt));
 float target=micropixel::math::Atan2(-imuGX,imuGY+0.0001f)*57.2957795f;
 // Small cheap angular substeps keep the spring stable across frame rates.
 int angularSteps=int(dt*120)+1;float angularDt=dt/angularSteps;
 for(int i=0;i<angularSteps;++i){
  float error=target-duck.angle;while(error>180)error-=360;while(error< -180)error+=360;
  duck.aVel=(duck.aVel+error*DUCK_ROT_STIFFNESS*angularDt*30)*timeDecay(11.569875f,angularDt);
  duck.angle+=duck.aVel*angularDt*30;
 }
 updateClouds(dt);frameEnergy=0;
 if(currentMode==MODE_FLIP){physicsStepFLIP(dt*0.5f);physicsStepFLIP(dt*0.5f);}
 else{physicsStepBOTTLE(dt);}
}
int main(){
 Application app;
 app.gamepad().set_enabled(false);
 auto sensor=app.sensors().OpenFirst<Acceleration>(app.devices(),20_ms);
 if(!sensor){app.log().Error("Accelerometer unavailable");return 1;}
 auto result=app.renderer().CreateHostSurface(2U,2U);
 if(!result){app.log().Error("Host surface unavailable");return 1;}
 auto surface=std::move(result.value());
 auto rasterResult=app.renderer().CreateRasterResources();
 if(!rasterResult){app.log().Error("Raster resources unavailable");return 1;}
 auto raster=rasterResult.value();
 if(surface.buffer_width()==0||surface.buffer_height()==0){app.log().Error("Invalid host surface size");return 1;}
 {char line[64];char*p=line;p=Perf::text(p,"LD buffer=");p=Perf::number(p,surface.buffer_width());p=Perf::text(p,"x");p=Perf::number(p,surface.buffer_height());*p=0;app.log().Info(line);}
 canvas.setOutputSize(int(surface.buffer_width()),int(surface.buffer_height()));
 rngState=uint32_t(app.clock().Now().microseconds())|1;
 canvas.createSprite(SCREEN_W,SCREEN_H);updateCharacterSprite();initClouds();initParticlesFLIP();updateBallSprites(0);currentBgColor=BALL_THEMES[0].defaultBg;
 const auto toBufferX=[&](int value){return value*int(surface.buffer_width())/240;};
 const auto toBufferY=[&](int value){return value*int(surface.buffer_height())/240;};
 // 33 ms (about 30 fps). Upstream measured 27-28 fps on its 480x480 panel, and
 // halving the frame rate halves the raster records the Host has to process per
 // second, which is what starved the USB task and caused command timeouts.
 auto ticker=app.timers().Every(33_ms).value();
 auto audioInfo=app.audio().info();
 bool audioEnabled=audioInfo && audioInfo->Supports(Waveform::kSine);
 uint64_t lastTone=0;
 uint64_t last=app.clock().Now().microseconds();
 TouchGesture gesture;
 int flash=-1;uint64_t flashUntil=0;
 // The polygon (Quad) rotation path is what the original 480x480 build used.
 // On other panel sizes we stay on the plain FillRect/Sprite path, which is
 // the most conservative record set the Host accepts.
 const bool nativeRotation=app.renderer().info().polygon_supported() && surface.buffer_width()==240U;
 g_duckRotate=nativeRotation;
 app.log().Info(nativeRotation?"LiquidDuck ready (polygon)":"LiquidDuck ready (software rotate)");
 Perf perf;
 bool updateLogged=false;bool presentLogged=false;
 app.Run([&](const Event& event){
  if(auto touch=event.touch()){
   int action=-1;
    int touchX=int(touch->x()*240U/surface.buffer_width());
    int touchY=int(touch->y()*240U/surface.buffer_height());
    if(touch->phase()==TouchPhase::kDown)gesture.down(touch->id(),touchX,touchY);
    if(touch->phase()==TouchPhase::kMove)gesture.move(touch->id(),touchX,touchY);
   if(touch->phase()==TouchPhase::kCancel)gesture.cancel(touch->id());
    if(touch->phase()==TouchPhase::kUp)action=gesture.up(touch->id(),touchX,touchY);
   if(action>=0){
    if(action==0){changeMode();}
    else if(action==1)changeTheme();
    else if(currentMode==MODE_FLIP)triggerExplosionFLIP(action==3?touchX:-1,action==3?touchY:-1);
    flash=action;flashUntil=app.clock().Now().microseconds()+160000;
   }
  }
  if(event.type()==EventType::kResume){last=app.clock().Now().microseconds();}

  if(event.TimerFrom(ticker)){
   uint32_t index;if(!surface.AcquireFree(index))return;
   auto sample=sensor->Read();if(sample){auto a=sample->value.meters_per_second_squared;tiltX=std::clamp(-a.x/9.80665f,-2.0f,2.0f);tiltY=std::clamp(a.y/9.80665f,-2.0f,2.0f);tiltZ=a.z/9.80665f;}
   uint64_t now=app.clock().Now().microseconds();auto elapsed=now-last;last=now;
   advance(frameSeconds(elapsed));
   uint64_t afterAdvance=app.clock().Now().microseconds();
   if(audioEnabled && (soundRequested || (frameEnergy>800 && now-lastTone>180000))){
    Tone tone{.waveform=Waveform::kSine,.frequency_hz=soundRequested?880U:1400U,.duration=60_ms,.volume_per_mille=45,.attack=3_ms,.release=40_ms};
    (void)app.audio().Play(tone);lastTone=now;
   }
   soundRequested=false;
   if(currentMode==MODE_FLIP)for(int i=0;i<8;++i)ballSprites[0][i].prepare(raster,uint8_t(i),0x0001);
   if(currentMode==MODE_FLIP)duckSprite.prepare(raster,16,0x0001,nativeRotation);
    auto update=surface.Update(index,[&](RasterDrawList& draw){
    canvas.draw=&draw;
    if(currentMode==MODE_FLIP)renderFrameFLIP();else renderFrameBOTTLE();
    canvas.draw=nullptr;
    // Three round icon buttons instead of the old full-width text bar. The
    // panel is a circle, so the bottom strip sat inside the bezel ring and had
    // its outer thirds clipped; three text labels also cannot fit side by side
    // (the small system font is ~11 buffer px per glyph).
    int active=gesture.action>=0?gesture.action:(now<flashUntil?flash:-1);
    const int bx[3]={73,107,141},by=184,bd=26;
    for(int i=0;i<3;++i){
     const int x=toBufferX(bx[i]),y=toBufferY(by);
     const int d=toBufferX(bx[i]+bd)-x;
     if(d<6)continue;
     const Color fill=(i==active)?Color::Rgb(74,136,190):Color::Rgb(15,25,40);
     for(int j=0;j<d;j+=2){
      const float dy=float(j)-(float(d)-1.0f)*0.5f;
      float sq=float(d*d)*0.25f-dy*dy;if(sq<0.0f)sq=0.0f;
      const int w=int(micropixel::math::Sqrt(sq)*2.0f);if(w<=0)continue;
      (void)draw.FillRect({x+(d-w)/2,y+j,w,j+2<=d?2:1},fill);
     }
     const int cx=x+d/2,cy=y+d/2,h=d/4>2?d/4:2;
     const Color ink=Color::White();
     if(i==0){
      for(int j=-h;j<=h;++j){          // triangle: cycle to the next mode
       const int aj=j<0?-j:j;
       const int w=(cx+h-2*aj)-(cx-h);
       if(w>0)(void)draw.FillRect({cx-h,cy+j,w,1},ink);
      }
     }else if(i==1){                    // colour swatches: theme / palette
      const int s=h>2?h:2;
      (void)draw.FillRect({cx-s-1,cy-s-1,s,s},Color::Rgb(90,215,255));
      (void)draw.FillRect({cx+1,cy-s-1,s,s},Color::Rgb(255,120,190));
      (void)draw.FillRect({cx-s-1,cy+1,s,s},Color::Rgb(255,205,90));
      (void)draw.FillRect({cx+1,cy+1,s,s},Color::Rgb(120,240,170));
     }else{                             // burst: splash / impact
      (void)draw.FillRect({cx-h,cy-1,2*h+1,2},ink);
      (void)draw.FillRect({cx-1,cy-h,2,2*h+1},ink);
     }
    }
    });
    uint64_t afterDraw=app.clock().Now().microseconds();
    if(!update){if(!updateLogged){updateLogged=true;app.log().Error(update.error().name());}}
    // Always hand the buffer back. Skipping Present on error would strand the
    // buffer, and once every buffer is stranded AcquireFree() never succeeds.
    auto presented=surface.Present(index);
    if(!presented&&!presentLogged){presentLogged=true;app.log().Error(presented.error().name());}
    if(!presented){surface.Reset();return;}
   perf.frame(app,now,afterAdvance,afterDraw,app.clock().Now().microseconds(),int(currentMode));
  }
 });
 if(fluid)flip_destroy(fluid);if(bottleFluid)flip_destroy(bottleFluid);portFree(bottleGrid);
 return 0;
}

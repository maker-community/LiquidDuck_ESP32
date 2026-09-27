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
 auto raster=app.renderer().CreateRasterResources().value();
 if(surface.buffer_width()!=240||surface.buffer_height()!=240){app.log().Error("Requires 480x480 display");return 1;}
 rngState=uint32_t(app.clock().Now().microseconds())|1;
 canvas.createSprite(SCREEN_W,SCREEN_H);updateCharacterSprite();initClouds();initParticlesFLIP();updateBallSprites(0);currentBgColor=BALL_THEMES[0].defaultBg;
 auto ticker=app.timers().Every(16_ms).value();
 auto audioInfo=app.audio().info();
 bool audioEnabled=audioInfo && audioInfo->Supports(Waveform::kSine);
 uint64_t lastTone=0;
 uint64_t last=app.clock().Now().microseconds();
 TouchGesture gesture;
 int flash=-1;uint64_t flashUntil=0;
 const bool nativeRotation=app.renderer().info().polygon_supported();
 app.log().Info("LiquidDuck ready: Mode / Theme / Splash");
 Perf perf;
 app.Run([&](const Event& event){
  if(auto touch=event.touch()){
   int action=-1;
   if(touch->phase()==TouchPhase::kDown)gesture.down(touch->id(),touch->x(),touch->y());
   if(touch->phase()==TouchPhase::kMove)gesture.move(touch->id(),touch->x(),touch->y());
   if(touch->phase()==TouchPhase::kCancel)gesture.cancel(touch->id());
   if(touch->phase()==TouchPhase::kUp)action=gesture.up(touch->id(),touch->x(),touch->y());
   if(action>=0){
    if(action==0){changeMode();}
    else if(action==1)changeTheme();
    else if(currentMode==MODE_FLIP)triggerExplosionFLIP(action==3?touch->x():-1,action==3?touch->y():-1);
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
   if(currentMode==MODE_FLIP && nativeRotation)duckSprite.prepare(raster,16,0x0001,true);
   surface.Update(index,[&](RasterDrawList& draw){
    canvas.draw=&draw;
    if(currentMode==MODE_FLIP)renderFrameFLIP();else renderFrameBOTTLE();
    canvas.draw=nullptr;
    (void)draw.FillRect({0,210,240,30},Color::Rgb(14,22,38));
    int active=gesture.action>=0?gesture.action:(now<flashUntil?flash:-1);
    if(active>=0&&active<3)(void)draw.FillRect({active*80,210,80,30},Color::Rgb(45,85,120));
    const char* modes[]={"BALLS","PIXEL","GRAD","WATER"};
    (void)draw.Text({9,216},modes[int(currentMode)],Color::White(),SystemFont::kSmall);
    (void)draw.Text({85,216},"THEME",Color::White(),SystemFont::kSmall);
    (void)draw.Text({164,216},"SPLASH",Color::White(),SystemFont::kSmall);
   }).value();uint64_t afterDraw=app.clock().Now().microseconds();surface.Present(index).value();
   perf.frame(app,now,afterAdvance,afterDraw,app.clock().Now().microseconds(),int(currentMode));
  }
 });
 if(fluid)flip_destroy(fluid);if(bottleFluid)flip_destroy(bottleFluid);portFree(bottleGrid);
 return 0;
}

#include <cassert>
#include <cmath>
#include "../src/liquidduck.cpp"
int main(){
 float minX,maxX,minY,maxY;
 FlipFluid* pixelFluid=flip_create(1.0f,12.0f/14.0f,14,12,0.45f);
 assert(pixelFluid);
 flip_get_particle_bounds(pixelFluid,&minX,&maxX,&minY,&maxY);
 assert(mapParticleToCell(minX,minX,maxX,14)==0);
 assert(mapParticleToCell(maxX,minX,maxX,14)==13);
 assert(mapParticleToCell(minY,minY,maxY,12)==0);
 assert(mapParticleToCell(maxY,minY,maxY,12)==11);
 for(int cell=0;cell<14;++cell){
  float p=minX+(float(cell)+0.5f)*(maxX-minX)/14.0f;
  float mirror=maxX-(float(cell)+0.5f)*(maxX-minX)/14.0f;
  assert(mapParticleToCell(p,minX,maxX,14)==cell);
  assert(mapParticleToCell(mirror,minX,maxX,14)==13-cell);
 }
 flip_destroy(pixelFluid);
 for(int angle=0;angle<360;angle+=5){
  duck.angle=angle;imuGX=imuGY=0;
  clouds[0]={-100,-100,-10,-10,24};clouds[1]={400,400,10,10,15};
  updateClouds(1.f/30);
  float sn=std::sin(angle*3.14159265f/180),cs=std::cos(angle*3.14159265f/180);
  for(auto c:clouds)for(int part=-1;part<=1;++part){
   float x=c.x,y=c.y,r=c.size;
   if(part){x+=c.size*(part*.7f*cs-.2f*sn);y+=c.size*(part*.7f*sn+.2f*cs);r*=.8f;}
   assert(x-r>=-.001f&&x+r<=SCREEN_W+.001f);assert(y-r>=-.001f&&y+r<=SCREEN_H+.001f);
  }
 }
 initParticlesFLIP();int n;float *p,*v;flip_get_particles(fluid,&n,&p,&v);
 for(int i=0;i<n*2;++i)v[i]=0;
 triggerExplosionFLIP(120,105);
 for(int i=0;i<n;++i){float dx=p[i*2]-screen_to_sim_x(120),dy=p[i*2+1]-screen_to_sim_y(105);assert(std::isfinite(v[i*2])&&std::isfinite(v[i*2+1]));assert(dx*v[i*2]+dy*v[i*2+1]>=-.001f);if(dx*dx+dy*dy>=EXPLOSION_RADIUS*EXPLOSION_RADIUS)assert(v[i*2]==0&&v[i*2+1]==0);}
 flip_destroy(fluid);fluid=nullptr;
}

//code by bitluni give me a shout-out if you like it

#include <soc/rtc.h>
#include "AudioSystem.h"
#include "AudioOutput.h"
#include "Graphics.h"
#include "Image.h"
#include "SimplePALOutput.h"
#include "GameControllers.h"
#include "Sprites.h"
#include "Font.h"

//lincude graphics and sounds
namespace font88
{
#include "gfx/font.h"
}
Font font(8, 8, font88::pixels);
#include "gfx/ship.h"
#include "gfx/enemy1.h"
#include "gfx/blaster1.h"
#include "gfx/laser.h"
#include "gfx/rock.h"
#include "gfx/rocket.h"
#include "gfx/shot.h"
#include "gfx/explosion.h"
#include "gfx/exhaust.h"
#include "sfx/sounds.h"

#include "GameEntity.h"
#include "Animation.h"

////////////////////////////
//controller pins
const int LATCH = 16;
const int CLOCK = 17;
const int CONTROLLER_DATA_PIN = 18;
GameControllers controllers;

////////////////////////////
//audio configuration
const int SAMPLING_RATE = 20000;
const int BUFFER_SIZE = 4000;
AudioSystem audioSystem(SAMPLING_RATE, BUFFER_SIZE);
AudioOutput audioOutput;

///////////////////////////
//Video configuration
const int XRES = 320;
const int YRES = 240;
Graphics graphics(XRES, YRES);
SimplePALOutput composite;

void compositeCore(void *data)
{
  while (true)
  {
    composite.sendFrame(&graphics.frame);
  }
}

int stars[256][2] = {1000000};

int starsDraw()
{
  for(int i = 0; i < 256; i++)
  {
    stars[i][1] += i + 1;
    int y = stars[i][1] >> 6;
    if(y >= 240)
    {
      stars[i][0] = rand() % 320;
      stars[i][1] = 0;
      y = 0;
    }
    int c = graphics.rgb(32 + (i >> 3), 32 + (i >> 3), 32 + (i >> 3));
    for(int l = 0; l < 1 + i >> 6; l++)
    {
      graphics.dot(stars[i][0], y + l, c);
    }
  }
}

class Projectile : public GameEntity
{
  public:
  int lifeTime;
  GameEntity *target;
  int power;
  int size;
  Projectile(Sprites &sprites, int x, int y, int power, int vx, int vy, int lifeTime, GameEntity *target = 0)
  {
    this->sprites = &sprites;
    this->x = x << 16;
    this->y = y << 16;
    this->vx = vx;
    this->vy = vy;
    this->lifeTime = lifeTime;
    this->target = target;
    this->power = power;
    size = 10;
  }
  
  void targetKilled(GameEntity *e)
  {
    if(e == target)
      target = 0;
  }
};

Projectile *projectiles[100] = {0};

void emitProjectile(Projectile *p)
{
  for(int i = 0; i < 100; i++)
  {
    if(!projectiles[i])
    {
      projectiles[i] = p;
      return;
    }
  }
  //change to oldest
  delete p;
}

void projectilesAct(int dt)
{
  for(int i = 0; i < 100; i++)
  {
    if(projectiles[i])
      if(!projectiles[i]->act(dt))
      {
        delete projectiles[i];
        projectiles[i] = 0;
      }
  }
}

void projectilesDraw()
{
  for(int i = 0; i < 100; i++)
    if(projectiles[i])
      if(projectiles[i])
        projectiles[i]->draw(graphics);
}

Animation *animations[100] = {0};

void animationsAct(int dt)
{
   for(int i = 0; i < 100; i++)
  {
    if(animations[i])
      if(!animations[i]->act(dt))
      {
        delete animations[i];
        animations[i] = 0;
      }
  }
}

void animationsDraw()
{
    for(int i = 0; i < 100; i++)
    if(animations[i])
      if(animations[i])
        animations[i]->draw(graphics);
}

void animationsEmit(Animation *e)
{
  for(int i = 0; i < 100; i++)
  {
    if(!animations[i])
    {
      animations[i] = e;
      return;
    }
  }
  delete e;
}
class Shot : public Projectile
{
  public:
  Shot(int x, int y, int vx, int vy)
    :Projectile(shot, x, y, 1, vx, vy, 1000)
  {
  }

  virtual bool act(int dt)
  {
    lifeTime -= dt;
    x += vx * dt;
    y += vy * dt;
    return lifeTime >= 0;
  }

  virtual void draw(Graphics &g)
  {
    sprites->drawMix(g, 0, x >> 16, y >> 16);
  }  
};


int trackAngle(int dx, int dy, int angle)
{
  float c = dx * sin(angle * M_PI / 180) - dy * cos(angle * M_PI / 180);
  if(c < 0) return 1;
  if(c > 0) return -1;
  return 0;
}

class Rocket : public Projectile
{
  public:
  int angle;
  int v;
  int smokeCooldown;
  Rocket(int x, int y, int angle, GameEntity *target)
    :Projectile(rocket, x, y, 3, 0, 0, 3000, target)
  {
    this->angle = angle << 16;
    v = 1000;
    smokeCooldown = 0;
  }

  virtual bool act(int dt)
  {
    lifeTime -= dt;
    smokeCooldown -= dt;
    if(smokeCooldown < 0)
    {
      smokeCooldown = 50;
      animationsEmit(new Animation(exhaust, x >> 16, y >> 16, 3, 7, 80, 0));
    }
    v += dt * 20;
    if(v > 20000) v = 20000;
    if(target)
    {
      angle += trackAngle( target->x - x, target->y - y, angle >> 16) * dt * 15000;
    }
    float a = (angle >> 16) * M_PI / 180;
    x += (cos(a) * v * dt);
    y += (sin(a) * v * dt);
    angle = angle % (360 << 16);
    return lifeTime >= 0;
  }

  virtual void draw(Graphics &g)
  {
    sprites->drawMix(g, (((angle >> 14) - 45) / 90 + 4) & 15, x >> 16, y >> 16);
  }  
};

class Laser : public Projectile
{
  public:
  int angle;
  int v;
  Laser(int x, int y, int angle)
    :Projectile(laser, x, y, 10, 0, 0, 500, 0)
  {
    this->angle = angle << 16;
    v = 80000;
    float a = (this->angle >> 16) * M_PI / 180;
    vx = (int)(cos(a) * v);
    vy = (int)(sin(a) * v);
    size = 30;
  }

  virtual bool act(int dt)
  {
    lifeTime -= dt;
    x += vx * dt;
    y += vy * dt;
    return lifeTime >= 0;
  }

  virtual void draw(Graphics &g)
  {
    //sprites->drawMix(g, ((angle >> 14) / 45 + 8) & 15, x >> 16, y >> 16);
  }  
};

class Enemy : public GameEntity
{
  public:
  int time;
  Enemy(Sprites &sprites, int x, int y, int vx, int vy)
  {
    this->sprites = &sprites;
    this->x = x << 16;
    this->y = y << 16;
    this->vx = vx;
    this->vy = vy;
    time = 0;
    life = 3;
  }
};

class Enemy1 : public Enemy
{
  public:
  int p[2];
  int ox, oy;
  Enemy1(int x, int y, int vx, int vy)
    :Enemy(enemy1, x, y, vx, vy)
  {
    p[0] = rand() & 64;
    p[1] = rand() & 31;
    ox = this->x;
    oy = this->y;
  }

  bool act(int dt)
  {
    time += dt;
    ox += vx * dt;
    oy += vy * dt;
    x = ox + ((int)(sin(time * 0.005f) * p[0])<< 16);
    y = oy + ((int)(cos(time * 0.005f) * p[1])<< 16);
    if(life <= 0)
    {
      sounds.play(audioSystem, 1, 0.5f);
      animationsEmit(new Animation(explosion, x >> 16, y >> 16, 0, 17, 20, 0));
    }
    return life > 0 && time < 5000;
  }

  void draw(Graphics &g)
  {
    sprites->drawMix(g, (time / 50) & 7, x >> 16, y >> 16);
  }
};

class Enemy2 : public Enemy
{
  public:
  Enemy2(int x, int y, int vx, int vy)
    :Enemy(rock, x, y, vx, vy)
  {
    life = 10;
  }

  bool act(int dt)
  {
    time += dt;
    x += vx * dt;
    y += vy * dt;
    if(life <= 0)
    {
      sounds.play(audioSystem, 1, 0.5f);
      animationsEmit(new Animation(explosion, x >> 16, y >> 16, 0, 17, 20, 0));
    }
    return life > 0 && time < 10000;
  }

  void draw(Graphics &g)
  {
    sprites->drawMix(g, (time / 50) & 15, x >> 16, y >> 16);
  }
};

Enemy *enemies[10] = {0};

void enemiesDraw()
{
    for(int i = 0; i < 10; i++)
    if(enemies[i])
      if(enemies[i])
        enemies[i]->draw(graphics);
}

void enemiesEmit(Enemy *e)
{
  for(int i = 0; i < 10; i++)
  {
    if(!enemies[i])
    {
      enemies[i] = e;
      return;
    }
  }
  delete e;
}

Enemy *getClosestEnemy(int x, int y)
{
  Enemy *e = 0;
  int d = 0x7fffffff;
  for(int i = 0; i < 10; i++)
  {
    if(enemies[i])
    {
      int dx = (enemies[i]->x >> 16) - x;
      int dy = (enemies[i]->y >> 16) - y;
      int cd = dx * dx + dy * dy;
      if(cd < d)
      {
        e = enemies[i];
        d = cd;
      }
    }
  }
  return e;
}

class Weapon : public GameEntity
{
  public:
  int cooldown;
  int dx, dy;
  GameEntity *parent;
  GameEntity *target;
  int angle;
  Weapon(Sprites &sprites, int x, int y, GameEntity *parent, int dx, int dy, int angle)
  {
    this->parent = parent;
    this->sprites = &sprites;
    this->angle = angle << 16;
    this->x = x << 16;
    this->y = y << 16;
    this->vx = 0;
    this->vy = 0;
    this->dx = dx << 16;
    this->dy = dy << 16;
    cooldown = 0;
    target = 0;
  }
  void targetKilled(GameEntity *e)
  {
    if(e == target)
      target = 0;
  }
};

class LaserBlaster : public Weapon
{
  public:
  int soundId;
  int active;
  int power;
  int cooldown2;
  LaserBlaster(int x, int y, GameEntity *parent, int dx, int dy, int angle)
    :Weapon(blaster1, x, y, parent, dx, dy, angle)
  {
    soundId = -1;
    active = 0;
    power = 800;
    cooldown2 = 0;
  }

  bool act(int dt)
  {
    cooldown -= dt;
    cooldown2 -= dt;
    active += dt;
    x = parent->x + dx;
    y = parent->y + dy;
    if(!target)
    {
      target = getClosestEnemy(x, y);
    }
    if(target)
    {
      angle += trackAngle( target->x - x, target->y - y, angle >> 16) * dt * 5000;
    }
    if(controllers.down(0, GameControllers::B) && cooldown2 <= 0)
    {
      active = true;
      if(cooldown <= 0)
      {
        cooldown += 100;
        emitProjectile(new Laser((x >> 16), (y >> 16), angle >> 16));
        if(soundId == -1)
          soundId = sounds.play(audioSystem, 0, 1, 1, true);
        power -= dt;
        if(power <= 0)
        {
          cooldown2 = 1000;
          power = 800;
        }
      }
    }
    else
    {
      active = false;
      if(soundId > -1)
      {
        sounds.stop(audioSystem, soundId);
        soundId = -1;
      }
    }
/*    x += vx * dt;
    y += vy * dt;*/
    true;
  }

  void draw(Graphics &g)
  {
    if(active)
    {
      float a = (this->angle >> 16) * M_PI / 180;
      int x0 = (x >> 16) - 1;
      int y0 = (y >> 16) - 1;
      int x1 = x0 + (int)(cos(a) * 320);
      int y1 = y0 + (int)(sin(a) * 320);
      g.line(x0 + 0, y0 + 0, x1 + 0, y1 + 0, g.rgb(200, 200, 255));
      g.line(x0 + 1, y0 + 0, x1 + 1, y1 + 0, g.rgb(200, 200, 255));
      g.line(x0 + 0, y0 + 1, x1 + 0, y1 + 1, g.rgb(200, 200, 255));
      g.line(x0 + 1, y0 + 1, x1 + 1, y1 + 1, g.rgb(200, 200, 255));
    }
    sprites->drawMix(g, ((((angle >> 14)-25) / 90 + 4) & 15) + (active?16:0), x >> 16, y >> 16);
  }
};

class Player : public GameEntity
{
  public:
  int cooldown[2];
  int gunPosition[2];
  Weapon *weapons[4];
  bool fire;
  Player(Sprites &sprites, int x, int y)
  {
    this->sprites = &sprites;
    this->x = x << 16;
    this->y = y << 16;
    vx = 0;
    vy = 0;
    cooldown[0] = cooldown[1] = 0;
    gunPosition[0] = gunPosition[1] = 0;
    for(int i = 0; i < 4; i++)
      weapons[i] = 0;
  }

  bool act(int dt)
  {
    const int topSpeed = 10000;
    const int cooldownValues[2] = {50, 500};

    if(controllers.pressed(0, GameControllers::SELECT) && !weapons[0])
    {
      weapons[0] = new LaserBlaster(x >> 16, y >> 16, this, -20, 0, 0);
    }
    if(controllers.down(0, GameControllers::LEFT))
      vx -= dt * 100;
    if(controllers.down(0, GameControllers::RIGHT))
      vx += dt * 100;
    vx -= (vx / 10);
    vx = min(topSpeed, max(-topSpeed, vx));
    if(controllers.down(0, GameControllers::UP))
      vy -= dt * 100;
    if(controllers.down(0, GameControllers::DOWN))
      vy += dt * 100;
    vy -= (vy / 10);
    vy = min(topSpeed, max(-topSpeed, vy));
    
    cooldown[0] -= dt;
    cooldown[1] -= dt;
    
    if(controllers.down(0, GameControllers::A) && cooldown[0] <= 0)
    {
      cooldown[0] = cooldownValues[0];
      emitProjectile(new Shot((x >> 16) + gunPosition[0], (y >> 16) + gunPosition[1], 0, -20000));
      sounds.play(audioSystem, 6, 0.3f, 1);
    }
    fire = controllers.down(0, GameControllers::B);
    if(controllers.down(0, GameControllers::B) && cooldown[1] <= 0)
    {
      cooldown[1] = cooldownValues[1];
      emitProjectile(new Rocket((x >> 16), (y >> 16), -90, getClosestEnemy(x >> 16, y >> 16)));
      sounds.play(audioSystem, 4, 0.3, 1.5f);
    }
    x += vx * dt;
    y += vy * dt;

    for(int i = 0; i < 4; i++)
      if(weapons[i])
          weapons[i]->act(dt);
    return true;
  }

  void draw(Graphics &g)
  {
    int dx = 0, dy = 0;
    if(vx > 4000) dx = 1;
    if(vx > 9000) dx = 2;
    if(vx < -4000) dx = 3;
    if(vx < -9000) dx = 4;
    if(vy > 5000) dy = 5;
    if(vy < -5000) dy = 10;
    sprites->drawMix(g, dx + dy, x >> 16, y >> 16);
    gunPosition[0] = sprites->point(dx + dy, 1)[0] - sprites->point(dx + dy, 0)[0];
    gunPosition[1] = sprites->point(dx + dy, 1)[1] - sprites->point(dx + dy, 0)[1];
    for(int i = 0; i < 4; i++)
      if(weapons[i])
        weapons[i]->draw(g);
  }

  void targetKilled(GameEntity *e)
  {
    for(int i = 0; i < 4; i++)
      if(weapons[i])
        weapons[i]->targetKilled(e);
  }

};

void setup()
{
  rtc_clk_cpu_freq_set(RTC_CPU_FREQ_240M);              //highest cpu frequency

  //initialize composite output and graphics
  composite.init();
  graphics.init();
  graphics.setFont(font);
  xTaskCreatePinnedToCore(compositeCore, "c", 1024, NULL, 1, NULL, 0);

  //initialize audio output
  audioOutput.init(audioSystem);

  //initialize controllers
  controllers.init(LATCH, CLOCK);
  controllers.setController(0, GameControllers::NES, CONTROLLER_DATA_PIN); //first controller

  //Play first sound in loop (music)
  //sounds.play(audioSystem, 1, 1, 1, true);
}

Player player(ship, 160, 200);

void enemiesAct(int dt)
{
   for(int i = 0; i < 10; i++)
  {
    if(enemies[i])
      if(!enemies[i]->act(dt))
      {
        for(int j = 0; j < 100; j++)
          if(projectiles[j])
            projectiles[j]->targetKilled(enemies[i]);
        player.targetKilled(enemies[i]);
        delete enemies[i];
        enemies[i] = 0;
      }
  }
}

void loop()
{
  static int time = 0;
  int t = millis();
  int dt = t - time;

  if((time / 1000) != (t / 1000))
    enemiesEmit(new Enemy1((rand() & 127) + 100, -20, (rand() & 1023) - 512, 3000));
  if((time / 1000) != (t / 1000))
    enemiesEmit(new Enemy2((rand() & 63) + 130, -20, (rand() & 2047) - 1024, 2500));
  
  time = t;

  //fill audio buffer
  audioSystem.calcSamples();
  controllers.poll();

  projectilesAct(dt);
  enemiesAct(dt);
  player.act(dt);
  animationsAct(dt);
  for(int i = 0; i < 100; i++)
    if(projectiles[i])
    {
      for(int j = 0; j < 10; j++)
      if(enemies[j])
      {
        if(abs((projectiles[i]->x >> 16) - (enemies[j]->x >> 16)) < projectiles[i]->size &&
          abs((projectiles[i]->y >> 16) - (enemies[j]->y >> 16)) < projectiles[i]->size
        )
        {
          animationsEmit(new Animation(exhaust, projectiles[i]->x >> 16, projectiles[i]->y >> 16, 0, 7, 20, 0));
          sounds.play(audioSystem, 6, 0.3f, 2);
          enemies[j]->life -= projectiles[i]->power;
          delete projectiles[i];
          projectiles[i] = 0;
          break;
        }
      }
    }
  graphics.begin(0);
  static int xpos = 0;
  xpos = (xpos +1 ) & 255;

  starsDraw();

  enemiesDraw();
  projectilesDraw();
  player.draw(graphics);
  animationsDraw();
  graphics.setCursor(16, 16);
  /*graphics.fillRect(0,0,2,240,graphics.rgb(255, 255, 255));
  graphics.fillRect(318,0,2,240,graphics.rgb(255, 255, 255));*/
  //graphics.setTextColor(graphics.rgb(255, 255, 255));
  //graphics.print("Hello!\n");
  /*graphics.print(player.gunPosition[0]);
  graphics.print("\n");
  graphics.print(player.gunPosition[1]);*/
  graphics.end();
}



#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <stdlib.h>
#include <string.h>

// include NESLIB header
#include "neslib.h"

// include CC65 NES Header (PPU)
#include <nes.h>

// link the pattern table into CHR ROM
//#link "chr_generic.s"

// BCD arithmetic support
#include "bcd.h"
//#link "bcd.c"

// VRAM update buffer
#include "vrambuf.h"
//#link "vrambuf.c"


//Static variables listing:

#define COLS    30
#define ROWS    60


#define MAX_LANES 10
#define GAP_SIZE   4
#define BOTTOM_LANE_Y 2

#define MAX_ACTORS 9
#define SCREEN_Y_BOTTOM 208
#define ACTOR_MIN_X 16
#define ACTOR_MAX_X 228
#define ACTOR_SCROLL_UP_Y 110
#define ACTOR_SCROLL_DOWN_Y 140


#define TILE 0xf4
#define ATTR 0x02
#define RED  0x00

// vertical scroll amount in pixels
static int scroll_pixel_yy = 0;

// vertical scroll amount in tiles (scroll_pixel_yy / 8)
static byte scroll_tile_y = 0;

// last screen Y position of player sprite
static byte player_screen_y = 0;


////////////////////////////////////////
//Metasprite Stuff:
//
//////////////////////////////////


//Shortcut to define the Player / Cars and have them face left / right:

// define a 2x2 metasprite
#define DEF_METASPRITE_2x2(name,code,pal)\
const unsigned char name[]={\
        0,      0,      (code)+0,   pal, \
        0,      8,      (code)+1,   pal, \
        8,      0,      (code)+2,   pal, \
        8,      8,      (code)+3,   pal, \
        128};

// define a 2x2 metasprite, flipped horizontally
#define DEF_METASPRITE_2x2_FLIP(name,code,pal)\
const unsigned char name[]={\
        8,      0,      (code)+0,   (pal)|OAM_FLIP_H, \
        8,      8,      (code)+1,   (pal)|OAM_FLIP_H, \
        0,      0,      (code)+2,   (pal)|OAM_FLIP_H, \
        0,      8,      (code)+3,   (pal)|OAM_FLIP_H, \
        128};

//Player's Metasprite:

// right-facing
DEF_METASPRITE_2x2(playerRStand, 0xd8, 0);
DEF_METASPRITE_2x2(playerRRun1, 0xdc, 0);
DEF_METASPRITE_2x2(playerRRun2, 0xe0, 0);
DEF_METASPRITE_2x2(playerRRun3, 0xe4, 0);


// left-facing
DEF_METASPRITE_2x2_FLIP(playerLStand, 0xd8, 0);
DEF_METASPRITE_2x2_FLIP(playerLRun1, 0xdc, 0);
DEF_METASPRITE_2x2_FLIP(playerLRun2, 0xe0, 0);
DEF_METASPRITE_2x2_FLIP(playerLRun3, 0xe4, 0);


//Car's Metasprite:

//Facing the Right
DEF_METASPRITE_2x2(carRStand, 0x81, 0);
DEF_METASPRITE_2x2(carRRun1, 0x81, 0);
DEF_METASPRITE_2x2(carRRun2, 0x81, 0);
DEF_METASPRITE_2x2(carRRun3, 0x81, 0);



//Facing the Left
DEF_METASPRITE_2x2_FLIP(carLStand, 0x81, 0);
DEF_METASPRITE_2x2_FLIP(carLRun1, 0x81, 0);
DEF_METASPRITE_2x2_FLIP(carLRun2, 0x81, 0);
DEF_METASPRITE_2x2_FLIP(carLRun3, 0x81, 0);



// player run sequence
const unsigned char* const playerRunSeq[16] = {
  playerLRun1, playerLRun2, playerLRun3, 
  playerLRun1, playerLRun2, playerLRun3, 
  playerLRun1, playerLRun2,
  playerRRun1, playerRRun2, playerRRun3, 
  playerRRun1, playerRRun2, playerRRun3, 
  playerRRun1, playerRRun2,
};



//Car's "Run" Sequence (just to make sure it can face the proper direction
const unsigned char* const carRunSeq[16] = {
  carLRun1, carLRun2, carLRun3, 
  carLRun1, carLRun2, carLRun3, 
  carLRun1, carLRun2,
  carRRun1, carRRun2, carRRun3, 
  carRRun1, carRRun2, carRRun3, 
  carRRun1, carRRun2,
};


//A sample Metasprite so I can use it to make sure the start is correct:
const unsigned char START[]={
        0,      0,      TILE+0,   RED, 
        0,      8,      TILE+1,   RED, 
        8,      0,      TILE+2,   RED, 
        8,      8,      TILE+3,   RED, 
        128};




//////////////////////////////////////////////////
//Function Listing:
//
///////////////////////////////////////////////////


//Display the Title Screen:
void titleScreen()
{
  char mess[32];
  byte cont;
  
  bool play;
  
  play = false;
  
  while (play == false)
  {
    //Display the Title:
    memset(mess,0,sizeof(mess));
    sprintf(mess,"Street Crossing");
    vrambuf_put(NTADR_A(2,2), mess, 32);
    
    //Give the prompt to the Player:
    memset(mess,0,sizeof(mess));
    sprintf(mess,"Press ENTER to play!");
    vrambuf_put(NTADR_A(2,4),mess,32);
    
    //Instruct the Player::
    memset(mess,0,sizeof(mess));
    sprintf(mess,"Can you escape the Lot?");
    vrambuf_put(NTADR_A(2,8),mess,32);
    
    
  
    
    
    //This reads off the Keyboard (only for Player 1)
    cont = pad_trigger(0);
    
    
    if (cont & PAD_START)
    {
      play = true;
      ppu_off();
      
      //Remove the text for the game:
      memset(mess,0,sizeof(mess));
      sprintf(mess,"");
      vrambuf_put(NTADR_A(2,2),mess,32);
      
      
      memset(mess,0,sizeof(mess));
      sprintf(mess,"");
      vrambuf_put(NTADR_A(2,4),mess,32);
      
      
      //Display the new text (or removal of in this case):
      ppu_on_all();
    }
    
    
  }
  
}

// random byte between (a ... b-1)
// use rand() because rand8() has a cycle of 255
byte rndint(byte a, byte b) 
{
  return (rand() % (b-a)) + a;
}


// assuming vertical scrolling (horiz. mirroring)
word getntaddr(byte x, byte y) 
{
  word addr;
  if (y < 30) {
    addr = NTADR_A(x,y);	// nametable A
  } else {
    addr = NTADR_C(x,y-30);	// nametable C
  }
  return addr;
}



// convert nametable address to attribute address
word nt2attraddr(word a) {
  return (a & 0x2c00) | 0x3c0 |
    ((a >> 4) & 0x38) | ((a >> 2) & 0x07);
}



//////////////////////////////
//Level / Lane Settings:
// - This segment is set to create the lanes for the game:
// - Functions declared here are to handle various elements in the Lane
//   .) Changing heights
//   .) Adjusting Starting Point
//   .) Drawing enemies (maybe?)
//   .) etc
//

///////////////////////////////////////
//Lane Structure Definition:
//- Set the properties of each lane
//- Each Lane is made of the following:
typedef struct Lane{
  byte ypos;
  int height:4;
  int gap:4;
  
} Lane; 



//This is roughly 2 bytes, so we only shift bits once.
Lane lanes[MAX_LANES]; //An array of Lanes


//This function sets the properties for each Lane
void setLanes()
{
  byte i;
  byte y = BOTTOM_LANE_Y;
  
  //This pointer references the start of the lane.
  Lane* prevlev = &lanes[0]; //Use &lanes[i] to reference the Array of Lanes
  
  //Assign the properties to each lane
  for (i = 0; i<MAX_LANES; i++)
  {
    Lane* lev = &lanes[i]; //Take the current index of loop
    lev->height = rndint(2,4)*2; //Give it a random height (somewhere between 4 - 10).
    lev->gap = 0;//The starting lane doesn't have a gap to go through.
    
    //Any lane past the start / 0 should have a gap:
    if (i>=1)
    {
      lev->gap = rndint(1,13); //Somehwere between 1-13
    }
    
    lev->ypos = y; //Assign the Y position to the array
    y += lev->height; //Set Y to the current height of the lev.
    prevlev = lev; //Assign the prelev Pointer to the current index in the loop.
  }
  
  
  //The Exit is special:
  //- They have fixed values to signifiy the end.
  lanes[MAX_LANES-1].height = 6; 
  lanes[MAX_LANES-1].gap =5; 
  
 
} //End of setLanes()


// create actors on a given lane, if slot is empty
// Declearing it here so we can define it later in a different segment.
void create_actors_on_lane(byte lane_index);


//This function will draw the lanes into the frame buffer at row_height
//0 == bottom of the stage
void drawLanes(byte row_height)
{
  char buf[COLS];
  char attrs[8];
  byte lane;
  byte dy;
  byte rowy;
  word addr;
  byte i;
  
  for (lane = 0; lane <MAX_LANES; lane++)
  {
    //Get the refernce to the array ready
    Lane* lev = &lanes[lane];
    
    //Find the height in each lane.
    dy = row_height - lev->ypos;
    
    //Just in case we overshoot:
    if (dy >= 255 - BOTTOM_LANE_Y)
    {
      dy = 0;
      
    }
    
    if (dy < lev->height)
    {
      if (dy <= 1)
      {
        for (i=0; i<COLS; i+=2)
        {
          
          if(dy)
          {
            buf[i] = TILE;
            buf[i+1] = TILE+2;
          }
          
          else
          {
            buf[i] = TILE+1;
            buf[i+1] = TILE+3;
          }
        }
        
        if (lev->gap)
        {
          memset(buf+lev->gap*2, 0, GAP_SIZE);
        }
        
      } //End of inner If Statement
      
      
      else
      {
        //clear the buffer: (it's gonna be full of stuff after the if statement)
        memset(buf,0,sizeof(buf));
        
        //Draw the Walls:
        if(lane<MAX_LANES-1)
        {
          buf[0] = TILE+1;
          buf[COLS-1] = TILE;
              
        }
        
      }//End of inner Else Statement
      
 
      //If you don't break here, you get a jumbled mess of tiles.
      //Break here
      break; //Escape the loop
      

    }
    
    
  } //End of For Loop
  
  
  //compute row in name buffer and address
  rowy = (ROWS-1) - (row_height % ROWS);
  addr = getntaddr(1,rowy);
  
  
  //Put stuff into the attribute table (every 4th row)
  if((addr &0x60) == 0)
  {
    byte a;
    
    if(dy == 1)
    {
      a = 0x05;
    }
    
    else if (dy == 3)
    {
      a = 0x50;
    }
    
    else
    {
      a = 0x00;
    }
    
    //write the entire row of attribute blocks
    memset(attrs,a,8);
    vrambuf_put(nt2attraddr(addr), attrs, 8);
    
  }
  
  //copy line to screen buffer:
  vrambuf_put(addr,buf,COLS);
  
  //We need this to draw enemy cars onscreen
  if (dy == 0 && (lane>=2))
  {
    create_actors_on_lane(lane);
  }
  
  
}// End of drawLanes()


//This function will draw the entire level.
void drawLevel()
{
  byte y;
  
  for(y=0; y<ROWS;y++)
  {
    drawLanes(y);
    vrambuf_flush();
  }
  
  
} //End of drawLevel()



//This function returns a 16-bit position representing the distance in pixels from the bottom of a lane to the bottom of the building. 
// - More accurately, this controls where the human and cars spawn.
// - _set_scroll_pixel_yy(byte n) determines where the screen is focused.
word get_lane_yy(byte lane)
{
  return lanes[lane].ypos*8+16;
}

// get Y ceiling position for a given floor
word get_ceiling_yy(byte lane) 
{
  return (lanes[lane].ypos + lanes[lane].height) * 8 + 16;
}

word get_limit_yy(byte lane)
{
  return (lanes[lane].ypos);
}


//This contols where the Screen is positioned. In relation to the player.
void set_scroll_pixel_yy(int yy)
{
  if((yy&7) == 0)
  {
    if(yy>scroll_pixel_yy)
    {
      drawLanes(scroll_tile_y+30);
    }
  }
  scroll_pixel_yy = yy;
  scroll_tile_y = yy >>3;
  
  scroll(0,479-((yy+224) % 480));
}



////////////////////////////////////
//- END OF LANE EDITOR
//
/////////////////////////////////////




/////////////////
//Actor Setttings:
//


//////
//Actor Types:
//-Declare what types of actors we're going to see
//-We only have two in this case: a Human (the player) and Cars (enemies):
//

typedef enum ActorType{
  ACTOR_PLAYER,ACTOR_CAR};



///////
//Actor States:
//-Each Actor has a state they can enter in
//- There's 5 in this case:
//  a.) INACTIVE: The Actor / Sprite is offscreen. Don't do anything
//  b.) STANDING: The Human is standing still. (Human is always onscreen)
//  c.) WALKING:  The Human is moving. It's X/Y cordiantes are chaging
//  d.) DRIVING:  The Car is moving. It's out to attack the Human!
//  e,) RISING:   The Human is "jumping". It can move up across gaps.
typedef enum ActorState{
  INACTIVE, STANDING, WALKING, DRIVING,RISING};


////////
//Actor Struct:
//- Define our Actors here. Every Actor has the following attributes:

typedef struct Actor{
  word yy;        // Y position in pixels (16 bits)
  byte x;         // X position in pixels (8 bits)
  byte lane;      // lane index
  byte state;     // Actor State 
  int name:2;     // ActorType (2 bits)
  int pal:2;      // palette color (2bits)
  int dir:1;      // directions (0 = right, 1 = left)
  int onscreen:1; // is the actor onscreen (0=no, 1= yes)
  } Actor;


Actor actors[MAX_ACTORS]; //An Array of Actors.
 
//This function assigns attributes to the enemy actors.
//The player actor (actors[0]) will be treated differently.
void create_actors_on_lane(byte lane_index)
{
  byte actor_index = (lane_index % (MAX_ACTORS-1)) + 1;
  struct Actor* a = &actors[lane_index];
  if (!a->onscreen) 
  {
    Lane* lane = &lanes[lane_index];
    a->state = DRIVING;
    a->name = ACTOR_CAR;
    a->x = rand8();
    a->yy = get_lane_yy(lane_index);
    a->lane = lane_index;
    a->onscreen = 1;
    a->dir = 0;
  }
}

//This function will draw the actor on screen. 
//- It also draws a sprite based on the STATE their in.
void draw_actors(byte i)
{
  struct Actor *a = &actors[i];
  bool dir;
  const unsigned char* meta;
  byte x,y;
  
  //get screen Y position of actor:
  int screen_y = SCREEN_Y_BOTTOM - a->yy + scroll_pixel_yy;
  
  //is it offscreen?
  if(screen_y > 192+8 || screen_y < -18)
  {
    a->onscreen = 0;
    return;
  }
  
  dir = a->dir;
  switch (a->state)
  {
    case INACTIVE:
      a->onscreen = 0;
      return; //inactive, offscreen
      
    case STANDING:
      meta = dir ? playerLStand : playerRStand;
      break;
      
    case WALKING:
      meta = playerRunSeq[((a->x>>1)&7) + (dir?0:8)];
      break;
      
    case DRIVING:
      meta = carRunSeq[((a->x>>1)&7) + (dir?0:8)];
      break; 
      
    case RISING:
      meta = dir ? playerLStand : playerRStand;
      break;
  }
  
  x = a->x;
  y = screen_y;
  
  oam_meta_spr_pal(x,y,a->pal,meta);
  
  //is this actor 0?
  if (i==0)
  {
    player_screen_y = y; //save last screen Y position
  }
  
  a->onscreen = 1; //if we drew the actor, consider it onscreen
  return;
  
}

// draw all sprites
void refresh_sprites() 
{
  byte i;
  // reset sprite index to 0
  oam_off = 0;
  // draw all actors
  for (i=0; i<MAX_ACTORS; i++)
  {
    draw_actors(i);
  }
  // hide rest of actors
  oam_hide_rest(oam_off);
}

// should we scroll the screen upward?
void check_scroll_up() 
{
  if (player_screen_y < ACTOR_SCROLL_UP_Y) 
  {
    set_scroll_pixel_yy(scroll_pixel_yy + 1);	// scroll up
  }
}



// move an actor (player or enemies)
// joystick - game controller mask
// scroll - if true, we should scroll screen (is player)
void move_actor(struct Actor* actor, byte joystick, bool scroll)
{
  
  
  
  //Min and Max X-cordaiante ranges for the new lane
  int gapMin;
  int gapMax;
  
  //Min and Max x-Cordinate ranges for the previous lane.
  int oldgapMin;
  int oldgapMax;
  
 

 // int count;
  //int limit;
  
  

  
  
  gapMin = (lanes[(actor->lane)+1].gap * 16)+8;
  gapMax = (lanes[(actor->lane)+1].gap * 16)+32;
  
  oldgapMin = (lanes[actor->lane].gap *16)+8;
  oldgapMax = (lanes[actor->lane].gap *16)+32;
  
  
  
  


  switch (actor->state) 
  {    
      
    case STANDING:
    //The player is moving left or right. Or they're trying to "rise".
    case WALKING:
      
      // Go Left if you hit Left
      if (joystick & PAD_LEFT) 
      {
        actor->x = actor->x -2;
        actor->dir = 1; //Make sure they're facing the right way.
        actor->state = WALKING;
      } 
      
      //Go Right if you hit Right
      else if (joystick &  PAD_RIGHT) 
      {
        actor->x = actor->x +2;
        actor->dir = 0; //See above
        actor->state = WALKING;
      } 
      
      //The player will attempt to Rise by pressing UP
      else if (joystick & PAD_UP) 
      {
       
        //You can only rise if you're within a certain range in the gaps.
        if(actor->x >= gapMin && actor->x <= gapMax)
        {
          //Once you're in that range, you enter the RISING state.
          
          //Here's a small boost to help you get started.
          actor->yy++;
          actor->state = RISING; //Now go rise Human!
          
       
       
          
        }
        
        //Any other attempt will just leave you in WALKING state
        else
        {
          actor->state = WALKING;
        }
       
       
        
      }
      
      //You can go back down the gap if you felt you timed your "Rise" wrong.
      else if (joystick & PAD_DOWN)
      {
        
        //Same as with UP: You can only Rise if you're in that range.
        if(actor->x >= oldgapMin && actor->x <= oldgapMax)
        {
            //You cannot fall through the lowest lane...
            if (actor->lane == 0)
            {
              
              actor->yy = actor->yy;
              actor->state = WALKING;
            }
          
           
            //Otherwise, go to the RISING state.
            else
            {
              //A small push to ensure you're falling.
              actor->yy--;
              actor->state = RISING; 
            }
        }
        
        
         
        
      }
      
      
      //No input? Just stand still.
      else 
      {
        actor->state = STANDING;
      }
      if (scroll) 
      {
        check_scroll_up();

      }
      break;
      
     
    //The player is "rising" through the gaps.  
    case RISING:
      
      //If you hit up, you'll go up.
      if (joystick & PAD_UP)
      {
        
        
         //Once we make it to a new lane, switch the WALKING State
         if(actor->yy >= get_lane_yy(actor->lane+1))
          { 
           
          
            actor->lane++; //New Lane. Go update it.
            actor->state = WALKING; //
           
            
          }
        
         //If you go back to the lane you were previously on, you'll go back to walking state.
         else if (actor->yy == get_lane_yy(actor->lane))
         {
           actor->state = WALKING;
         }
        
       
        
        //If you haven't hit a new lane yet, just keep track of it.   
        else
        {
           actor->yy++; //Incriment counter
           actor->state = RISING; //Keep going up...
          
       
        }
          
      }
      
      //If you hit down, you can go downwards while rising.
      else if (joystick & PAD_DOWN)
      {
        
          //If you hit the previous lane, then adjust the count so you can move on that lane properly
          if(actor->yy == get_lane_yy(actor->lane -1))
          {
            
           
              //If you're on the bottom floor, don't go past it.
               if (actor->lane == 0)
               {
                 actor->state = WALKING;
               }
      
               
               //Indiacate you're in a new lane and switch states.
               actor->lane--;
               actor->state = WALKING;
            
              
              
             
          }
        
       
          else
          {
      
            //If you haven't hit the old lane yet, keep track of the Human..
            actor->yy--; //Keep going down...
            actor->state = RISING; //You're still in the rising state
          }
        
        
        
          
      }
  
      
  
      //No input? Stay in RISING State:
      else
      {
        actor->state = RISING;
      }
      
      break;
    
    //This case is exclusive to the Cars:
    //- Go one direction until the car hits a wall:  
    case DRIVING:
      
     //Start off facing the right
     if (actor->dir == 0)
     {
       //Go to the right
       actor->x++;
       actor->state = DRIVING;
       actor->dir = 0;
       
       //If the car hits the wall, then turn around and go the other way.
       if(actor->x > ACTOR_MAX_X)
       {
         actor->dir = 1; //The Car is now facing left:
         actor->state = DRIVING;
       }
       
     }
      
     else
     {
       //Go to the left:
       actor->x--;
       actor->state = DRIVING;
       actor->dir = 1;
       
       //Same as before: turn around once the car hits the wall:
       if (actor->x < ACTOR_MIN_X)
       {
         actor->dir = 0;  //The Car is now facing right
         actor->state = DRIVING;
       }
     }

   
      break;
      
  }
  
  // don't allow player to travel past left/right edges of screen
  if (actor->x > ACTOR_MAX_X) 
  {
    actor->x = ACTOR_MAX_X; // we wrapped around right edge
  }
  if (actor->x < ACTOR_MIN_X) 
  {
    actor->x = ACTOR_MIN_X; //We go back to the left edge.
  }
}

// read joystick 0 and move the player
void move_player() 
{
  byte joy = pad_poll(0);
  move_actor(&actors[0], joy, true);
}


// returns absolute value of x
byte iabs(int x) {
  return x >= 0 ? x : -x;
}

// check to see if actor collides with any non-player actor
bool check_collision(Actor* a) {
  byte i;
  byte alane = a->lane;
;
  // iterate through entire list of actors
  for (i=1; i<MAX_ACTORS; i++) 
  {
    Actor* b = &actors[i];
    // actors must be on same lane and within 8 pixels
    if (b->onscreen &&
        alane == b->lane && 
        iabs(a->yy - b->yy) < 8 && 
        iabs(a->x - b->x) < 8) {
      return true;
    }
  }
  return false;
}






// game loop
void play_scene() 
{
  
  char dis[32]; //Quick Check to test out a few things:
  //char test[32]; //Another check for testing purposes;
  
  byte i; //index note
  // byte limit;
  bool gameOver; //A check to see how the game ended.
  
  
 
  
  gameOver = false;
  
  // initialize actors array  
  memset(actors, 0, sizeof(actors));
  
  //Hard code the player's attributes:
  actors[0].state = STANDING;
  actors[0].name = ACTOR_PLAYER;
  actors[0].pal = 3;
  actors[0].x = 64;
  actors[0].lane = 0;
  actors[0].yy = get_lane_yy(0);
  
  
  // put the player at bottom
  set_scroll_pixel_yy(0);
  
  //limit = get_limit_yy(actors->lane);
  
  // draw initial view of level into nametable
  drawLevel();
  
  //This is the main game loop:
  //- We play until either the Player makes it to the top
  //  or a car hits the player.
  while (actors[0].lane != MAX_LANES-1 && gameOver == false)
 {
     
    //limit = get_lane_yy(actors->lane);
    //Quick Check to see if the lane counter is going up.
    // - Can be subbed for other tests:
    
    
    //memset(dis,0,sizeof(dis));
    //sprintf(dis,"GapMin: %d",gapL);
    //vrambuf_put(NTADR_A(2,200), dis, 32);
    
    //memset(dis,0,sizeof(dis));
    //sprintf(dis,"Gap-Max: %d",gapR);
    //vrambuf_put(NTADR_A(2,208), dis, 32);
    
    //memset(dis,0,sizeof(dis));
    //sprintf(dis,"Actor-Y: %d",actors[0].yy);
    //vrambuf_put(NTADR_A(12,208),dis,32);
    
    //memset(dis,0,sizeof(dis));
    //sprintf(test,"Lane: %d",actors[0].lane);
    //vrambuf_put(NTADR_A(12,200),test,32);
    
    // flush VRAM buffer (waits next frame)
    vrambuf_flush();
    refresh_sprites();
    move_player();
    
    // move all the actors
    for (i=1; i<MAX_ACTORS; i++) 
    {
      move_actor(&actors[i], rand8(), false);
    }
    
    //If there's a crash, the game ends as signified by the 
    //gameOver variable.
    if(check_collision(&actors[0]))
    {
      gameOver = true;
    }
    
    

  }
  
  //Display a message, It changes depending on how the game ended:
  
  //If there was crash, tell the player the bad news...
  if (gameOver == true)
  {
    memset(dis,0,sizeof(dis));
    sprintf(dis, "Game Over...");
    vrambuf_put(NTADR_A(2,222), dis, 32);
    
  }
  
  
  //But if they made it, then congradlate them:  
  else
  {
     memset(dis,0,sizeof(dis));
     sprintf(dis, "You Win! You escaped!");
     vrambuf_put(NTADR_A(2,16), dis, 32);
    
  }
  
  ppu_on_all();
      

}








////////////////////////////
//
//END OF ACTOR EDITIOR
/////////////////////////

/*{pal:"nes",layout:"nes"}*/
const char PALETTE[32] = { 
  0x0E,			// screen color

  0x11,0x30,0x27,0x00,	// background palette 0
  0x1C,0x20,0x2D,0x00,	// background palette 1
  0x00,0x10,0x20,0x00,	// background palette 2
  0x06,0x16,0x26,0x00,   // background palette 3

  0x16,0x35,0x24,0x00,	// sprite palette 0
  0x00,0x37,0x25,0x00,	// sprite palette 1
  0x0D,0x2D,0x3A,0x00,	// sprite palette 2
  0x0D,0x27,0x2A	// sprite palette 3
};




// setup PPU and tables
void setup_graphics() 
{
  // clear sprites
  oam_clear();
  
   // clear vram buffer
  vrambuf_clear();
  
  // set NMI handler
  set_vram_update(updbuf);
  
  // set palette colors
  pal_all(PALETTE);
  
  
}


void main(void)
{
  setup_graphics();
  // draw message  
  //vram_adr(NTADR_A(2,2));
  //vram_write("HELLO, WORLD!", 12);
  // enable rendering
  

  
  
  ppu_on_all();
  
  
  ////////////////////////
  //Test Block: 
  //- A series of teststo make sure each segment of the game works properly
  //- If something goes wrong, I can go check it out each function individually
  
  
  /////////////////////
  //Test 0: Title Screen:
  //- Does the Tilte Screen display and fade correctly?
  
  //Display the Title Screen
  titleScreen();
  
  
  
  ////////////////////////
  //Test 1: Level Generator:
  //- Are the Lanes setup properly?
  //- Are the heights even / odd?
  //- Any weird bugs / popups?
  //- Can I set the screen to start at the bottom?
  
  
  //Get the properties of the level layed out.
  setLanes();
  
  //Get the screen to start at a specific point (at the bottom of the game)
  //set_scroll_pixel_yy(0); 
  
  
  //Draw the level:
  //drawLevel();
  
  
  
  play_scene();
  
  /////////////
  //Test 2: Actor / Sprite Generator:
  //- Do the charaacters load in properly?
  //- Do the collisions work correctly?
  
  
  
  
  
  ///End of Test Block
  /////////////////////////////////////
 
  // infinite loop
  while(1) {
  }
}

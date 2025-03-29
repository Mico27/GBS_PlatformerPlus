/*
Future notes on things to do:
- Limit air dashes before touching the ground
- Add an option for wall jump that only allows alternating walls.
- Currently, dashing through walls checks the potential end point, and if it isn't clear then it continues with the normal dash routine. 
    The result is that there could be a valid landing point across a wall, but the player is just a little too close for it to register. 
    I could create a 'look-back' loop that runs through the intervening tiles until it finds an empty landing spot.
- The bounce event is a funny one, because it can have the player going up without being in the jump state. I should perhaps add some error catching stuff for such situations
- Can I have a wall_jump init ahead of the normal jump init? If it's just checking a few more boxes....
- Improve ladder situation: jump from ladder option, bug with hitting the bottom of ladders, other stuff?

- Add check for camera bounds on Dash Init
- Solid actors have a fault: they only check collisions once on enter. This is especially problematic because of how GBStudio does invincibility frames,
  because it means the player can attach to a platform without causing the hit trigger to run. 
     - 2 potential solutions: 
     - give the player a minimum velocity every frame, forcing a re-collision. This might break the moving platform code though
     - manually re-trigger the collision call if the actor is attached.

TARGETS for Optimization
- State script assignment could be 100% be re-written to avoid all those assignments and directly use the pointers. I am not canny enough to do that.
- I should be able to combine solid actors and platform actors into a single check...
- It's inellegant that the dash check requires me to check again later if it succeeded or not. Can I reorganize this somehow?
- I think I can probably combine actor_attached and last_actor
- I need to refactor the downwards collision for Y, it's a bit of a mess at this point. I just can't wrap my head around it atm
- Wall Slide could be optimized to skill the acceleration bit, as the only thing that matters is tapping away

THINGS TO WATCH
- Does every state (that needs to) end up resetting DeltaX?

NOTES on GBStudio Quirks
- 256 velocities per position, 16 'positions' per pixel, 8 pixels per tile
- Player bounds: for an ordinary 16x16 sprite, bounds.left starts at 0 and bounds.right starts at 16. If it's smaller, bounds.left is POSITIVE
    For bounds.top, however, Y starts counting from the middle of a sprite. bounds.top is negative and bounds.bottom is positive
- CameraX is in the middle of the screen, not left corner

GENERAL STRUCTURE OF THIS FILE
The old format was well structured as a state-machine, isolating all the components into states. Unfortunately, it seems like the overhead of calling 
collision functions on the GameBoy makes this model unperformant. However, I'm also limited to the total amount of code that can be placed in a single bank. 
I cannot get rid of the functions and move the code into the file itself. New structure is a compromise that uses goto commands to skip some sections that are
shared by most of the states. 
    
INIT()
    Tweak a few fields so they don't overflow variables
    Normalize some fields so that they are applied over multiple frames
    Initialize variables
UPDATE()
    A. Input Checks (Double-tap Dash, Drop-Through)    I'm considering moving the drop-through check into a state
    B. STATE MACHINE 1 SWITCH: Falling, Ground, Jumping, Dashing, Climbing a Ladder, Wall Sliding
        State Initialization
        Calculate Change in Vertical Movement
        Some calculate horziontal movement
        Some calculate collisions
    C. Shared Collision
        Acceleration Code   
        Basic X Collision           gotoXCol
        Basic Y Collision           gotoYCol
        Actor Collision Check       gotoActorCol
    D. STATE MACHINE 2 SWITCH:      gotoSwitch2
        Animation
        State Change Logic
        Some Counters
    E. Trigger Check                gotoTriggerCol
    G. Tic Counters                 gotoCounters


BUGS:
 - When the player is on a moving platform and is hit by another one, they get caught mid-way on the next one.
*/
#pragma bank 255

#include "data/states_defines.h"
#include "states/platform.h"
#include "actor.h"
#include "camera.h"
#include "collision.h"
#include "data_manager.h"
#include "game_time.h"
#include "input.h"
#include "math.h"
#include "scroll.h"
#include "trigger.h"
#include "vm.h"
#include "states/playerstate_dash.h"
#include "states/playerstate_ground.h"
#include "states/playerstate_ladder.h"
#include "states/playerstate_knockback.h"
#include "states/playerstate_jump.h"
#include "states/playerstate_wall.h"
#include "states/playerstate_fall.h"


#ifndef INPUT_PLATFORM_JUMP
#define INPUT_PLATFORM_JUMP        INPUT_A
#endif
#ifndef INPUT_PLATFORM_RUN
#define INPUT_PLATFORM_RUN         INPUT_B
#endif
#ifndef INPUT_PLATFORM_INTERACT
#define INPUT_PLATFORM_INTERACT    INPUT_A
#endif
#ifndef PLATFORM_CAMERA_DEADZONE_Y
#define PLATFORM_CAMERA_DEADZONE_Y 16
#endif

#ifndef COLLISION_LADDER
#define COLLISION_LADDER 0x10
#endif
#ifndef COLLISION_SLOPE_45_RIGHT
#define COLLISION_SLOPE_45_RIGHT 0x20
#endif
#ifndef COLLISION_SLOPE_225_RIGHT_BOT
#define COLLISION_SLOPE_225_RIGHT_BOT 0x40
#endif
#ifndef COLLISION_SLOPE_225_RIGHT_TOP
#define COLLISION_SLOPE_225_RIGHT_TOP 0x60
#endif
#ifndef COLLISION_SLOPE_45_LEFT
#define COLLISION_SLOPE_45_LEFT 0x30
#endif
#ifndef COLLISION_SLOPE_225_LEFT_BOT
#define COLLISION_SLOPE_225_LEFT_BOT 0x50
#endif
#ifndef COLLISION_SLOPE_225_LEFT_TOP
#define COLLISION_SLOPE_225_LEFT_TOP 0x70
#endif
#ifndef COLLISION_SLOPE
#define COLLISION_SLOPE 0xF0
#endif

#define IS_ON_SLOPE(t) ((t) & 0x60)
#define IS_SLOPE_LEFT(t) ((t) & 0x10)
#define IS_SLOPE_RIGHT(t) (((t) & 0x10) == 0)
#define IS_LADDER(t) (((t) & 0xF0) == 0x10)

pStates plat_state;
pStates que_state;
script_state_t state_events[21];

//DEFAULT ENGINE VARIABLES
WORD plat_min_vel;
WORD plat_walk_vel;
WORD plat_run_vel;
WORD plat_climb_vel;
WORD plat_walk_acc;
WORD plat_run_acc;
WORD plat_dec;
WORD plat_jump_vel;
WORD plat_grav;
WORD plat_hold_grav;
WORD plat_max_fall_vel;

//PLATFORMER PLUS ENGINE VARIABLES
//All engine fields are prefixed with plat_
BYTE plat_camera_deadzone_x; // Camera deadzone
UBYTE plat_camera_block;    //Limit the player's movement to the camera's edges
UBYTE plat_drop_through;    //Drop-through control
UBYTE plat_mp_group;        //Collision group for platform actors
UBYTE plat_solid_group;     //Collision group for solid actors
WORD plat_jump_min;         //Jump amount applied on the first frame of jumping
UBYTE plat_hold_jump_max;   //Maximum number for frames for continuous input
UBYTE plat_extra_jumps;     //Number of jumps while in the air
WORD plat_jump_reduction;   //Reduce height each double jump
UBYTE plat_coyote_max;      //Coyote Time maximum frames
UBYTE plat_buffer_max;      //Jump Buffer maximum frames
UBYTE plat_wall_jump_max;   //Number of wall jumps in a row
UBYTE plat_wall_slide;      //Enables/Disables wall sliding
WORD plat_wall_grav;        //Gravity while clinging to the wall
WORD plat_wall_kick;        //Horizontal force for pushing off the wall
UBYTE plat_float_input;     //Input type for float (hold up or hold jump)
WORD plat_float_grav;       //Speed of fall descent while floating
UBYTE plat_air_control;     //Enables/Disables air control
UBYTE plat_turn_control;    //Controls the amount of slippage when the player turns while running.
WORD plat_air_dec;          // air deceleration rate
UBYTE plat_run_type;        //Chooses type of acceleration for jumping
WORD plat_turn_acc;         //Speed with which a character turns
UBYTE plat_run_boost;        //Additional jump height based on player horizontal speed
UBYTE plat_dash;            //Choice of input for dashing: double-tap, interact, or down and interact
UBYTE plat_dash_style;      //Ground, air, or both
UBYTE plat_dash_momentum;   //Applies horizontal momentum or vertical momentum, neither or both
UBYTE plat_dash_through;    //Choose if the player can dash through actors, triggers, and walls
WORD plat_dash_dist;        //Distance of the dash
UBYTE plat_dash_frames;     //Number of frames for dashing
UBYTE plat_dash_ready_max;  //Time before the player can dash again
UBYTE plat_dash_deadzone;

UBYTE nocontrol_h;          //Turns off horizontal input, currently only for wall jumping
UBYTE nocollide;            //Turns off vertical collisions, currently only for dropping through platforms
WORD deltaX;
WORD deltaY;

//COUNTER variables
UBYTE ct_val;               //Coyote Time Variable
UBYTE jb_val;               //Jump Buffer Variable
UBYTE wc_val;               //Wall Coyote Time Variable
UBYTE hold_jump_val;        //Jump input hold variable
UBYTE dj_val;               //Current double jump
UBYTE wj_val;               //Current wall jump

//WALL variables 
BYTE last_wall;             //tracks the last wall the player touched
BYTE col;

//DASH VARIABLES
UBYTE dash_ready_val;       //tracks the current amount before the dash is ready
WORD dash_dist;             //Takes overall dash distance and holds the amount per-frame
UBYTE dash_currentframe;    //Tracks the current frame of the overall dash
BYTE tap_val;               //Number of frames since the last time left or right button was tapped
UBYTE dash_end_clear;       //Used to store the result of whether the end-position of a dash is empty

//COLLISION VARS
actor_t *last_actor;        //The last actor the player hit, and that they were attached to
UBYTE actor_attached;       //Keeps track of whether the player is currently on an actor and inheriting its movement
WORD mp_last_x;             //Keeps track of the pos.x of the attached actor from the previous frame
WORD mp_last_y;             //Keeps track of the pos.y of the attached actor from the previous frame


//JUMPING VARIABLES
WORD jump_reduction_val;    //Holds a temporary jump velocity reduction
WORD jump_per_frame;        //Holds a jump amount that has been normalized over the number of jump frames
WORD jump_reduction;        //Holds the reduction amount that has been normalized over the number of jump frames
WORD boost_val;

//WALKING AND RUNNING VARIABLES
WORD pl_vel_x;              //Tracks the player's x-velocity between frames
WORD pl_vel_y;              //Tracks the player's y-velocity between frames

//VARIABLES FOR CAMERAS
WORD *edge_left;
WORD *edge_right;
WORD mod_image_right;
WORD mod_image_left;

//VARIABLES FOR EVENT PLUGINS
//UBYTE grounded;             //Variable to keep compatability with other plugins that use the older 'grounded' check
BYTE run_stage;             //Tracks the stage of running based on the run type
UBYTE jump_type;            //Tracks the type of jumping, from the ground, in the air, or off the wall

//SLOPE VARIABLES
UBYTE grounded;             //Needed? Add for compatability?
UBYTE on_slope;
UBYTE slope_y;


void platform_init(void) BANKED {
    //Initialize Camera
    camera_offset_x = 0;
    camera_offset_y = 0;
    camera_deadzone_x = plat_camera_deadzone_x;
    camera_deadzone_y = PLATFORM_CAMERA_DEADZONE_Y;
    if ((camera_settings & CAMERA_LOCK_X_FLAG)){
        camera_x = (PLAYER.pos.x >> 4) + 8;
    } else{
        camera_x = 0;
    }
    if ((camera_settings & CAMERA_LOCK_Y_FLAG)){
        camera_y = (PLAYER.pos.y >> 4) + 8;
    } else{
        camera_y = 0;
    }

    //Initialize Camera Bounds
    mod_image_right = image_width - SCREEN_WIDTH;
    mod_image_left = 0;
    if (plat_camera_block & 1){
        edge_left = &scroll_x;
    }
    else{
        edge_left = &mod_image_left;
    }

    if (plat_camera_block & 2){
        edge_right = &scroll_x;
    }
    else{
        edge_right = &image_width;
    }

    
    //Make sure jumping doesn't overflow variables
    //First, check for jumping based on Frames and Initial Jump Min
    while (32000 - (plat_jump_vel/MIN(15,plat_hold_jump_max)) - plat_jump_min < 0){
        plat_hold_jump_max += 1;
    }

    //This ensures that, by itself, the plat run boost active on any single frame cannot overflow a WORD.
    //It is complemented by another check in the jump itself that works with the actual velocity. 
    if (plat_run_boost != 0){
        while((32000/plat_run_boost) < ((plat_run_vel>>8)/plat_hold_jump_max)){
            plat_run_boost--;
        }
    }

    //Normalize variables by number of frames
    jump_per_frame = plat_jump_vel / MIN(15, plat_hold_jump_max);   //jump force applied per frame in the JUMP_STATE
    jump_reduction = plat_jump_reduction / plat_hold_jump_max;      //Amount to reduce subequent jumps per frame in JUMP_STATE
    dash_dist = plat_dash_dist / plat_dash_frames;                    //Dash distance per frame in the DASH_STATE
    boost_val = plat_run_boost / plat_hold_jump_max;                  //Vertical boost from horizontal speed per frame in JUMP STATE

    //Initialize State
    plat_state = FALL_STATE;
    que_state = FALL_STATE;
    actor_attached = FALSE;
    run_stage = 0;
    nocontrol_h = 0;
    nocollide = 0;
    if (PLAYER.dir == DIR_UP || PLAYER.dir == DIR_DOWN || PLAYER.dir == DIR_NONE) {
        PLAYER.dir = DIR_RIGHT;
    }

    //Initialize other vars
    game_time = 0;
    pl_vel_x = 0;
    pl_vel_y = 4000;                //Magic number for preventing a small glitch when loading into a scene
    last_wall = 0;                  //This could be 1 bit
    hold_jump_val = plat_hold_jump_max;
    dj_val = 0;
    wj_val = plat_wall_jump_max;
    dash_end_clear = FALSE;         //could also be mixed into the collision bitmask
    jump_type = 0;
    deltaX = 0;
    deltaY = 0;

}

void platform_update(void) BANKED {
    //The switch here is to disagreggate the state code, because it had collectively grown too large to stay in a single bank.
    plat_state = que_state;
    switch(plat_state){
        case FALL_INIT:
            que_state = FALL_STATE;
        case FALL_STATE:
            fall_state();
            break;
        case GROUND_INIT:
            que_state = GROUND_STATE;
            pl_vel_y = 256;
            jump_type = 0;
            wc_val = 0;
            ct_val = plat_coyote_max; 
            dj_val = plat_extra_jumps; 
            wj_val = plat_wall_jump_max;
            jump_reduction_val = 0;
        case GROUND_STATE:
            ground_state();
            break;
        case JUMP_INIT:
            que_state = JUMP_STATE;
            hold_jump_val = plat_hold_jump_max; 
            actor_attached = FALSE;
            pl_vel_y = -plat_jump_min;
            jb_val = 0;
            ct_val = 0;
            wc_val = 0;
        case JUMP_STATE:
            jump_state();
            break;
        case DASH_INIT:
            dash_init();
            break;
        case DASH_STATE:
            dash_state();
            break;
        case LADDER_INIT:
            que_state = LADDER_STATE;
            jump_type = 0;
        case LADDER_STATE:
            ladder_state();
            break;
        case WALL_INIT:
            que_state = WALL_STATE;
            jump_type = 0;
            run_stage = 0;
        case WALL_STATE:
            wall_state();
            break;
        case KNOCKBACK_INIT:
            que_state = KNOCKBACK_STATE;
            run_stage = 0;
            jump_type = 0;
        case KNOCKBACK_STATE:
            knockback_state();
            break;
        case BLANK_INIT:
            que_state = BLANK_STATE;
            pl_vel_x = 0;
            pl_vel_y = 0;
            run_stage = 0;
            jump_type = 0;
        case BLANK_STATE:
            blank_state();
            break;
    }
}

void blank_state(void) BANKED {
    //INITIALIZE VARS
    WORD temp_y = 0;

    UBYTE p_half_width = (PLAYER.bounds.right - PLAYER.bounds.left) >> 1;
    UBYTE tile_x_mid = ((PLAYER.pos.x >> 4) + PLAYER.bounds.left + p_half_width) >> 3; 
    UBYTE tile_y = ((PLAYER.pos.y >> 4) + PLAYER.bounds.top + 1) >> 3;
    UBYTE prev_on_slope = 0;
    UBYTE old_x = 0;
    col = 0;                  
    
    //Actor Collisions
    gotoActorCol:
    {
        deltaX = 0;
        deltaY = 0;
        actor_t *hit_actor;
        hit_actor = actor_overlapping_player(FALSE);
        if (hit_actor != NULL && hit_actor->collision_group) {
            //Solid Actors
            if (hit_actor->collision_group == plat_solid_group){
                if(!actor_attached || hit_actor != last_actor){
                    if (temp_y < (hit_actor->pos.y + (hit_actor->bounds.top << 4)) && pl_vel_y >= 0){
                        //Attach to MP
                        last_actor = hit_actor;
                        mp_last_x = hit_actor->pos.x;
                        mp_last_y = hit_actor->pos.y;
                        PLAYER.pos.y = hit_actor->pos.y + (hit_actor->bounds.top << 4) - (PLAYER.bounds.bottom << 4) - 4;
                        //Other cleanup
                        pl_vel_y = 0;
                        actor_attached = TRUE;                        
                        que_state = GROUND_INIT;
                        //PLAYER bounds top seems to be 0 and counting down...
                    } else if (temp_y + (PLAYER.bounds.top << 4) > hit_actor->pos.y + (hit_actor->bounds.bottom<<4)){
                        deltaY += (hit_actor->pos.y - PLAYER.pos.y) + ((-PLAYER.bounds.top + hit_actor->bounds.bottom)<<4) + 32;
                        pl_vel_y = plat_grav;

                        if(que_state == JUMP_STATE || actor_attached){
                            que_state = FALL_INIT;
                        }

                    } else if (PLAYER.pos.x < hit_actor->pos.x){
                        deltaX = (hit_actor->pos.x - PLAYER.pos.x) - ((PLAYER.bounds.right + -hit_actor->bounds.left)<<4);
                        col = 1;
                        last_wall = 1;
                        wc_val = plat_coyote_max + 1;
                        if(!INPUT_RIGHT){
                            pl_vel_x = 0;
                        }
                        if(que_state == DASH_STATE){
                            que_state = FALL_INIT;
                        }
                    } else if (PLAYER.pos.x > hit_actor->pos.x){
                        deltaX = (hit_actor->pos.x - PLAYER.pos.x) + ((-PLAYER.bounds.left + hit_actor->bounds.right)<<4)+16;
                        col = -1;
                        last_wall = -1;
                        wc_val = plat_coyote_max  + 1;
                        if (!INPUT_LEFT){
                            pl_vel_x = 0;
                        }
                        if(que_state == DASH_STATE){
                            que_state = FALL_INIT;
                        }
                    }

                }
            } else if (hit_actor->collision_group == plat_mp_group){
                //Platform Actors
                if(!actor_attached || hit_actor != last_actor){
                    if (temp_y < hit_actor->pos.y + (hit_actor->bounds.top << 4) && pl_vel_y >= 0){
                        //Attach to MP
                        last_actor = hit_actor;
                        mp_last_x = hit_actor->pos.x;
                        mp_last_y = hit_actor->pos.y;
                        PLAYER.pos.y = hit_actor->pos.y + (hit_actor->bounds.top << 4) - (PLAYER.bounds.bottom << 4) - 4;
                        //Other cleanup
                        pl_vel_y = 0;
                        actor_attached = TRUE;                        
                        que_state = GROUND_INIT;
                    }
                }
            }
            //All Other Collisions
            player_register_collision_with(hit_actor);
        } else if (INPUT_PRESSED(INPUT_PLATFORM_INTERACT)) {
            if (!hit_actor) {
                hit_actor = actor_in_front_of_player(8, TRUE);
            }
            if (hit_actor && !hit_actor->collision_group && hit_actor->script.bank) {
                script_execute(hit_actor->script.bank, hit_actor->script.ptr, 0, 1, 0);
            }
        }
    }

    gotoTriggerCol:
    //FUNCTION TRIGGERS
    trigger_activate_at_intersection(&PLAYER.bounds, &PLAYER.pos, INPUT_UP_PRESSED);

    gotoCounters:
    //COUNTERS===============================================================
    // Counting down until dashing is ready again
    // XX Set in dash Init and checked in wall, fall, ground, and jump states
    if (dash_ready_val != 0){
        dash_ready_val -=1;
    }

    //Counting down from the max double-tap time (left is -15, right is +15)
    if (tap_val > 0){
        tap_val -= 1;
    } else if (tap_val < 0){
        tap_val += 1;
    }

    //Hone Camera after the player has dashed
    if (camera_deadzone_x > plat_camera_deadzone_x){
        camera_deadzone_x -= 1;
    }

    //State-Based Events
    if(state_events[plat_state].script_addr != 0){
        script_execute(state_events[plat_state].script_bank, state_events[plat_state].script_addr, 0, 0);
    }
}

void assign_state_script(SCRIPT_CTX * THIS) OLDCALL BANKED {
    UWORD *slot = VM_REF_TO_PTR(FN_ARG2);
    UBYTE *bank = VM_REF_TO_PTR(FN_ARG1);
    UBYTE **ptr = VM_REF_TO_PTR(FN_ARG0);
    state_events[*slot].script_bank = *bank;
    state_events[*slot].script_addr = *ptr;
}

void clear_state_script(SCRIPT_CTX * THIS) OLDCALL BANKED {
    UWORD *slot = VM_REF_TO_PTR(FN_ARG0);
    state_events[*slot].script_bank = NULL;
    state_events[*slot].script_addr = NULL;
}

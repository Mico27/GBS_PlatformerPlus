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

void dash_init(void) BANKED {
        //INITIALIZE VARS
    WORD temp_y = 0;
    col = 0;                   //tracks if there is a block left or right
    UBYTE p_half_width = (PLAYER.bounds.right - PLAYER.bounds.left) >> 1;
    UBYTE tile_x_mid = ((PLAYER.pos.x >> 4) + PLAYER.bounds.left + p_half_width) >> 3; 
    UBYTE tile_y = ((PLAYER.pos.y >> 4) + PLAYER.bounds.top + 1) >> 3;
    UBYTE prev_on_slope = 0;
    UBYTE old_x = 0;
    WORD new_x;


    //If the player is pressing a direction (but not facing a direction, ie on a wall or on a changed frame)
    if (INPUT_RIGHT){
        PLAYER.dir = DIR_RIGHT;
    }
    else if(INPUT_LEFT){
        PLAYER.dir = DIR_LEFT;
    }

    //Set new_x be the final destination of the dash (ie. the distance covered by all of the dash frames combined)
    if (PLAYER.dir == DIR_RIGHT){
        new_x = PLAYER.pos.x + (dash_dist*plat_dash_frames);
    }
    else{
        new_x = PLAYER.pos.x + (-dash_dist*plat_dash_frames);
    }

    //Dash through walls
    if(plat_dash_through == 3 && plat_dash_momentum < 2){
        dash_end_clear = true;                              //Assume that the landing spot is clear, and disable if we collide below
        UBYTE tile_start = (((PLAYER.pos.y >> 4) + PLAYER.bounds.top)    >> 3);
        UBYTE tile_end   = (((PLAYER.pos.y >> 4) + PLAYER.bounds.bottom) >> 3) + 1;     

        //Do a collision check at the final landing spot (but not all the steps in-between.)
        if (PLAYER.dir == DIR_RIGHT){
            //Don't dash off the screen to the right
            if (PLAYER.pos.x + (PLAYER.bounds.right <<4) + (dash_dist*(plat_dash_frames)) > (image_width -16) << 4){   
                dash_end_clear = false;                                     
            } else {
                UBYTE tile_xr = (((new_x >> 4) + PLAYER.bounds.right) >> 3) +1;  
                UBYTE tile_xl = ((new_x >> 4) + PLAYER.bounds.left) >> 3;   
                while (tile_xl != tile_xr){                                             //This checks all the tiles between the left bounds and the right bounds
                    while (tile_start != tile_end) {                                    //This checks all the tiles that the character occupies in height
                        if (tile_at(tile_xl, tile_start) & COLLISION_ALL) {
                                dash_end_clear = false;
                                goto initDash;                                          //Gotos are still good for breaking embedded loops.
                        }
                        tile_start++;
                    }
                    tile_start = (((PLAYER.pos.y >> 4) + PLAYER.bounds.top)    >> 3);   //Reset the height after each loop
                    tile_xl++;
                }
            }
        } else if(PLAYER.dir == DIR_LEFT) {
            //Don't dash off the screen to the left
            if (PLAYER.pos.x <= ((dash_dist*plat_dash_frames)+(PLAYER.bounds.left << 4))+(8<<4)){
                dash_end_clear = false;         //To get around unsigned position, test if the player's current position is less than the total dist.
            } else {
                UBYTE tile_xl = ((new_x >> 4) + PLAYER.bounds.left) >> 3;
                UBYTE tile_xr = (((new_x >> 4) + PLAYER.bounds.right) >> 3) +1;  

                while (tile_xl != tile_xr){   
                    while (tile_start != tile_end) {
                        if (tile_at(tile_xl, tile_start) & COLLISION_ALL) {
                                dash_end_clear = false;
                                goto initDash;
                        }
                        tile_start++;
                    }
                    tile_start = (((PLAYER.pos.y >> 4) + PLAYER.bounds.top)    >> 3);
                    tile_xl++;
                }
            }
        }
    }
    initDash:
    actor_attached = FALSE;
    camera_deadzone_x = plat_dash_deadzone;
    dash_ready_val = plat_dash_ready_max + plat_dash_frames;
    if(plat_dash_momentum < 2){
        pl_vel_y = 0;
    }
    dash_currentframe = plat_dash_frames;
    tap_val = 0;
    jump_type = 0;
    run_stage = 0;
    que_state = DASH_STATE;
   
    //State-Based Events
    if(state_events[plat_state].script_addr != 0){
        script_execute(state_events[plat_state].script_bank, state_events[plat_state].script_addr, 0, 0);
    }
}

void dash_state(void) BANKED{
    //INITIALIZE VARS
    WORD temp_y = 0;

    UBYTE p_half_width = (PLAYER.bounds.right - PLAYER.bounds.left) >> 1;
    UBYTE tile_x_mid = ((PLAYER.pos.x >> 4) + PLAYER.bounds.left + p_half_width) >> 3; 
    UBYTE tile_y = ((PLAYER.pos.y >> 4) + PLAYER.bounds.top + 1) >> 3;
    UBYTE prev_on_slope = 0;
    UBYTE old_x = 0;
    col = 0;                   

    //Movement & Collision Combined----------------------------------------------------------------------------------
    //Dashing uses much of the basic collision code. Comments here focus on the differences.
    UBYTE tile_current; //For tracking collisions across longer distances
    UBYTE tile_start = (((PLAYER.pos.y >> 4) + PLAYER.bounds.top)    >> 3);
    UBYTE tile_end   = (((PLAYER.pos.y >> 4) + PLAYER.bounds.bottom) >> 3) + 1;        
    col = 0;

    //Right Dash Movement & Collision
    if (PLAYER.dir == DIR_RIGHT){
        //Get tile x-coord of player position
        tile_current = ((PLAYER.pos.x >> 4) + PLAYER.bounds.right) >> 3;
        //Get tile x-coord of final position
        UWORD new_x = PLAYER.pos.x + (dash_dist);
        UBYTE tile_x = (((new_x >> 4) + PLAYER.bounds.right) >> 3) + 1;
        //check each space from the start of the dash until the end of the dash.
        //in the future, I should build a reversed version of this section for dashing through walls.
        //However, it's not quite as simple as reversing the direction of the check. The loops need to store the player's width and only return when there are enough spaces in a row
        while (tile_current != tile_x){
            //Don't go past camera bounds
            if ((plat_camera_block & 2) && tile_current > (camera_x + SCREEN_WIDTH_HALF - 16) >> 3){
                new_x = ((((tile_current) << 3) - PLAYER.bounds.right) << 4) -1;
                dash_currentframe == 0;
                goto endRcol;
            }
                //CHECK TOP AND BOTTOM
            while (tile_start != tile_end) {
                //Check for Collisions (if the player collides with walls)
                if(plat_dash_through != 3 || dash_end_clear == FALSE){                    
                    if (tile_at(tile_current, tile_start) & COLLISION_LEFT) {
                        //The landing space is the tile we collided on, but one to the left
                        new_x = ((((tile_current) << 3) - PLAYER.bounds.right) << 4) -1;
                        col = 1;
                        last_wall = 1;
                        wc_val = plat_coyote_max;
                        dash_currentframe == 0;
                        goto endRcol;
                    }   
                }
                //Check for Triggers at each step. If there is a trigger stop the dash (but don't run the trigger yet).
                /*if (plat_dash_through < 2){
                    if (trigger_at_tile(tile_current, tile_start) != NO_TRIGGER_COLLISON) {
                        new_x = ((((tile_current+1) << 3) - PLAYER.bounds.right) << 4);
                    }
                }*/
                tile_start++;
            }
            
            tile_start = (((PLAYER.pos.y >> 4) + PLAYER.bounds.top) >> 3);
            tile_current += 1;
        }
        endRcol: 
        if(plat_dash_momentum == 1 || plat_dash_momentum == 3){           
            //Dashes don't actually use velocity, so we will simulate the momentum by adding the full run speed. 
            pl_vel_x = plat_run_vel;
        } else{
            pl_vel_x = 0;
        }
        PLAYER.pos.x = MIN((image_width - 16) << 4, new_x);
    }

    //Left Dash Movement & Collision
    else if (PLAYER.dir == DIR_LEFT){
        //Get tile x-coord of player position
        tile_current = ((PLAYER.pos.x >> 4) + PLAYER.bounds.left) >> 3;
        //Get tile x-coord of final position
        WORD new_x = PLAYER.pos.x - (dash_dist);
        UBYTE tile_x = (((new_x >> 4) + PLAYER.bounds.left) >> 3)-1;
        //CHECK EACH SPACE FROM START TO END
        while (tile_current != tile_x){
            //Camera lock check
            if ((plat_camera_block & 1) && tile_current < (camera_x - SCREEN_WIDTH_HALF) >> 3){
                new_x = ((((tile_current + 1) << 3) - PLAYER.bounds.left) << 4)+1;
                dash_currentframe == 0;
                goto endLcol;
            }
            //CHECK TOP AND BOTTOM
            while (tile_start != tile_end) {   
                //check for walls
                if(plat_dash_through != 3 || dash_end_clear == FALSE){  //If you collide with walls
                    if (tile_at(tile_current, tile_start) & COLLISION_RIGHT) {
                        new_x = ((((tile_current + 1) << 3) - PLAYER.bounds.left) << 4)+1;
                        col = -1;
                        last_wall = -1;
                        dash_currentframe == 0;
                        wc_val = plat_coyote_max;
                        goto endLcol;
                    }
                }
                //Check for triggers
                /*if (plat_dash_through  < 2){
                    if (trigger_at_tile(tile_current, tile_start) != NO_TRIGGER_COLLISON) {
                        new_x = ((((tile_current - 1) << 3) - PLAYER.bounds.left) << 4);
                        goto endLcol;
                    }
                }*/  
                tile_start++;
            }
            tile_start = (((PLAYER.pos.y >> 4) + PLAYER.bounds.top) >> 3);
            tile_current -= 1;
        }
        endLcol: 
        if(plat_dash_momentum == 1 || plat_dash_momentum == 3){            
            pl_vel_x = -plat_run_vel;
        } else{
            pl_vel_x = 0;
        }
        PLAYER.pos.x = MAX(0, new_x);
    }

    //Vertical Movement & Collision-------------------------------------------------------------------------
    if(plat_dash_momentum >= 2){
        //If we're using vertical momentum, add gravity as normal (otherwise, vel_y = 0)
        pl_vel_y += plat_hold_grav;

        //Add Jump force
        if (INPUT_PRESSED(INPUT_PLATFORM_JUMP)){
            //Coyote Time (CT) functions here as a proxy for being grounded. 
            if (ct_val != 0){
                actor_attached = FALSE;
                pl_vel_y = -(plat_jump_min + (plat_jump_vel/2));
                jb_val = 0;
                ct_val = 0;
                jump_type = 1;
            } else if (dj_val != 0){
            //If the player is in the air, and can double jump
                dj_val -= 1;
                jump_reduction_val += jump_reduction;
                actor_attached = FALSE;
                //We can't switch states for jump frames, so approximate the height. Engine val limits ensure this doesn't overflow.
                pl_vel_y = -(plat_jump_min + (plat_jump_vel/2));    
                jb_val = 0;
                ct_val = 0;
                jump_type = 2;
            }
        } 

        //Vertical Collisions
        temp_y = PLAYER.pos.y;    
        deltaY += pl_vel_y >> 8;
        deltaY = CLAMP(deltaY, -127, 127);
        UBYTE tile_start = (((PLAYER.pos.x >> 4) + PLAYER.bounds.left)  >> 3);
        UBYTE tile_end   = (((PLAYER.pos.x >> 4) + PLAYER.bounds.right) >> 3) + 1;
        if (deltaY > 0) {

        //Moving Downward
            WORD new_y = PLAYER.pos.y + deltaY;
            UBYTE tile_y = ((new_y >> 4) + PLAYER.bounds.bottom) >> 3;
            while (tile_start != tile_end) {
                if (tile_at(tile_start, tile_y) & COLLISION_TOP) {                    
                    //Land on Floor
                    new_y = ((((tile_y) << 3) - PLAYER.bounds.bottom) << 4) - 1;
                    actor_attached = FALSE; //Detach when MP moves through a solid tile.                                   
                    pl_vel_y = 256;
                    break;
                }
                tile_start++;
            }
            PLAYER.pos.y = new_y;
        } else if (deltaY < 0) {

            //Moving Upward
            WORD new_y = PLAYER.pos.y + deltaY;
            UBYTE tile_y = (((new_y >> 4) + PLAYER.bounds.top) >> 3);
            while (tile_start != tile_end) {
                if (tile_at(tile_start, tile_y) & COLLISION_BOTTOM) {
                    new_y = ((((UBYTE)(tile_y + 1) << 3) - PLAYER.bounds.top) << 4) + 1;
                    pl_vel_y = 0;
                    break;
                }
                tile_start++;
            }
            PLAYER.pos.y = new_y;
        }
        // Clamp Y Velocity
        pl_vel_y = CLAMP(pl_vel_y,-plat_max_fall_vel, plat_max_fall_vel);
    } else{
        temp_y = PLAYER.pos.y;  
    }

    //Actor Collisions-------------------------------------------------------------------------------------------------------
    if (plat_dash_through >= 1){
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

    //ANIMATION-------------------------------------------------------------------------------------------------------
    //Currently this animation uses the 'jump' animation is it's default. 
    //This animation is currently shared by jumping, dashing, and falling. Dashing doesn't need this complexity though.
    //Here velocity overrides direction. Whereas on the ground it is the reverse. 
    if(plat_turn_control){
        if (INPUT_LEFT){
            PLAYER.dir = DIR_LEFT;
        } else if (INPUT_RIGHT){
            PLAYER.dir = DIR_RIGHT;
        } else if (pl_vel_x < 0) {
            PLAYER.dir = DIR_LEFT;
        } else if (pl_vel_x > 0) {
            PLAYER.dir = DIR_RIGHT;
        }
    }

    if (PLAYER.dir == DIR_LEFT){
        actor_set_anim(&PLAYER, ANIM_JUMP_LEFT);
    } else {
        actor_set_anim(&PLAYER, ANIM_JUMP_RIGHT);
    }

    //STATE CHANGE: No exits above.------------------------------------------------------------------------------------
    //DASH -> NEUTRAL Check
    //Colliding with a wall sets the currentframe to 0 above.
    if (dash_currentframe == 0){
        que_state = FALL_INIT;
        actor_attached = FALSE;
    } else{
        dash_currentframe -= 1;
    }
    
    //Check for final frame
    if (que_state != DASH_STATE){
        plat_state = DASH_END;
    }

    if(plat_dash_through >= 2){
        goto gotoCounters;
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

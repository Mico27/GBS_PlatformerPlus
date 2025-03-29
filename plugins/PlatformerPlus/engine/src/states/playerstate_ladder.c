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
#include "states/playerstate_ladder.h"

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

void ladder_state(void) BANKED{
    //INITIALIZE VARS
    WORD temp_y = 0;
    UBYTE tile_y = ((PLAYER.pos.y >> 4) + PLAYER.bounds.top + 1) >> 3;
    UBYTE prev_on_slope = 0;
    UBYTE old_x = 0;
    col = 0;
    
    //For positioning the player in the middle of the ladder
    UBYTE p_half_width = (PLAYER.bounds.right - PLAYER.bounds.left) >> 1;
    UBYTE tile_x_mid = ((PLAYER.pos.x >> 4) + PLAYER.bounds.left + p_half_width) >> 3; 
    pl_vel_y = 0;
    if (INPUT_UP) {
        // Climb laddder
        UBYTE tile_y = ((PLAYER.pos.y >> 4) + PLAYER.bounds.top + 1) >> 3;
        //Check if the tile above the player is a ladder tile. If so add ladder velocity
        if (IS_LADDER(tile_at(tile_x_mid, tile_y))) {
            pl_vel_y = -plat_climb_vel;
        }
    } else if (INPUT_DOWN) {
        // Descend ladder
        UBYTE tile_y = ((PLAYER.pos.y >> 4) + PLAYER.bounds.bottom + 1) >> 3;
        if (IS_LADDER(tile_at(tile_x_mid, tile_y))) {
            pl_vel_y = plat_climb_vel;
        }
    } else if (INPUT_LEFT) {
        que_state = FALL_INIT; //Assume we're going to leave the ladder state, 
        // Check if able to leave ladder on left
        UBYTE tile_start = (((PLAYER.pos.y >> 4) + PLAYER.bounds.top)    >> 3);
        UBYTE tile_end   = (((PLAYER.pos.y >> 4) + PLAYER.bounds.bottom) >> 3) + 1;
        while (tile_start != tile_end) {
            if (tile_at(tile_x_mid - 1, tile_start) & COLLISION_RIGHT) {
                que_state = LADDER_STATE; //If there is a wall, stay on the ladder.
                break;
            }
            tile_start++;
        }            
    } else if (INPUT_RIGHT) {
        que_state = FALL_INIT;
        // Check if able to leave ladder on right
        UBYTE tile_start = (((PLAYER.pos.y >> 4) + PLAYER.bounds.top)    >> 3);
        UBYTE tile_end   = (((PLAYER.pos.y >> 4) + PLAYER.bounds.bottom) >> 3) + 1;
        while (tile_start != tile_end) {
            if (tile_at(tile_x_mid + 1, tile_start) & COLLISION_LEFT) {
                que_state = LADDER_STATE;
                break;
            }
            tile_start++;
        }
    }
    PLAYER.pos.y += (pl_vel_y >> 8);

    //Animation----------------------------------------------------------------------------------------------------
    actor_set_anim(&PLAYER, ANIM_CLIMB);
    if (pl_vel_y == 0) {
        actor_stop_anim(&PLAYER);
    }


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
    //State Change-------------------------------------------------------------------------------------------------
    //Collision logic provides options for exiting to Neutral

    //Above is the default GBStudio setup. However it seems worth adding a jump-from-ladder option, at the very least to drop down.
    if (INPUT_PRESSED(INPUT_PLATFORM_JUMP)){
        que_state = FALL_INIT;
    }
    //Check for final frame
    if (que_state != LADDER_STATE){
        plat_state = LADDER_END;
    }

    //FUNCTION TRIGGERS
    trigger_activate_at_intersection(&PLAYER.bounds, &PLAYER.pos, INPUT_UP_PRESSED);

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

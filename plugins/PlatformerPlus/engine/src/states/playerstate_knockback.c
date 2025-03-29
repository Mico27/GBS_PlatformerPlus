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
#include "states/playerstate_knockback.h"

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

void knockback_state(void) BANKED {
    //INITIALIZE VARS
    WORD temp_y = 0;
    UBYTE p_half_width = (PLAYER.bounds.right - PLAYER.bounds.left) >> 1;
    UBYTE tile_x_mid = ((PLAYER.pos.x >> 4) + PLAYER.bounds.left + p_half_width) >> 3; 
    UBYTE tile_y = ((PLAYER.pos.y >> 4) + PLAYER.bounds.top + 1) >> 3;
    UBYTE prev_on_slope = 0;
    UBYTE old_x = 0;
    col = 0; 


    //Horizontal Movement----------------------------------------------------------------------------------------
    if (pl_vel_x < 0) {
            pl_vel_x += plat_air_dec;
            pl_vel_x = MIN(pl_vel_x, 0);
    } else if (pl_vel_x > 0) {
            pl_vel_x -= plat_air_dec;
            pl_vel_x = MAX(pl_vel_x, 0);
    }
    deltaX += pl_vel_x >> 8;

    //Vertical Movement--------------------------------------------------------------------------------------------
    pl_vel_y += plat_grav;
    pl_vel_y = MIN(pl_vel_y,plat_max_fall_vel);

    //Collision ---------------------------------------------------------------------------------------------------
    //Vertical Collision Checks
    deltaY += pl_vel_y >> 8;
    temp_y = PLAYER.pos.y;    
    nocollide = 0;

    {
        deltaX = CLAMP(deltaX, -127, 127);
        prev_on_slope = on_slope;
        old_x = PLAYER.pos.x;
        on_slope = FALSE;
        UBYTE tile_start = (((PLAYER.pos.y >> 4) + PLAYER.bounds.top)    >> 3);
        UBYTE tile_end   = (((PLAYER.pos.y >> 4) + PLAYER.bounds.bottom) >> 3) + 1;       
        UWORD new_x = PLAYER.pos.x + deltaX;
        
        UBYTE tile_x = 0;
        UBYTE col_mid = 0;
        
        //Edge Locking
        //If the player is past the right edge (camera or screen)
        if (new_x > (*edge_right + SCREEN_WIDTH - 16) <<4){
            //If the player is trying to go FURTHER right
            if (new_x > PLAYER.pos.x){
                new_x = PLAYER.pos.x;
                pl_vel_x = 0;
            } else {
            //If the player is already off the screen, push them back
                new_x = PLAYER.pos.x - MIN(PLAYER.pos.x - ((*edge_right + SCREEN_WIDTH - 16)<<4), 16);
            }
        //Same but for left side. This side needs a 1 tile (8px) buffer so it doesn't overflow the variable.
        } else if (new_x < *edge_left << 4){
            if (deltaX < 0){
                new_x = PLAYER.pos.x;
                pl_vel_x = 0;
            } else {
                new_x = PLAYER.pos.x + MIN(((*edge_left+8)<<4)-PLAYER.pos.x, 16);
            }
        }

        //Step-Check for collisions one tile left or right for each avatar height tile
        if (new_x > PLAYER.pos.x) {
            tile_x = ((new_x >> 4) + PLAYER.bounds.right) >> 3;

            //New Slope Stuff Part 1
            tile_y   = (((PLAYER.pos.y >> 4) + PLAYER.bounds.bottom) >> 3);
            UBYTE tile_x_mid = ((new_x >> 4) + PLAYER.bounds.left + p_half_width + 1) >> 3; 
            col_mid = tile_at(tile_x_mid, tile_y);
            if (IS_ON_SLOPE(col_mid)) {
                on_slope = col_mid;
                slope_y = tile_y;
            }
            UBYTE slope_on_y = FALSE;
            //New Slope Stuff P1 End

            while (tile_start != tile_end) {
               
                //New Slope Stuff P2
                col = tile_at(tile_x, tile_start);
                if (IS_ON_SLOPE(col)) {
                    slope_on_y = TRUE;
                }
                if (col & COLLISION_LEFT) {
                    // only ignore collisions if there is a slope on this y column somewhere
                    if (slope_on_y || tile_start == slope_y) {
                        // Right slope
                        if ((IS_ON_SLOPE(on_slope) && IS_SLOPE_RIGHT(on_slope)) ||
                            (IS_ON_SLOPE(prev_on_slope) && IS_SLOPE_RIGHT(prev_on_slope))
                            )
                            {
                            if (tile_start <= slope_y) {
                                tile_start++;
                                continue;
                            }
                        }
                    }
                    if (slope_on_y) {
                        // Left slope
                        if ((IS_ON_SLOPE(on_slope) && IS_SLOPE_LEFT(on_slope)) ||
                            (IS_ON_SLOPE(prev_on_slope) && IS_SLOPE_LEFT(prev_on_slope))
                            )
                            {
                            if (tile_start >= slope_y) {
                                tile_start++;
                                continue;
                            }
                        }
                    }
                //End New Slope Stuff P2
                new_x = (((tile_x << 3) - PLAYER.bounds.right) << 4) - 1;
                pl_vel_x = 0;
                col = 1;
                last_wall = 1;
                wc_val = plat_coyote_max + 1;
                break;
                }
                tile_start++;
            }
        } else if (new_x < PLAYER.pos.x) {
            //New Slope 3
            tile_x = ((new_x >> 4) + PLAYER.bounds.left) >> 3;
            tile_y   = (((PLAYER.pos.y >> 4) + PLAYER.bounds.bottom) >> 3);
            UBYTE tile_x_mid = ((new_x >> 4) + PLAYER.bounds.left + p_half_width + 1) >> 3; 
            col_mid = tile_at(tile_x_mid, tile_y);
            if (IS_ON_SLOPE(col_mid)) {
                on_slope = col_mid;
                slope_y = tile_y;
            }

            tile_start = (((PLAYER.pos.y >> 4) + PLAYER.bounds.top)    >> 3);
            UBYTE slope_on_y = FALSE;
            //End New Slope 3

            while (tile_start != tile_end) {
            //New Slope 4
                col = tile_at(tile_x, tile_start);
                if (IS_ON_SLOPE(col)) {
                    slope_on_y = TRUE;
                }

                if (col & COLLISION_RIGHT) {
                    // only ignore collisions if there is a slope on this y column somewhere
                    if (slope_on_y || tile_start == slope_y) {
                        // Left slope
                        if ((IS_ON_SLOPE(on_slope) && IS_SLOPE_LEFT(on_slope)) ||
                            (IS_ON_SLOPE(prev_on_slope) && IS_SLOPE_LEFT(prev_on_slope))                            
                            )
                            {
                            if (tile_start <= slope_y) {
                                tile_start++;
                                continue;
                            }
                        }
                    }
                    if (slope_on_y) {
                        // Right slope
                        if ((IS_ON_SLOPE(on_slope) && IS_SLOPE_RIGHT(on_slope)) ||
                            (IS_ON_SLOPE(prev_on_slope) && IS_SLOPE_RIGHT(prev_on_slope))
                            )
                            {
                            if (tile_start >= slope_y) {
                                tile_start++;
                                continue;
                            }
                        }
                    }
                    new_x = ((((tile_x + 1) << 3) - PLAYER.bounds.left) << 4) + 1;
                    pl_vel_x = 0;
                    col = -1;
                    last_wall = -1;
                    wc_val = plat_coyote_max + 1;
                    break;
                }
                tile_start++;
            }
        }
        PLAYER.pos.x = new_x;
    }

    gotoYCol:
    {
        //FUNCTION Y COLLISION
        deltaY = CLAMP(deltaY, -127, 127);

        //New Y Slopes 1
        UBYTE prev_grounded = grounded;
        UWORD old_y = PLAYER.pos.y;
        grounded = FALSE;
        // 1 frame leniency of grounded state if we were on a slope last frame
        if (prev_on_slope) grounded = TRUE;
        //End New Y Slopes 1

        UBYTE tile_start = (((PLAYER.pos.x >> 4) + PLAYER.bounds.left)  >> 3);
        UBYTE tile_end   = (((PLAYER.pos.x >> 4) + PLAYER.bounds.right) >> 3) + 1;
        if (deltaY > 0) {
            //Moving Downward
            WORD new_y = PLAYER.pos.y + deltaY;

            //New Slope Y 2
            UBYTE tile_y = (((PLAYER.pos.y >> 4) + PLAYER.bounds.bottom) >> 3) - 1;
            UBYTE new_tile_y = ((new_y >> 4) + PLAYER.bounds.bottom) >> 3;
            // If previously grounded and gravity is not enough to pull us down to the next tile, manually check it for the next slope
            // This prevents the "animation glitch" when going down slopes
            if (prev_grounded && new_tile_y == (tile_y + 1)) new_tile_y += 1;
            UWORD x_mid_coord = ((PLAYER.pos.x >> 4) + PLAYER.bounds.left + p_half_width + 1);
            while (tile_y <= new_tile_y) {
                UBYTE col = tile_at(x_mid_coord >> 3, tile_y);
                UWORD tile_x_coord = (x_mid_coord >> 3) << 3;
                UWORD x_offset = x_mid_coord - tile_x_coord;
                UWORD slope_y_coord = 0;
                if (IS_ON_SLOPE(col)) {
                    if ((col & COLLISION_SLOPE) == COLLISION_SLOPE_45_RIGHT) {
                        slope_y_coord = (((tile_y << 3) + (8 - x_offset) - PLAYER.bounds.bottom) << 4) - 1;
                    } else if ((col & COLLISION_SLOPE) == COLLISION_SLOPE_225_RIGHT_BOT) {
                        slope_y_coord = (((tile_y << 3) + (8 - (x_offset >> 1)) - PLAYER.bounds.bottom) << 4) - 1;
                    } else if ((col & COLLISION_SLOPE) == COLLISION_SLOPE_225_RIGHT_TOP) {
                        slope_y_coord = (((tile_y << 3) + (4 - (x_offset >> 1)) - PLAYER.bounds.bottom) << 4) - 1;
                    }

                    else if ((col & COLLISION_SLOPE) == COLLISION_SLOPE_45_LEFT) {
                        slope_y_coord = (((tile_y << 3) + (x_offset) - PLAYER.bounds.bottom) << 4) - 1;
                    } else if ((col & COLLISION_SLOPE) == COLLISION_SLOPE_225_LEFT_BOT) {
                        slope_y_coord = (((tile_y << 3) + (x_offset >> 1) - PLAYER.bounds.bottom + 4) << 4) - 1;
                    } else if ((col & COLLISION_SLOPE) == COLLISION_SLOPE_225_LEFT_TOP) {
                        slope_y_coord = (((tile_y << 3) + (x_offset >> 1) - PLAYER.bounds.bottom) << 4) - 1;
                    }
                }

                if (slope_y_coord) {
                    // If going downwards into a slope, don't snap to it unless we've actually collided
                    if (!prev_grounded && slope_y_coord > new_y) {
                        tile_y++;
                        continue;
                    }
                    // If we are moving up a slope, check for top collision
                    UBYTE slope_top_tile_y = (((slope_y_coord >> 4) + PLAYER.bounds.top) >> 3);
                    while (tile_start != tile_end) {
                        if (tile_at(tile_start, slope_top_tile_y) & COLLISION_BOTTOM) {
                            pl_vel_y = 0;
                            pl_vel_x = 0;
                            PLAYER.pos.y = old_y;
                            PLAYER.pos.x = old_x;
                            grounded = TRUE;
                            on_slope = col;
                            slope_y = tile_y;
                            goto gotoActorCol;
                        }
                        tile_start++;
                    }

                    PLAYER.pos.y = slope_y_coord;
                    pl_vel_y = 0;
                    grounded = TRUE;
                    if(plat_state == GROUND_STATE){
                            que_state = GROUND_STATE; 
                            pl_vel_y = 256;
                    } else if(plat_state == GROUND_INIT){
                            que_state = GROUND_STATE;
                    } else {que_state = GROUND_INIT;}
                    on_slope = col;
                    slope_y = tile_y;
                    goto gotoActorCol;
                }
                tile_y++;
            }

            tile_start = (((PLAYER.pos.x >> 4) + PLAYER.bounds.left)  >> 3);
            tile_end   = (((PLAYER.pos.x >> 4) + PLAYER.bounds.right) >> 3) + 1;
            tile_y = ((new_y >> 4) + PLAYER.bounds.bottom) >> 3;
            //End New Slope Y 2


            if (nocollide == 0){
                //Check collisions from left to right with the bottom of the player
                while (tile_start != tile_end) {
                    if (tile_at(tile_start, tile_y) & COLLISION_TOP) {
                            while (tile_start != tile_end) {
                                if (tile_at(tile_start, tile_y) & COLLISION_BOTTOM){
                                    //Escape two levels of looping.
                                    goto land;
                                }
                            tile_start++;
                            }

                        //Land on Floor
                        land:
                        new_y = ((((tile_y) << 3) - PLAYER.bounds.bottom) << 4) - 1;
                        actor_attached = FALSE; //Detach when MP moves through a solid tile.
                        //The distinction here is used so that we can check the velocity when the player hits the ground.
                        if(plat_state == GROUND_STATE){
                            que_state = GROUND_STATE; 
                            pl_vel_y = 256;
                        } else if(plat_state == GROUND_INIT){
                            que_state = GROUND_STATE;
                        } else {que_state = GROUND_INIT;}
                        break;
                    }
                    tile_start++;
                }
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
                    //MP Test: Attempting stuff to stop the player from continuing upward
                    if(actor_attached){
                        temp_y = last_actor->pos.y;
                        if (last_actor->bounds.top > 0){
                            temp_y += last_actor->bounds.top + last_actor->bounds.bottom << 5;
                        }
                        new_y = temp_y;
                    }
                    ct_val = 0;
                    que_state = FALL_INIT;
                    actor_attached = FALSE;
                    break;
                }
                tile_start++;
            }
            PLAYER.pos.y = new_y;
        }
    }

    //FUNCTION ACTOR CHECK
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

    //STATE MACHINE 2
    if (que_state == GROUND_INIT){
        pl_vel_y = 256;
    }
    que_state = KNOCKBACK_STATE;

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

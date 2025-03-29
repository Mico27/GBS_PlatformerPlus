#ifndef STATE_PLATFORM_H
#define STATE_PLATFORM_H

#include <gb/gb.h>

void platform_init(void) BANKED;
void platform_update(void) BANKED;

void fall_state(void) BANKED;
void ground_state(void) BANKED;
void jump_state(void) BANKED;
void dash_init(void) BANKED;
void dash_state(void) BANKED;
void ladder_state(void) BANKED;
void wall_state(void) BANKED;
void knockback_state(void) BANKED;
void blank_state(void) BANKED;

void basic_anim(void) BANKED;
void wall_check(void) BANKED;
void ladder_check(void) BANKED;


UBYTE drop_press(void) BANKED;

typedef enum {              //Datatype for tracking states
    FALL_INIT = 0,
    FALL_STATE = 1,
    FALL_END = 2,
    GROUND_INIT = 3,
    GROUND_STATE = 4,
    GROUND_END = 5,
    JUMP_INIT = 6,
    JUMP_STATE = 7,
    JUMP_END = 8,
    DASH_INIT = 9,
    DASH_STATE = 10,
    DASH_END = 11,
    LADDER_INIT = 12,
    LADDER_STATE = 13,
    LADDER_END = 14,
    WALL_INIT = 15,
    WALL_STATE = 16,
    WALL_END = 17,
    KNOCKBACK_INIT = 18,
    KNOCKBACK_STATE = 19,
    BLANK_INIT = 20,
    BLANK_STATE = 21
}  pStates;

typedef struct script_state_t {
    UBYTE script_bank;
    UBYTE *script_addr;
} script_state_t;


extern WORD pl_vel_x;
extern WORD pl_vel_y;
extern WORD plat_min_vel;
extern WORD plat_walk_vel;
extern WORD plat_run_vel;
extern WORD plat_climb_vel;
extern WORD plat_walk_acc;
extern WORD plat_run_acc;
extern WORD plat_dec;
extern WORD plat_jump_vel;
extern WORD plat_grav;
extern WORD plat_hold_grav;
extern WORD plat_max_fall_vel;

extern BYTE plat_camera_deadzone_x;
extern UBYTE plat_camera_block;
extern UBYTE plat_drop_through;   
extern UBYTE plat_mp_group;        
extern UBYTE plat_solid_group;    
extern WORD plat_jump_min;        
extern UBYTE plat_hold_jump_max; 
extern UBYTE plat_extra_jumps;     
extern WORD plat_jump_reduction;  
extern UBYTE plat_coyote_max;     
extern UBYTE plat_buffer_max;     
extern UBYTE plat_wall_jump_max;   
extern UBYTE plat_wall_slide;      
extern WORD plat_wall_grav;       
extern WORD plat_wall_kick;        
extern UBYTE plat_float_input;     
extern WORD plat_float_grav;      
extern UBYTE plat_air_control; 
extern UBYTE plat_turn_control;    
extern WORD plat_air_dec;        
extern UBYTE plat_run_type;      
extern WORD plat_turn_acc;        
extern UBYTE plat_run_boost;       
extern UBYTE plat_dash;            
extern UBYTE plat_dash_style;      
extern UBYTE plat_dash_momentum;  
extern UBYTE plat_dash_through;   
extern WORD plat_dash_dist;       
extern UBYTE plat_dash_frames;
extern UBYTE plat_dash_ready_max; 

#endif

#include "states/platform.h"

void dash_init(void) BANKED;
void dash_state(void) BANKED;

extern pStates plat_state;
extern pStates que_state;
extern script_state_t state_events[21];

extern UBYTE nocontrol_h;          
extern UBYTE nocollide;            
extern WORD deltaX;
extern WORD deltaY;

extern UBYTE ct_val;               //Coyote Time Variable
extern UBYTE jb_val;               //Jump Buffer Variable
extern UBYTE wc_val;               //Wall Coyote Time Variable
extern UBYTE hold_jump_val;        //Jump input hold variable
extern UBYTE dj_val;               //Current double jump
extern UBYTE wj_val;               //Current wall jump

extern BYTE last_wall;             //tracks the last wall the player touched
extern BYTE col;

extern UBYTE dash_ready_val;       //tracks the current amount before the dash is ready
extern WORD dash_dist;             //Takes overall dash distance and holds the amount per-frame
extern UBYTE dash_currentframe;    //Tracks the current frame of the overall dash
extern BYTE tap_val;               //Number of frames since the last time left or right button was tapped
extern UBYTE dash_end_clear;       //Used to store the result of whether the end-position of a dash is empty

extern actor_t *last_actor;        //The last actor the player hit, and that they were attached to
extern UBYTE actor_attached;       //Keeps track of whether the player is currently on an actor and inheriting its movement
extern WORD mp_last_x;             //Keeps track of the pos.x of the attached actor from the previous frame
extern WORD mp_last_y;             //Keeps track of the pos.y of the attached actor from the previous frame

extern WORD jump_reduction_val;    //Holds a temporary jump velocity reduction
extern WORD jump_per_frame;        //Holds a jump amount that has been normalized over the number of jump frames
extern WORD jump_reduction;        //Holds the reduction amount that has been normalized over the number of jump frames
extern WORD boost_val;

//WALKING AND RUNNING VARIABLES
extern WORD pl_vel_x;              //Tracks the player's x-velocity between frames
extern WORD pl_vel_y;              //Tracks the player's y-velocity between frames

//VARIABLES FOR CAMERAS
extern WORD *edge_left;
extern WORD *edge_right;
extern WORD mod_image_right;
extern WORD mod_image_left;

//VARIABLES FOR EVENT PLUGINS
//UBYTE grounded;             //Variable to keep compatability with other plugins that use the older 'grounded' check
extern BYTE run_stage;             //Tracks the stage of running based on the run type
extern UBYTE jump_type;            //Tracks the type of jumping, from the ground, in the air, or off the wall

//SLOPE VARIABLES
extern UBYTE grounded;             //Needed? Add for compatability?
extern UBYTE on_slope;
extern UBYTE slope_y;

extern UBYTE plat_dash_deadzone; 
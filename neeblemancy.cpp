#include <fenv.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <iostream>




//TODO: Improve neeble movement/chasing
//  - Camping
//TODO: 4-player
//TODO: make game more efficient
//TODO: More/different abilities
//  - Charge up shots to make them faster
//  - Minimaps


/**
static void
fpe_signal_handler( int sig, siginfo_t *sip, void *scp )
{
    int fe_code = sip->si_code;

    printf("In signal handler : ");

    if (fe_code == ILL_ILLTRP)
        printf("Illegal trap detected\n");
    else
        printf("Code detected : %d\n",fe_code);

    abort();
}

void enable_floating_point_exceptions()
{
    fenv_t env;
    fegetenv(&env);

    env.__fpcr = env.__fpcr | __fpcr_trap_invalid;
    fesetenv(&env);

    struct sigaction act;
    act.sa_sigaction = fpe_signal_handler;
    sigemptyset (&act.sa_mask);
    act.sa_flags = SA_SIGINFO;
    sigaction(SIGILL, &act, NULL);
}

**/

#define COW_PATCH_FRAMERATE

#include "include.cpp"
#include <time.h>

#undef TINY_VAL
#define TINY_VAL            0.000001
#define KEY_SCHEME_1        "wasdeq"
#define KEY_SCHEME_2        "okl;ip"
#define GHOST_TIME          0.65
#define NUM_LIVES           3
#define PARTS_PER_PLAYER    50
#define NUM_SKINS           4

int TOTAL_NEEBLES = 4;

real P1_RADIUS = 0.25;
real P1_SPEED = 7.5;
real P1_TURN = 5;

real global_time = 0.0;
real SHOW_CONTROLLERS = 9.0;

real WORLD_SIZE;
int GRID_SIDE, NUM_PLAYERS;
short* MAP;
int2* SPAWN_POINTS;
real SPAWN_THETAS[4] = {180, 0, 90, -90};

bool MODE_2D;
mat4 C_PLAYERS[4];

int WORLD_SKIN = 1;

#define HEDGE_WALL          V2(0.0, 128.0)
#define HEDGE_SHADE_WALL    V2(16.0, 128.0)
#define BLUE_WALL           V2(96.0, 32.0)
#define BLUE_SHADE_WALL     V2(64.0, 32.0)
#define PURPLE_WALL         V2(96.0, 96.0)
#define PURPLE_SHADE_WALL   V2(64.0, 96.0)
#define GREEN_WALL          V2(32.0, 64.0)
#define GREEN_SHADE_WALL    V2(0.0, 64.0)
#define RED_WALL            V2(32.0, 32.0)
#define RED_SHADE_WALL      V2(0.0, 32.0)
#define YELLOW_WALL         V2(96.0, 64.0)
#define YELLOW_SHADE_WALL   V2(64.0, 64.0)
#define STONE_FLOOR         V2(0.0, 112.0)
#define LAVA                V2(32.0, 128.0)
#define LAVA_BED            V2(32.0, 112.0)

vec2 wall_tex_list[10] = {BLUE_WALL, BLUE_SHADE_WALL, RED_WALL, RED_SHADE_WALL, YELLOW_WALL, YELLOW_SHADE_WALL, GREEN_WALL, GREEN_SHADE_WALL, PURPLE_WALL, PURPLE_SHADE_WALL};

#define GM_CLASSIC 0
#define GM_HOTEL 1
int GAMEMODE;
bool TEAM_MODE;

vec2 get_grid_pos(vec2 world_pos) {
    return V2((world_pos.x / WORLD_SIZE)+(real)GRID_SIDE*0.5, (world_pos.y / WORLD_SIZE)+(real)GRID_SIDE*0.5);
}

vec2 get_world_pos(vec2 grid_pos) {
    return V2((grid_pos.x - (real)GRID_SIDE*0.5)*WORLD_SIZE , (grid_pos.y - (real)GRID_SIDE*0.5)*WORLD_SIZE);
}

struct player {
    int p_num;
    short skin, turn, cell_x, cell_y;
    int lives = NUM_LIVES;
    int part_index_0; //particles
    int part_index_c = 0;
    bool dead = false, ghost = false, alert = false, RT_armed = true, LT_armed = true;
    int controller = -1;
    real radius, speed, theta, pitch;
    real death_timer = 0.0, ghost_timer = GHOST_TIME;
    vec2 world_pos, grid_pos;
    vec4 wall_coll = V4(100.0, -100.0, -100.0, 100.0);
    short neebles_stored = TOTAL_NEEBLES;
    //pfp, ghost, alert, neeb
    vec2 hud_tl[4] = {V2(9), V2(9), V2(9)};
};

player player_list[4];

struct neeble {
    player *owner;
    vec2 world_pos;
    vec2 grid_pos;
    real vert_pos, wait;
    bool ball_form, dead;
    vec3 ball_velocity;
    short skin, pose;
    short cell_x, cell_y;
    vec2 target_cell = V2(-1), previous_cell = V2(-1), super_previous_cell = V2(-1), previous_move = V2(0.0);

    neeble(){
        dead = true;
        owner = 0;
    }

    neeble(player *p_0, vec3 ball_v){
        owner = p_0;
        world_pos = p_0->world_pos;
        grid_pos = p_0->grid_pos;
        cell_x = (int)(grid_pos.x);
        cell_y = (int)(grid_pos.y);
        vert_pos = 0.75;
        wait = 0.0;
        ball_form = true;
        dead = false;
        ball_velocity = ball_v;
        skin = owner->skin;
        pose = 0;
    }
};

neeble neeble_list[20];

struct particle {
    // 0    SINGLE PIXEL
    bool active;
    int type;
    int skin;
    real lifespan;
    vec3 position;
    vec3 velocity;

    particle() {
        active = false;
    }

    particle(int t, int s, real l, vec3 p, vec3 v) {
        active = true;
        type = t;
        skin = s;
        lifespan = l;
        position = p;
        velocity = v;
    }

    particle(int t, int s, real l, vec3 p) {
        active = true;
        type = t;
        skin = s;
        lifespan = l;
        position = p;
        velocity = V3(0.0);
    }
};

particle particle_list[PARTS_PER_PLAYER * 4];

void single_hanging_particle(player* owner, vec3 position) {
    if (owner->part_index_c > PARTS_PER_PLAYER - 1 + owner->part_index_0) owner->part_index_c = owner->part_index_0;
    particle_list[owner->part_index_c] = {0, owner->skin, 1.5, position, V3(0.0, -0.005, 0.0)};
    owner->part_index_c += 1;
}

void update_particles() {
    for (int i = 0; i < NUM_PLAYERS * PARTS_PER_PLAYER; i++) {
        if (particle_list[i].active) {
            particle_list[i].position += particle_list[i].velocity;
            particle_list[i].lifespan -= 0.0167;
        }
        if (particle_list[i].lifespan <= 0.0) particle_list[i].active = false;
    }
}

struct spawn{
    int cell_x, cell_y;
    real theta;
    vec2 grid_pos;

    spawn(){
        cell_x = -1;
        cell_y = -1;
        grid_pos = V2(-1);
    }
    spawn(int x, int y){
        cell_x = x;
        cell_y = y;
        grid_pos = V2(x+0.5, y+0.5);
    }
};

spawn spawn_list[4];

void draw_square(vec2 btm_lft, real side, vec3 color) {
    eso_color(color);
    btm_lft *= WORLD_SIZE;
    real side_r = (real)side * WORLD_SIZE;
    eso_vertex(btm_lft);
    eso_vertex(btm_lft.x + side_r, btm_lft.y);
    eso_vertex(btm_lft.x + side_r, btm_lft.y + side_r);
    eso_vertex(btm_lft.x, btm_lft.y + side_r);
}

//V4 collisions format: x=UP, y=DOWN, z=LEFT, w=RIGHT
vec4 get_wall_collisions(player* p_0) {
    vec4 collisions = V4(100.0, -100.0, -100.0, 100.0);
    short grid_x = p_0->cell_x;
    short grid_y = p_0->cell_y;
    real size_adjust = (p_0->radius + 0.01) / WORLD_SIZE;

    //get upper world edge dist if in range
    if(grid_y == GRID_SIDE-1) {
        collisions.x = GRID_SIDE - (p_0->grid_pos).y - size_adjust;
    } //get upper grid space dist if there is a pillar
    else if(MAP[(GRID_SIDE - grid_y - 2) * GRID_SIDE + grid_x] && !(p_0->ghost && MAP[(GRID_SIDE - grid_y - 2) * GRID_SIDE + grid_x] == 3)) {
        collisions.x = grid_y + (1.0) - (p_0->grid_pos).y - size_adjust;
    }

    //get lower world edge dist if in range
    if(grid_y == 0) {
        collisions.y = - (p_0->grid_pos).y + size_adjust;
    } //get lower grid space dist if there is a pillar
    else if(MAP[(GRID_SIDE - grid_y) * GRID_SIDE + grid_x] && !(p_0->ghost && MAP[(GRID_SIDE - grid_y) * GRID_SIDE + grid_x] == 3)) {
        collisions.y = grid_y - (p_0->grid_pos).y + size_adjust;
    }

    //get left world edge dist if in range
    if(grid_x == 0) {
        collisions.z = -(p_0->grid_pos).x + size_adjust;
    } //get left grid space dist if there is a pillar
    else if (MAP[(GRID_SIDE - grid_y - 1) * GRID_SIDE + grid_x - 1] && !(p_0->ghost && MAP[(GRID_SIDE - grid_y - 1) * GRID_SIDE + grid_x - 1] == 3)) {
        collisions.z = grid_x - (p_0->grid_pos).x + size_adjust;
    }

    //get right world edge dist if in range
    if(grid_x == GRID_SIDE-1) {
        collisions.w = GRID_SIDE - (p_0->grid_pos).x - size_adjust;
    } //get right grid space dist if there is a pillar
    else if (MAP[(GRID_SIDE - grid_y - 1) * GRID_SIDE + grid_x + 1] && !(p_0->ghost && MAP[(GRID_SIDE - grid_y - 1) * GRID_SIDE + grid_x + 1] == 3)) {
        collisions.w = grid_x + (1.0) - (p_0->grid_pos).x - size_adjust;
    }

    return collisions * WORLD_SIZE;
}

vec2 update_for_corner(player* p_0, vec2 world_move) {
    vec2 new_pos = p_0->grid_pos + world_move / WORLD_SIZE;
    vec2 corner;
    bool up = new_pos.y - p_0->cell_y >= 0.5; 
    bool right = new_pos.x - p_0->cell_x >= 0.5;
    corner = V2(2*right - 1, 2*up -1);
    vec2 new_pos_collision = new_pos + corner * (p_0->radius / WORLD_SIZE)*0.85;

    corner = V2(p_0->cell_x + right, p_0->cell_y + up);
    bool corner_coll = MAP[(GRID_SIDE - (int)(new_pos_collision.y) - 1) * GRID_SIDE + (int)(new_pos_collision.x)];
    corner_coll &= !(p_0->ghost && MAP[(GRID_SIDE - (int)(new_pos_collision.y) - 1) * GRID_SIDE + (int)(new_pos_collision.x)] == 3);

    if (!corner_coll) return world_move;
    else if(ABS(new_pos_collision.y - corner.y) > ABS(new_pos_collision.x - corner.x)) {
        world_move.x = right ? MIN(world_move.x, (corner.x + TINY_VAL - new_pos_collision.x)*WORLD_SIZE)
                            : MAX(world_move.x, (corner.x - TINY_VAL - new_pos_collision.x)*WORLD_SIZE);
    }
    else world_move.y = up ? MIN(world_move.y, (corner.y + TINY_VAL - new_pos_collision.y)*WORLD_SIZE)
                            : MAX(world_move.y, (corner.y - TINY_VAL - new_pos_collision.y)*WORLD_SIZE);
    
    return world_move;
}

void kill_neeble(neeble* n_0) {
    n_0->dead = true;
}

void kill_player(player* p_0) {
    p_0->dead = true;
    p_0->lives -= 1;
    p_0->alert = false;
    p_0->death_timer = 3.0;
    for (int i = 0; i < TOTAL_NEEBLES; i++) {
        kill_neeble(&neeble_list[(p_0->p_num - 1)*TOTAL_NEEBLES + i]);
    }
    p_0->neebles_stored = TOTAL_NEEBLES;
}

void spawn_player(player* p_0) {
    int spawn_scores[4] = {0, 0, 0, 0};
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < NUM_PLAYERS; j++) {
            if (player_list[j].dead == false) {
                int dist_to_player = ABS(norm(player_list[j].grid_pos - spawn_list[i].grid_pos));
                for (int k = 0; k < 5; k++) {
                    if (dist_to_player < 12 - 2*k) spawn_scores[i] -= (int)(k * (p_0->skin == player_list[j].skin ? 0.5 : 1.0));
                    else break;
                }
            }
        }
    }
    int max_score = -200;
    int winner = -1;
    for (int i = 0; i < 4; i ++) {
        if (spawn_scores[i] > max_score || (spawn_scores[i] == max_score && rand() % 3 == 0)) {
            max_score = spawn_scores[i];
            winner = i;
        }
    }
    for(int i = 0; i < NUM_PLAYERS * TOTAL_NEEBLES; i++) {
        int dist_to_neeble = ABS(norm(neeble_list[i].grid_pos - spawn_list[winner].grid_pos));
        if (dist_to_neeble < 5) kill_neeble(&(neeble_list[i]));
    }

    p_0->theta = spawn_list[winner].theta;
    p_0->grid_pos = spawn_list[winner].grid_pos;
    p_0->world_pos = get_world_pos(p_0->grid_pos);
    p_0->cell_x = spawn_list[winner].cell_x;
    p_0->cell_y = spawn_list[winner].cell_y;
    p_0->dead = false;
    p_0->ghost_timer = GHOST_TIME;
}

void update_hud(player* p_0) {
    //pfp
    p_0->hud_tl[0] = V2(82, 46 + 48*(p_0->skin) - (p_0->dead ? 11 : 0));
    //ghost
    if (p_0->ghost_timer < 0) p_0->hud_tl[1] = V2(256 - 14 - 13 * (int)(5 * (-p_0->ghost_timer / (GHOST_TIME))), 256 - 41);
    else p_0->hud_tl[1] = V2(256 - 14 - 13 * (!p_0->ghost ? (8-TINY_VAL-(int)(8 * (p_0->ghost_timer / GHOST_TIME)))
                                                        : (int)(5 * (p_0->ghost_timer / GHOST_TIME))),
                        256 - (p_0->ghost ? 28 : 15));
    //neeb
    p_0->hud_tl[2] = V2(95, 35 + 48*p_0->skin);
}

void update_player(player* p_0) {
    if (p_0->dead) {
        p_0->death_timer -= 0.0167;
        if (p_0->death_timer <= 0 && p_0->lives > 0) {
            spawn_player(p_0);
        }
    }
    else {
        p_0->alert = false;
        p_0->wall_coll = get_wall_collisions(p_0);
        vec2 movement = V2(0.0);
        bool creating_neeble = false;
        bool destroying_neebles = false;

        //XBOX CONTROLS
        if (p_0->controller > 1) {
            int glfw_controller = p_0->controller - 2;
            GLFWgamepadstate state;
            if (glfwGetGamepadState(glfw_controller, &state)) {
                if (p_0->ghost_timer >= GHOST_TIME) {
                    if (state.axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER] > 0.95 && p_0->LT_armed) {
                        p_0->ghost = true;
                        p_0->LT_armed = false;
                    }
                    else if (state.buttons[GLFW_GAMEPAD_BUTTON_LEFT_BUMPER]) p_0->ghost = true;
                }
                if (state.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER] < 0.2) p_0->LT_armed = true;

                real look_ud = state.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y];
                real look_lr = state.axes[GLFW_GAMEPAD_AXIS_RIGHT_X];
                if (ABS(look_lr) > 0.2) {
                    look_lr = CLAMP((look_lr * 1.25) - 0.22 * (look_lr < 0 ? -1 : 1), -1.0, 1.0);
                    p_0->theta += look_lr * p_0->turn;
                }
                if (ABS(look_ud) > 0.2) {
                    look_ud = CLAMP((look_ud * 1.25) - 0.22 * (look_ud < 0 ? -1 : 1), -1.0, 1.0);
                    p_0->pitch -= look_ud * p_0->turn * 0.4;
                    p_0->pitch = CLAMP(p_0->pitch, -20, 20);
                }

                if (p_0->theta >= 360) p_0->theta -= 360;
                else if (p_0->theta < 0) p_0->theta += 360;

                real strafe_fb = state.axes[GLFW_GAMEPAD_AXIS_LEFT_Y];
                real strafe_lr = state.axes[GLFW_GAMEPAD_AXIS_LEFT_X];
                real speed_current = p_0->speed;
                if (ABS(strafe_lr) > 0.2) {
                    strafe_lr = CLAMP((strafe_lr * 1.25) - 0.22 * (strafe_lr < 0 ? -1 : 1), -1.0, 1.0);
                    int perp = p_0->theta + 90;
                    movement += strafe_lr * speed_current * 0.01 *
                                V2(sin(RAD(perp)), cos(RAD(perp)));
                }

                speed_current *= (p_0->ghost ? 2 : 1);
                if (ABS(strafe_fb) > 0.2) {
                    strafe_fb = CLAMP((strafe_fb * 1.25) - 0.22 * (strafe_fb < 0 ? -1 : 1), -1.0, 1.0);
                    movement -= strafe_fb * speed_current * 0.01 *
                                V2(sin(RAD(p_0->theta)), cos(RAD(p_0->theta)));
                }
                if (norm(movement) > speed_current*0.01) {
                    movement *= (speed_current*0.01)/norm(movement);
                }

                if (!MODE_2D) {
                    if (state.buttons[GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER]) destroying_neebles = true;
                    if (state.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER] > 0.95 && p_0->RT_armed) {
                        creating_neeble = true;
                        p_0->RT_armed = false;
                    }
                    if (state.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER] < 0.2) p_0->RT_armed = true;
                }
            }
        }
        //KEYBOARD CONTROLS
        else {
            p_0->pitch = 0;
            const char *key_scheme = p_0->controller == 0 ? KEY_SCHEME_1 : KEY_SCHEME_2;
            if(p_0->ghost_timer >= GHOST_TIME && globals.key_pressed[(p_0->controller == 0 ? COW_KEY_SHIFT_LEFT : COW_KEY_SHIFT_RIGHT)]) {
                p_0->ghost = true;
            }
            real speed_current = p_0->speed * (p_0->ghost ? 2 : 1);
            if(globals.key_held[key_scheme[1]] != globals.key_held[key_scheme[3]]) {
                    if(globals.key_held[key_scheme[1]]) p_0->theta -= p_0->turn;
                    if(globals.key_held[key_scheme[3]]) p_0->theta += p_0->turn;

                    if(globals.key_pressed[key_scheme[1]]) p_0->theta += 0.5 * p_0->turn;
                    if(globals.key_pressed[key_scheme[3]]) p_0->theta -= 0.5 * p_0->turn;
            }

            if (p_0->theta >= 360) p_0->theta -= 360;
            else if (p_0->theta < 0) p_0->theta += 360;

            if(globals.key_held[key_scheme[0]] != globals.key_held[key_scheme[2]]) {
                if(globals.key_held[key_scheme[0]]) {
                    movement = V2(speed_current * 0.01 * sin(RAD(p_0->theta)),
                                speed_current * 0.01 * cos(RAD(p_0->theta)));
                }
                if(globals.key_held[key_scheme[2]]) {
                    movement = V2(speed_current * -0.01 * sin(RAD(p_0->theta)), speed_current * -0.01 * cos(RAD(p_0->theta)));
                }
            }
            if(!MODE_2D) {
                if(globals.key_pressed[key_scheme[5]]) destroying_neebles = true;
                else if(globals.key_pressed[key_scheme[4]]) creating_neeble = true;
            }
        }

        int h = p_0->p_num - 1;
        if (creating_neeble) {
            for (int i = 0; i < TOTAL_NEEBLES; i++) {
                if (neeble_list[h * TOTAL_NEEBLES + i].dead) {
                    p_0->neebles_stored -= 1;
                    neeble n = {p_0, -V3(C_PLAYERS[h](0 , 2), C_PLAYERS[h](1 , 2), -C_PLAYERS[h](2 , 2))*0.2};
                    neeble_list[h * TOTAL_NEEBLES + i] = n;
                    break;
                }
            }
        }
        if (destroying_neebles) {
            bool ball_exists = false;
            for (int i = 0; i < TOTAL_NEEBLES; i++) {
                if (!neeble_list[h*TOTAL_NEEBLES + i].dead) {
                    if (!neeble_list[h*TOTAL_NEEBLES + i].ball_form) kill_neeble(&(neeble_list[h*TOTAL_NEEBLES + i]));
                    else ball_exists = true;
                }
            }
            if (!ball_exists) p_0->neebles_stored = TOTAL_NEEBLES;
        }

        movement.x = CLAMP(movement.x, p_0->wall_coll.z, p_0->wall_coll.w);
        movement.y = CLAMP(movement.y, p_0->wall_coll.y, p_0->wall_coll.x);

        p_0->world_pos += update_for_corner(p_0, movement);
        p_0->grid_pos = get_grid_pos(p_0->world_pos);
        p_0->cell_x = (short)(p_0->grid_pos).x;
        p_0->cell_y = (short)(p_0->grid_pos).y;

        //lava "overtime"
        if (MAP[(GRID_SIDE - 1 - p_0->cell_y) * GRID_SIDE + p_0->cell_x] == 3) {
            p_0->ghost_timer = MAX(p_0->ghost_timer - 0.0167, -GHOST_TIME);
            if (p_0->ghost_timer <= -GHOST_TIME){
                p_0->ghost = false;
                kill_player(p_0);
            }
        }
        //recharging the clock
        else if (!p_0->ghost) {
            if (p_0->ghost_timer < GHOST_TIME) p_0->ghost_timer = MIN(p_0->ghost_timer + 0.0015, GHOST_TIME);
        }
        //spending the clock
        else {
            if (p_0->ghost_timer <= 0.0) p_0->ghost = false;
            else p_0->ghost_timer = MAX(p_0->ghost_timer - 0.0167, 0.0);
        }
    }
}

void screencheatify (short* map) {
    int map_radius = (GRID_SIDE/2);
    for (int i = 0; i < GRID_SIDE; i++) {
        for(int j = 0; j < GRID_SIDE; j++) {
            if (i <= map_radius && map[i*GRID_SIDE + j] == 3) {
                if(rand() % 3 > 0) {
                    map[i*GRID_SIDE + j] = 2;
                    map[GRID_SIDE * GRID_SIDE - 1 - i*GRID_SIDE - j] = 2;
                }
                else {
                    map[i*GRID_SIDE + j] = 0;
                    map[GRID_SIDE * GRID_SIDE - 1 - i*GRID_SIDE - j] = 0;
                }
            }
            if (map[i*GRID_SIDE + j] && map[i*GRID_SIDE + j] % 2 == 0 && !(ABS(j-map_radius) < GRID_SIDE/4 && ABS(i-map_radius) < GRID_SIDE/4)) {
                if (i < map_radius && j <= map_radius) {
                    map[i*GRID_SIDE + j] = 4;
                }
                else if (i <= map_radius && j > map_radius) {
                    map[i*GRID_SIDE + j] = 6;
                }
                else if (i > map_radius && j >= map_radius) {
                    map[i*GRID_SIDE + j] = 8;
                }
                else if (i >= map_radius && j < map_radius) {
                    map[i*GRID_SIDE + j] = 10;
                }
            }
        }
    }
}

//2 = HEDGE
//3 = HOLE
void polish_map (short* map) {
    for (int i = 0; i < (GRID_SIDE/2)+1; i++) {
        for(int j = 0; j < GRID_SIDE && i*GRID_SIDE + j <= 1+(GRID_SIDE*GRID_SIDE)/2; j++) {
            int num_adjacent_walls = 0;
            int num_diagonal_walls = 0;
            bool edge = false;

            for (int a = i-1; a <= i+1; a+=2) {
                if (!(a < 0 || a >= GRID_SIDE)) {
                    for (int b = j-1; b <= j+1; b+=2){
                        if (!(b < 0 || b >= GRID_SIDE) && map[a*GRID_SIDE + b]) num_diagonal_walls += 1;
                    }
                }
            }

            if (map[i*GRID_SIDE + j] && (num_diagonal_walls >= 3 || (num_diagonal_walls >= 2 && rand()%3 == 0))) {
                map[i*GRID_SIDE + j] = 3;
                map[GRID_SIDE*GRID_SIDE - 1 - (i*GRID_SIDE + j)] = 3;
            }
            else {
                if (i == 0) {
                    edge = true;
                    num_adjacent_walls+=1;
                }
                else if (i <= (GRID_SIDE/2) && map[(i-1)*GRID_SIDE + j]) {
                    num_adjacent_walls+=1;
                }
                
                if (j == 0) {
                    edge = true;
                    num_adjacent_walls+=1;
                }
                else if (map[i*GRID_SIDE + j-1]) {
                    num_adjacent_walls+=1;
                }

                if (j == GRID_SIDE-1) {
                    edge = true;
                    num_adjacent_walls+=1;
                }
                else if (map[i*GRID_SIDE + j+1]) {
                    num_adjacent_walls+=1;
                }

                if (i <= (GRID_SIDE/2) && map[(i+1)*GRID_SIDE + j]) {
                    num_adjacent_walls+=1;
                }
                if (i == (GRID_SIDE/2) && map[i*GRID_SIDE + j] == 1 && 
                    (num_adjacent_walls + num_diagonal_walls > 0) &&
                    j >= (GRID_SIDE/3) && j <= GRID_SIDE - (GRID_SIDE/3))
                    {
                        map[i*GRID_SIDE + j] = 3;
                        map[GRID_SIDE*GRID_SIDE - 1 - (i*GRID_SIDE + j)] = 3;
                }

                if (map[i*GRID_SIDE + j] && num_diagonal_walls >= 2 && num_diagonal_walls + num_adjacent_walls >= 3) {
                    map[i*GRID_SIDE + j] = 3;
                    map[GRID_SIDE*GRID_SIDE - 1 - (i*GRID_SIDE + j)] = 3;
                }
                else if (num_adjacent_walls == 4) {
                    for (int k = MAX(j-1, 0); k <= MIN(j+1, GRID_SIDE-1); k+=2) {
                        map[i*GRID_SIDE + k] = 3;
                        map[GRID_SIDE*GRID_SIDE - 1 - (i*GRID_SIDE + k)] = 3;
                    }
                    for (int k = MAX(i-1, 0); k <= MIN(i+1, GRID_SIDE-1); k+=2) {
                        map[k*GRID_SIDE + j] = 3;
                        map[GRID_SIDE*GRID_SIDE - 1 - (k*GRID_SIDE + j)] = 3;
                    }
                    int r = 2 + rand() % 2;
                    map[i*GRID_SIDE + j] = edge ? 3 : r;
                    map[GRID_SIDE*GRID_SIDE - 1 - (i*GRID_SIDE + j)] = edge ? 3 : r;
                }
                else if (map[i*GRID_SIDE + j] == 1){
                    if(edge && num_adjacent_walls + num_diagonal_walls > rand()%2 + 1) {
                        map[i*GRID_SIDE + j] = 3;
                        map[GRID_SIDE*GRID_SIDE - 1 - (i*GRID_SIDE + j)] = 3;
                    }
                    else if (rand()%3 == 0) {
                        if (i > 0 && map[(i-1)*GRID_SIDE + j] == 3) {
                            map[i*GRID_SIDE + j] = 3;
                            map[GRID_SIDE*GRID_SIDE - 1 - (i*GRID_SIDE + j)] = 3;
                        }
                        else if (j > 0 && map[i*GRID_SIDE + j-1] == 3) {
                            map[i*GRID_SIDE + j] = 3;
                            map[GRID_SIDE*GRID_SIDE - 1 - (i*GRID_SIDE + j)] = 3;
                        }
                    }
                    if (map[i*GRID_SIDE + j] == 1){
                        map[i*GRID_SIDE + j] = 2;
                        map[GRID_SIDE*GRID_SIDE - 1 - (i*GRID_SIDE + j)] = 2;
                    }
                }
            }
        }
    }
}

void shuffle (short* map) {
    short* map_copy = (short*)malloc(GRID_SIDE * GRID_SIDE * sizeof(short));
    for (int i = 0; i < GRID_SIDE; i++) {
        for(int j = 0; j < GRID_SIDE; j++) {
            map_copy[i*GRID_SIDE + j] = map[i*GRID_SIDE + j];
        }
    }
    for (int i = 0; i < (GRID_SIDE/2)+1; i++) {
        for(int j = 0; j < GRID_SIDE; j++) {
            if (map_copy[i*GRID_SIDE + j]) {
                real r = random_real(0.0, 1.0);
                vec2 possible[4];
                int num_adjacent_spaces = 0;
                if (j > 0 && !map_copy[i*GRID_SIDE + j-1]) {
                    possible[num_adjacent_spaces] = V2(j-1, i);
                    num_adjacent_spaces++;
                }
                if (j < GRID_SIDE-1 && !map_copy[i*GRID_SIDE + j+1]) {
                    possible[num_adjacent_spaces] = V2(j+1, i);
                    num_adjacent_spaces++;
                }
                if (i > 0 && i < (GRID_SIDE/2) && !map_copy[(i-1)*GRID_SIDE + j]) {
                    possible[num_adjacent_spaces] = V2(j, i-1);
                    num_adjacent_spaces++;
                }
                if (i < (GRID_SIDE/2) && !map_copy[(i+1)*GRID_SIDE + j]) {
                    possible[num_adjacent_spaces] = V2(j, i+1);
                    num_adjacent_spaces++;
                }
                if (num_adjacent_spaces>0) {
                    r = random_real(0.0, num_adjacent_spaces);
                    int x = (int)(possible[(int)r].x);
                    int y = (int)(possible[(int)r].y);
                    map[(int)(y * GRID_SIDE + x)] = 1;
                    map[i*GRID_SIDE + j] = 0;

                    map[GRID_SIDE*GRID_SIDE - 1 - (int)(y * GRID_SIDE + x)] = 1;
                    map[GRID_SIDE*GRID_SIDE - 1 - (i*GRID_SIDE + j)] = 0;
                }
            }
        }
    }
    free(map_copy);
}

//0 = EMPTY
//1 = VERT
//2 = HORZ
short* generate_map(int style) {
    short* map = (short*)malloc(GRID_SIDE * GRID_SIDE * sizeof(short));
    bool wall_factor = false;
    for (int i = 0; i < GRID_SIDE; i++) {
        for(int j = 0; j < GRID_SIDE; j++) {
            switch(style) {
                case 0:
                    wall_factor = 0;
                    break;
                case 1:
                    wall_factor = j%2 == 0 && i != GRID_SIDE/2
                                    && i != GRID_SIDE/4 && i != GRID_SIDE - 1 - GRID_SIDE/4;
                    break;
                case 2:
                    wall_factor = i%2 == 0 && j != GRID_SIDE/2
                                    && j != GRID_SIDE/4 && j != GRID_SIDE - 1 - GRID_SIDE/4;                                    
                    break;
            }
            if(wall_factor) {
                map[i*GRID_SIDE + j] = 1;
            } else map[i*GRID_SIDE + j] = 0;
        }
    }
    shuffle(map);
    shuffle(map);
    if (GAMEMODE != GM_HOTEL) shuffle(map);
    polish_map(map);

    if (GAMEMODE == GM_HOTEL) screencheatify(map);
    else {
        wall_tex_list[0] = HEDGE_WALL;
        wall_tex_list[1] = HEDGE_SHADE_WALL;
    }

    return map;
}

void initialize_game() {
    global_time = 0.0;
    GRID_SIDE = (rand() % 2) * 2 + ((GAMEMODE == GM_HOTEL || NUM_PLAYERS > 2) ? 19 : 17);
    WORLD_SIZE = 1.6;

    TOTAL_NEEBLES = NUM_PLAYERS > 3 ? 3 : 4;

    for (int i = 0; i < 4 * PARTS_PER_PLAYER; i++) {
        particle_list[i].active = false;
    }

    MAP = generate_map(1 + rand() % 2);

    int spawn_12_x = 0, spawn_34_y = 0;

    for(int i = 1; i<GRID_SIDE; i++) {
        if(!MAP[i] && !MAP[i + GRID_SIDE] && !MAP[i + GRID_SIDE*2]) {
            spawn_12_x = i;
            break;
        }
    }
    for(int i = GRID_SIDE - 2; i>=0; i--) {
        if(!MAP[GRID_SIDE * i] && !MAP[GRID_SIDE * i + 1] && !MAP[GRID_SIDE * i + 2]) {
            spawn_34_y = i;
            break;
        }
    }

    spawn_list[0] = {spawn_12_x, GRID_SIDE-1};
    spawn_list[0].theta = 181;
    spawn_list[1] = {GRID_SIDE - 1 - spawn_12_x, 0};
    spawn_list[1].theta = 1;
    spawn_list[2] = {0, GRID_SIDE - 1 - spawn_34_y};
    spawn_list[2].theta = 89;
    spawn_list[3] = {GRID_SIDE - 1, spawn_34_y};
    spawn_list[3].theta = -91;

    StretchyBuffer<int> avail_skins = {};
    for (int i = 0; i < NUM_SKINS; i++) {
        sbuff_push_back(&avail_skins, i);
    }

    StretchyBuffer<spawn> avail_spawns = {};
    for (int i = 0; i < 4; i++) {
        sbuff_push_back(&avail_spawns, spawn_list[i]);
    }

    player p_0;
    for(int i = 0; i < NUM_PLAYERS; i++) {
        p_0.p_num = i+1;
        p_0.turn = P1_TURN;
        p_0.speed = P1_SPEED;
        p_0.radius = P1_RADIUS;
        if (i > 1 && TEAM_MODE) {
            p_0.skin = player_list[i-2].skin;
        }
        else {
            int skin_temp = rand() % (NUM_SKINS-i);
            p_0.skin = avail_skins.data[skin_temp];
            sbuff_delete(&avail_skins, skin_temp);
        }

        if (player_list[i].controller == -1) {
            p_0.controller = i + (NUM_PLAYERS > 2 ? 2 : 0);
        }
        else {
            p_0.controller = player_list[i].controller;
        }

        int st = rand() % (4-i);
        spawn spawn_temp = avail_spawns.data[st];
        sbuff_delete(&avail_spawns, st);
        p_0.theta = spawn_temp.theta;
        p_0.grid_pos = spawn_temp.grid_pos;
        p_0.world_pos = get_world_pos(p_0.grid_pos);
        p_0.cell_x = spawn_temp.cell_x;
        p_0.cell_y = spawn_temp.cell_y;
        player_list[i] = p_0;
        p_0.part_index_0 = 0;
        p_0.part_index_c = 0;
    }
    sbuff_free(&avail_spawns);
    sbuff_free(&avail_skins);
}

void draw_map_2D(mat4 PV, float fade, int max_tiles) {
    //drawing the space
    eso_begin(PV, SOUP_QUADS);
    //draw_square(V2(((-(real)GRID_SIDE)/2.0)-0.175), (real)GRID_SIDE+0.35, V3(1.0,0.0,0.0));
    //draw_square(V2((-(real)GRID_SIDE)/2.0), (real)GRID_SIDE, V3(0.0, 0.0, 0.0)*fade);
    for(int i=0; i < GRID_SIDE; i++) {
        for(int j = 0; j < GRID_SIDE; j++) {
            if (i*GRID_SIDE + j <= max_tiles) {
                vec3 s_color;
                switch (MAP[i*GRID_SIDE + j]) {
                    case 0:
                        s_color = monokai.black;
                    case 1:
                        s_color = monokai.black;
                        break;
                    case 2:
                        s_color = GAMEMODE == GM_HOTEL ? monokai.purple*fade : V3(0.0,0.75,0.25)*fade;
                        break;
                    case 3:
                        s_color = GAMEMODE == GM_HOTEL ? monokai.gray*fade : V3(1.0, 0.0, 0.0)*fade;
                        break;
                    case 4:
                        s_color = V3(1.0, 0.0, 0.0)*fade;
                        break;
                    case 6:
                        s_color = monokai.yellow*fade;
                        break;
                    case 8:
                        s_color = monokai.green*fade;
                        break;
                    case 10:
                        s_color = monokai.blue*fade;
                        break;
                }
                draw_square(V2((real)j-((real)GRID_SIDE*0.5), (real)(GRID_SIDE-1-i)-((real)GRID_SIDE*0.5)), 1.0,
                                s_color);
            }
            else break;
        }
    }
    eso_end();
}

void app1() {
    Camera2D camera = {GRID_SIDE * WORLD_SIZE +0.6};

    player* player_1 = &player_list[0];
    player* player_2 = &player_list[1];
    MODE_2D = true;

    while (cow_begin_frame()) {
        camera_move(&camera);
        mat4 PV = camera_get_PV(&camera);

        draw_map_2D(PV, 1.0, GRID_SIDE*GRID_SIDE);

        update_player(player_1);
        update_player(player_2);

        //drawing the guy(s)
        eso_begin(PV, SOUP_POINTS, player_1->radius, true);
        eso_color(monokai.yellow);
        eso_vertex(player_1->world_pos);
        eso_end();

        eso_begin(PV, SOUP_POINTS, player_2->radius, true);
        eso_color(monokai.purple);
        eso_vertex(player_2->world_pos);
        eso_end();

        //drawing the guys' direction
        eso_begin(PV, SOUP_POINTS, P1_RADIUS/3, true);
        eso_color(monokai.blue);
        eso_vertex((player_1->world_pos).x + player_1->radius*sin(RAD(player_1->theta)),
                    (player_1->world_pos).y + player_1->radius*cos(RAD(player_1->theta)));
        eso_end();

        eso_begin(PV, SOUP_POINTS, P1_RADIUS/3, true);
        eso_color(monokai.blue);
        eso_vertex((player_2->world_pos).x + player_2->radius*sin(RAD(player_2->theta)),
                    (player_2->world_pos).y + player_2->radius*cos(RAD(player_2->theta)));
        eso_end();
        
        gui_printf("P1 World Pos: %lf %lf \n", player_1->world_pos.x, player_1->world_pos.y);
        gui_printf("P1 Grid Pos: %lf %lf \n", player_1->grid_pos.x, player_1->grid_pos.y);
        gui_printf("P2 World Pos: %lf %lf \n", player_2->world_pos.x, player_2->world_pos.y);
        gui_printf("Grid: %d", GRID_SIDE);
        gui_printf("World Size: %lf", WORLD_SIZE);
        gui_readout("time", &global_time);
        if (gui_button("shuffle! :)")){
            shuffle(MAP);
            polish_map(MAP);
        }

        global_time += 0.0167;
    }
}

bool r_equals(real a, real b){
    return ABS(a - b) <= TINY_VAL;
}

void move_neeble(neeble *n_0, vec2 grid_move) {
    n_0->grid_pos += n_0->previous_move * 0.5;
    n_0->previous_move = grid_move;
    n_0->grid_pos += n_0->previous_move;
    n_0->world_pos = get_world_pos(n_0->grid_pos);
    n_0->cell_x = (int)(n_0->grid_pos.x);
    n_0->cell_y = (int)(n_0->grid_pos.y);
}


void update_neeble(neeble *n_0, int n_0_index) {
    player *owner = n_0->owner;
    //NEEBLE AS PROJECTILE
    if(n_0->ball_form) {
        if (n_0->vert_pos < WORLD_SIZE*3) {
            vec2 new_pos = n_0->world_pos + V2((n_0->ball_velocity).x, (n_0->ball_velocity).z);
            vec2 new_grid_pos = get_grid_pos(new_pos);
            int tile_type = MAP[(GRID_SIDE - 1 - (int)new_grid_pos.y) * GRID_SIDE + (int)(new_grid_pos.x)];
            if (n_0->vert_pos < WORLD_SIZE && 
                ((tile_type % 2 == 0 && tile_type) || n_0->vert_pos <= 0 ||
                new_grid_pos.x <= 0 || new_grid_pos.x >= GRID_SIDE || new_grid_pos.y <= 0 || new_grid_pos.y >= GRID_SIDE)) {
                    n_0->world_pos -= V2((n_0->ball_velocity).x, (n_0->ball_velocity).z)*2.0;
                    n_0->grid_pos = get_grid_pos(n_0->world_pos);
                    n_0->cell_x = (int)n_0->grid_pos.x;
                    n_0->cell_y = (int)n_0->grid_pos.y;
                    n_0->ball_form = false;
                    n_0->ball_velocity = V3(0.0);
                    n_0->pose = 0;
                    n_0->target_cell = V2(n_0->cell_x, n_0->cell_y);
            }
            else {
                n_0->world_pos = new_pos;
                n_0->grid_pos = new_grid_pos;
                n_0->cell_x = (int)(new_grid_pos.x);
                n_0->cell_y = (int)(new_grid_pos.y);
                n_0->vert_pos = MAX(n_0->vert_pos + (n_0->ball_velocity).y, 0.0);
                n_0->pose = (int)(global_time*10) % 3;
                if ((int)(global_time * 60.0)%6 == 0 && !((n_0->ball_velocity).y < -0.035)) {
                    single_hanging_particle(owner, V3(n_0->world_pos.x, n_0->vert_pos + P1_RADIUS * 0.4, -n_0->world_pos.y));
                }
            }
        }
        else kill_neeble(n_0);
    }

    //NEEBLE AS GOBLIN
    //in lava
    else if(MAP[(GRID_SIDE - 1 - n_0->cell_y)*GRID_SIDE + n_0->cell_x] == 3 ) {
        n_0->vert_pos -= n_0->vert_pos <= -0.2 ? 0.015 : 0.1;
        if (n_0->vert_pos < -10.0){
            kill_neeble(n_0);
        }
    }
    //solid ground, typical updating
    else if(n_0->vert_pos == 0.0) {
        n_0->pose = (int)(global_time * 5)%2;

        bool chase = false, get_new_target = false;
        real speedy = 1;
        vec2 target_grid_pos = n_0->target_cell + V2(0.5);
        vec2 dist_to_target = target_grid_pos - n_0->grid_pos;

        if (ABS(norm(dist_to_target)) <= 0.05) get_new_target = true;

        for(int j = 0; j < NUM_PLAYERS; j++) {
            player *p_0 = &player_list[j];
            if (!p_0->dead && owner->skin != p_0->skin) {
                if (p_0->cell_x == n_0->cell_x && p_0->cell_y == n_0->cell_y) { 
                    speedy = 2.5;
                    move_neeble(n_0, normalized(p_0->grid_pos - n_0->grid_pos)* 0.01*speedy);
                    n_0->target_cell = V2(p_0->cell_x, p_0->cell_y);
                    n_0->previous_cell = V2(-1);
                    n_0->super_previous_cell = V2(-1);
                    n_0->wait = -1.0;
                    chase = true;
                    p_0->alert = true;
                    break;
                }
                else if (ABS(p_0->cell_x - n_0->cell_x) + ABS(p_0->cell_y - n_0->cell_y) <= 3.0) {
                    get_new_target = true;
                    n_0->previous_cell = V2(-1);
                    n_0->super_previous_cell = V2(-1);
                    n_0->wait = -1.0;
                    break;
                }
            }
        }

        if (n_0->wait <= 0.0) {
            if(get_new_target) {
                //get new target
                //UP DOWN LEFT RIGHT CURRENT
                vec2 curr_tile = V2(n_0->cell_x, n_0->cell_y);
                vec2 target_options[5] = {curr_tile + V2(0, 1),
                                        curr_tile + V2(0, -1),
                                        curr_tile + V2(-1, 0),
                                        curr_tile + V2(1, 0),
                                        curr_tile};
                int target_scores[5] = {0, 0, 0, 0, 1};
                for (int i = 0; i < 5; i++) {
                    //do not want to move into a pit or a hedge
                    if(target_options[i].x < 0 || target_options[i].y < 0 || target_options[i].x > GRID_SIDE-1 || target_options[i].y > GRID_SIDE-1 ||
                        MAP[(GRID_SIDE - 1 - (int)(target_options[i].y))*GRID_SIDE + (int)(target_options[i].x)] != 0)
                        {
                            target_scores[i] -= 100;
                        }
                    //do not want to move to a space we were already in
                    if (r_equals(target_options[i].x, n_0->previous_cell.x) && r_equals(target_options[i].y, n_0->previous_cell.y)) {
                        target_scores[i] -= 1;
                    }
                    else if (r_equals(target_options[i].x, n_0->super_previous_cell.x) && r_equals(target_options[i].y, n_0->super_previous_cell.y)) {
                        target_scores[i] -= 1;
                    }
                    //do not want to move to another neeble's space
                    for(int j = 0; j < TOTAL_NEEBLES * NUM_PLAYERS; j++) {
                        if (j != n_0_index && !neeble_list[j].dead && !neeble_list[j].ball_form) {
                            if(r_equals(neeble_list[j].target_cell.x, target_options[i].x) && r_equals(neeble_list[j].target_cell.y, target_options[i].y)) {
                                target_scores[i] -= 4;
                            }
                            else if (r_equals(neeble_list[j].cell_x, target_options[i].x) && r_equals(neeble_list[j].cell_y, target_options[i].y)) {
                                target_scores[i] -= 3;
                            }
                        }
                    }
                    //want to chase the enemy
                    for(int j = 0; j < NUM_PLAYERS; j++) {
                        player *p_0 = &player_list[j];
                        if (!p_0->dead && owner->skin != p_0->skin) {
                            if(r_equals(p_0->cell_x, target_options[i].x) && r_equals(p_0->cell_y, target_options[i].y)) {
                                target_scores[i] += 6;
                                speedy = 2.35;
                            }
                            if (ABS(p_0->cell_x - target_options[i].x) + ABS(p_0->cell_y - target_options[i].y) <= 1+TINY_VAL) {
                                target_scores[i] += 4;
                                speedy = 2.2;
                                p_0->alert = true;
                            }
                            else if (ABS(p_0->cell_x - target_options[i].x) + ABS(p_0->cell_y - target_options[i].y) <= 2+TINY_VAL) {
                                target_scores[i] += 2;
                                p_0->alert = true;
                            }
                            else if (ABS(p_0->cell_x - target_options[i].x) + ABS(p_0->cell_y - target_options[i].y) >= 4-TINY_VAL) {
                                target_scores[i] -= 2;
                            }
                            else p_0->alert = true;
                        }
                    }
                }
                int max_score = -200;
                int target_winners[5];
                int num_winners = 0;
                for (int i = 0; i < 5; i ++) {
                    if (target_scores[i] > max_score) max_score = target_scores[i];
                }
                for (int i = 0; i < 5; i ++) {
                    if (target_scores[i] == max_score) {
                        target_winners[num_winners] = i;
                        num_winners++;
                    }
                }
                n_0->super_previous_cell = n_0->previous_cell;
                n_0->previous_cell = n_0->target_cell;
                int winner = target_winners[rand()%num_winners];
                if (winner == 4) n_0->wait = 0.6;
                else n_0->wait = 0.0;
                n_0->target_cell = target_options[winner];
            }

            //move towards target grid position
            if (!chase) {
                target_grid_pos = n_0->target_cell + V2(0.5);
                dist_to_target = target_grid_pos - n_0->grid_pos;
                if(ABS(norm(dist_to_target)) >= 0.05) {
                    move_neeble(n_0, normalized(target_grid_pos - n_0->grid_pos) * 0.01*speedy);
                }
            }
        }
        else {
            n_0->wait -= 0.0167;
            move_neeble(n_0, V2(0.0));
        }
    }
    else if (n_0->vert_pos > 0.0) {
        n_0->vert_pos = MAX(n_0->vert_pos-0.1, 0.0);
    }
    else n_0->vert_pos = 0.0;

    for (int i = 0; i < NUM_PLAYERS; i++) {
        if (n_0->owner->skin != player_list[i].skin && !player_list[i].dead && !(player_list[i].ghost) &&
            ABS(norm(n_0->grid_pos - player_list[i].grid_pos)) <= player_list[i].radius * (n_0->ball_form ? 0.7 : 1.15) &&
            n_0->vert_pos < 1.0)
            {
                kill_player(&player_list[i]);
            }
    }
}

void get_square_tex_coords(vec2* tex_coords, vec2 top_left, int side_len, int image_dimension) {
    tex_coords[0] = V2(top_left.x, top_left.y - side_len)*(1.0/image_dimension);
    tex_coords[1] = top_left*(1.0/image_dimension);
    tex_coords[2] = V2(top_left.x + side_len, top_left.y)*(1.0/image_dimension);
    tex_coords[3] = V2(top_left.x + side_len, top_left.y - side_len)*(1.0/image_dimension);

    real nudge = .000001;
    tex_coords[0] += V2( nudge,  nudge);
    tex_coords[1] += V2( nudge, -nudge);
    tex_coords[2] += V2(-nudge, -nudge);
    tex_coords[3] += V2(-nudge,  nudge);

}

IndexedTriangleMesh3D WORLD;
IndexedTriangleMesh3D panel;
StretchyBuffer<vec3> WORLD_VERTEX_POSITIONS;
StretchyBuffer<int3> WORLD_TRIANGLE_INDICES;
StretchyBuffer<vec2> WORLD_TEX_COORDS;

void panel_draw(mat4 M, vec2 tex_top_left, int side_len) {
    vec2 coords[4];
    get_square_tex_coords(coords, tex_top_left, side_len, 128);
    for (int i = 0; i < panel.num_vertices; i++) {
        vec4 transformed_vertex = M * V4(panel.vertex_positions[i], 1.0);
        sbuff_push_back(&WORLD_VERTEX_POSITIONS, V3(transformed_vertex.x, transformed_vertex.y, transformed_vertex.z));
        sbuff_push_back(&WORLD_TEX_COORDS, coords[i]);
    }
    int zeroth_vertex = WORLD_VERTEX_POSITIONS.length - 4;
    for (int i = 0; i < panel.num_triangles; i++) {
        int3 indices = { panel.triangle_indices[i][0] + zeroth_vertex,
                         panel.triangle_indices[i][1] + zeroth_vertex,
                         panel.triangle_indices[i][2] + zeroth_vertex};

        sbuff_push_back(&WORLD_TRIANGLE_INDICES, indices);
    }
}

// 0 = EMPTY
// 2 = WALL
// 3 = PIT

void draw_tile_3D(short type, int2 position) {
    if (!type) {
        //EMPTY
        panel_draw(M4_Translation(position.i * WORLD_SIZE - (GRID_SIDE*WORLD_SIZE)/2.0, 0.0, position.j * WORLD_SIZE - (GRID_SIDE*WORLD_SIZE)/2.0)
                *M4_RotationAboutXAxis(PI/2.0), STONE_FLOOR, 16);

    }
    else if (type%2 == 0) {
        vec2 wall = wall_tex_list[type - 2];
        vec2 wall_shade = wall_tex_list[type - 1];

        //UP
        if (position.j != 0 && !(MAP[GRID_SIDE * (position.j - 1) + position.i] && MAP[GRID_SIDE * (position.j - 1) + position.i] % 2 == 0)) {
            panel_draw(M4_Translation(position.i * WORLD_SIZE - (GRID_SIDE*WORLD_SIZE)/2.0, 0, position.j * WORLD_SIZE - (GRID_SIDE*WORLD_SIZE)/2.0),
                        wall, GAMEMODE == GM_HOTEL ? 32 : 16);
        }

        //RIGHT
        if (position.i != GRID_SIDE-1 && !(MAP[GRID_SIDE * (position.j) + position.i + 1] && MAP[GRID_SIDE * (position.j) + position.i + 1] % 2 == 0)) {
            panel_draw(M4_Translation((1 + position.i) * WORLD_SIZE - (GRID_SIDE*WORLD_SIZE)/2.0,
                    0,
                    position.j * WORLD_SIZE - (GRID_SIDE*WORLD_SIZE)/2.0)*M4_RotationAboutYAxis(-PI/2.0),
                        wall_shade, GAMEMODE == GM_HOTEL ? 32 : 16);
        }

        //DOWN
        if (position.j != GRID_SIDE-1 && !(MAP[GRID_SIDE * (position.j + 1) + position.i] && MAP[GRID_SIDE * (position.j + 1) + position.i] % 2 == 0)) {
            panel_draw(M4_Translation((1 + position.i) * WORLD_SIZE - (GRID_SIDE*WORLD_SIZE)/2.0, 0, (1+position.j) * WORLD_SIZE - (GRID_SIDE*WORLD_SIZE)/2.0) *M4_RotationAboutYAxis(PI),
                        wall, GAMEMODE == GM_HOTEL ? 32 : 16);
        }

        //LEFT
        if (position.i != 0 && !(MAP[GRID_SIDE * (position.j) + position.i - 1] && MAP[GRID_SIDE * (position.j) + position.i - 1] % 2 == 0)) {
            panel_draw(M4_Translation(position.i * WORLD_SIZE - (GRID_SIDE*WORLD_SIZE)/2.0, 0, (1+position.j) * WORLD_SIZE - (GRID_SIDE*WORLD_SIZE)/2.0)*M4_RotationAboutYAxis(PI/2.0),
                        wall_shade, GAMEMODE == GM_HOTEL ? 32 : 16);
        }
    }
    else if (type == 3) {
        //UP
        if (position.j == 0 || MAP[(int)((position.j-1) * GRID_SIDE + position.i)] != 3) {
            panel_draw(M4_Translation(position.i * WORLD_SIZE - (GRID_SIDE*WORLD_SIZE)/2.0, -WORLD_SIZE, position.j * WORLD_SIZE - (GRID_SIDE*WORLD_SIZE)/2.0),
                        LAVA_BED, 16);
        }
        //RIGHT
        if (position.i == GRID_SIDE-1 || MAP[(int)(position.j * GRID_SIDE + position.i + 1)] != 3) {
            panel_draw(M4_Translation((1 + position.i) * WORLD_SIZE - (GRID_SIDE*WORLD_SIZE)/2.0, -WORLD_SIZE, position.j * WORLD_SIZE - (GRID_SIDE*WORLD_SIZE)/2.0)*M4_RotationAboutYAxis(-PI/2.0),
                        LAVA_BED, 16);
        }
        //DOWN
        if (position.j == GRID_SIDE-1 || MAP[(int)((position.j+1) * GRID_SIDE + position.i)] != 3) {
            panel_draw(M4_Translation((1 + position.i) * WORLD_SIZE - (GRID_SIDE*WORLD_SIZE)/2.0, -WORLD_SIZE, (1+position.j) * WORLD_SIZE - (GRID_SIDE*WORLD_SIZE)/2.0) *M4_RotationAboutYAxis(PI),
                        LAVA_BED, 16);
        }
        //LEFT
        if (position.i == 0 || MAP[(int)(position.j * GRID_SIDE + position.i - 1)] != 3) {
            panel_draw(M4_Translation(position.i * WORLD_SIZE - (GRID_SIDE*WORLD_SIZE)/2.0, -WORLD_SIZE, (1+position.j) * WORLD_SIZE - (GRID_SIDE*WORLD_SIZE)/2.0) *M4_RotationAboutYAxis(PI/2.0),
                        LAVA_BED, 16);
        }
        panel_draw(M4_Translation(position.i * WORLD_SIZE - (GRID_SIDE*WORLD_SIZE)/2.0, -0.1 + 0.035, position.j * WORLD_SIZE - (GRID_SIDE*WORLD_SIZE)/2.0) *M4_RotationAboutXAxis(PI/2.0),
                    LAVA, 16);
    }
}



IndexedTriangleMesh3D create_world_mesh() {
    //draw grid tiles and border
    WORLD_VERTEX_POSITIONS = {};
    WORLD_TRIANGLE_INDICES = {};
    WORLD_TEX_COORDS = {};
    vec2 texture;
    int side_length = GAMEMODE == GM_HOTEL ? 32 : 16;
    int map_radius = (GRID_SIDE/2);
    for (int i = -1; i <= GRID_SIDE; i++) {
        for (int j = -1; j <= GRID_SIDE; j++) {
            if (i == GRID_SIDE) {
                if (j != -1 && j != GRID_SIDE) {
                    texture = GAMEMODE == GM_CLASSIC ? wall_tex_list[0] : (j < map_radius ? wall_tex_list[8] : wall_tex_list[6]);
                    panel_draw(M4_Translation(j * WORLD_SIZE - (GRID_SIDE*WORLD_SIZE)/2.0, 0, i * WORLD_SIZE - (GRID_SIDE*WORLD_SIZE)/2.0), texture, side_length);
                }
            }
            else if (i == -1) {
                if (j != -1 && j != GRID_SIDE) {
                    texture = GAMEMODE == GM_CLASSIC ? wall_tex_list[0] : (j > map_radius ? wall_tex_list[4] : wall_tex_list[2]);
                    panel_draw(M4_Translation((1 + j) * WORLD_SIZE - (GRID_SIDE*WORLD_SIZE)/2.0, 0, (1+i) * WORLD_SIZE - (GRID_SIDE*WORLD_SIZE)/2.0)*M4_RotationAboutYAxis(PI), texture, side_length);
                }
            }
            else if (j == -1) {
                texture = GAMEMODE == GM_CLASSIC ? wall_tex_list[1] : (i < map_radius ? wall_tex_list[3] : wall_tex_list[9]);
                panel_draw(M4_Translation((1 + j) * WORLD_SIZE - (GRID_SIDE*WORLD_SIZE)/2.0, 0, i * WORLD_SIZE - (GRID_SIDE*WORLD_SIZE)/2.0)*M4_RotationAboutYAxis(-PI/2.0), texture, side_length);
            }
            else if (j == GRID_SIDE) {
                texture = GAMEMODE == GM_CLASSIC ? wall_tex_list[1] : (i > map_radius ? wall_tex_list[7] : wall_tex_list[5]);
                panel_draw(M4_Translation(j * WORLD_SIZE - (GRID_SIDE*WORLD_SIZE)/2.0, 0, (1+i) * WORLD_SIZE - (GRID_SIDE*WORLD_SIZE)/2.0)*M4_RotationAboutYAxis(PI/2.0), texture, side_length);
            }
            else {
                int2 coords = {j, i};
                draw_tile_3D(MAP[i*GRID_SIDE + j], coords);
            }
        }
    }

    IndexedTriangleMesh3D world = {WORLD_VERTEX_POSITIONS.length, WORLD_TRIANGLE_INDICES.length, WORLD_VERTEX_POSITIONS.data, NULL, NULL, WORLD_TRIANGLE_INDICES.data, WORLD_TEX_COORDS.data, "map_textures.png"};
    return world;
}


mat4 fps_camera_get_C(player *p_0) {
    mat4 C = M4_Translation(V3(p_0->world_pos.x, 0.7, -p_0->world_pos.y)) 
                * M4_RotationAboutYAxis(RAD(360 - p_0->theta))
                * M4_RotationAboutXAxis(RAD(p_0->pitch));
    return C;
}

void draw_sprite_3D(player *p_0, int3 *sprite_indic, vec3 *player_verts, mat4 P, mat4 V, mat4 C) {
    int pose = 0;
    real difference = 0.0;
    real angle_to_cam;
    int theta = (180 - p_0->theta);

    angle_to_cam = atan2(C(0, 3) - (p_0->world_pos.x), C(2, 3) + (p_0->world_pos.y));

    difference = (((int)DEG(angle_to_cam) - theta) + 180)%360 - 180;
    if (difference < -180) difference += 360;

    if(ABS(difference) <= 20) pose = 1;
    else if(ABS(difference) >= 160) pose = 4;
    else if (ABS(difference) <= 100) pose = 1 + (int)(difference / ABS(difference));
    else if (ABS(difference) > 100) pose = 4 - (int)(difference / ABS(difference));

    vec2 bl_pixel = V2(4.0 + 13*pose, 4.0 + 48.0*p_0->skin + (p_0->ghost ? 22 : 0));
    vec2 player_tex_coords[4] = {bl_pixel*(1.0/256.0), V2(bl_pixel.x, bl_pixel.y+20)*(1.0/256.0),
                                V2(bl_pixel.x+9, bl_pixel.y+20)*(1.0/256.0), V2(bl_pixel.x+9, bl_pixel.y)*(1.0/256.0)};

    mesh_draw(P, V,
                M4_Translation(p_0->world_pos.x, 0.0, -(p_0->world_pos.y))
                * M4_RotationAboutYAxis(angle_to_cam)* M4_Scaling((1.0/5.0)*P1_RADIUS),
                2, sprite_indic, 4, player_verts, NULL, NULL, V3(1.0, 1.0, 0.0), player_tex_coords, "wizard_sprites.png");
}

void draw_sprite_3D(neeble *n_0, int3 *sprite_indic, vec3 *neeble_verts, mat4 P, mat4 V, mat4 C){

    real angle_to_cam = atan2(C(0, 3) - (n_0->world_pos.x), C(2, 3) + (n_0->world_pos.y));

    vec2 bl_pixel = V2((n_0->ball_form ? 84 : 121) + 13*n_0->pose, (n_0->ball_form ? 11 : 4) + 48*n_0->skin);
    int side_len_pix = n_0->ball_form ? 5 : 9;
    vec2 neeble_tex_coords[4] = {bl_pixel*(1.0/256.0), V2(bl_pixel.x, bl_pixel.y+side_len_pix)*(1.0/256.0),
                                V2(bl_pixel.x+side_len_pix, bl_pixel.y+side_len_pix)*(1.0/256.0), V2(bl_pixel.x+side_len_pix, bl_pixel.y)*(1.0/256.0)};

    mesh_draw(P, V,
                M4_Translation(n_0->world_pos.x, n_0->vert_pos, -(n_0->world_pos.y))
                * M4_RotationAboutYAxis(angle_to_cam)*M4_Scaling((1.0/5.0)*P1_RADIUS * (n_0->ball_form ? 1 : 2)),
                2, sprite_indic, 4, neeble_verts, NULL, NULL, V3(1.0, 1.0, 0.0), neeble_tex_coords, "wizard_sprites.png");
}

void draw_particles(mat4 P, mat4 V, mat4 C) {

    int3 sprite_indic[2] = { {0, 1, 3}, {1, 2, 3} };
    vec3 particle_verts[4] = {V3(-0.5, 0.0, 0.0), V3(-0.5, 1.0, 0.0), V3(0.5, 1.0, 0.0), V3(0.5, 0.0, 0.0)};
    int side_len_pix;
    vec2 bl_pixel;
    real angle_to_cam_horizontal;


    for (int i = 0; i < PARTS_PER_PLAYER * NUM_PLAYERS; i++) {
        if (particle_list[i].active) {
            if (norm(V3(C(0, 3), C(1, 3), C(2, 3)) - particle_list[i].position) >= P1_RADIUS * 2) {
                angle_to_cam_horizontal = atan2(C(0, 3) - (particle_list[i].position.x), C(2, 3) - (particle_list[i].position.z));
                switch(particle_list[i].type) {
                    case 0:
                      bl_pixel = V2(110, 39 + 48 * particle_list[i].skin);
                      side_len_pix = 1;
                      break;
                }
                vec2 particle_tex_coords[4] = {bl_pixel*(1.0/256.0), V2(bl_pixel.x, bl_pixel.y+side_len_pix)*(1.0/256.0),
                                            V2(bl_pixel.x+side_len_pix, bl_pixel.y+side_len_pix)*(1.0/256.0), V2(bl_pixel.x+side_len_pix, bl_pixel.y)*(1.0/256.0)};
                mesh_draw(P, V,
                            M4_Translation(particle_list[i].position.x, particle_list[i].position.y, (particle_list[i].position.z))
                            * M4_RotationAboutYAxis(angle_to_cam_horizontal) * M4_Scaling((1.0/5.0)*P1_RADIUS),
                            2, sprite_indic, 4, particle_verts, NULL, NULL, V3(1.0, 1.0, 1.0), particle_tex_coords, "wizard_sprites.png");
            }
        }
    }
}

void update_time() {
    global_time += 0.0167;
    SHOW_CONTROLLERS -= 0.0167;
    update_particles();
}

void app3() {
    Camera3D camera = {5};
    camera.phi = 0.0;
    camera.angle_of_view = 45.0;
    
    vec3 panel_verts[4] = {V3(0.0, 0.0, 0.0), V3(0.0, WORLD_SIZE, 0.0), V3(WORLD_SIZE, WORLD_SIZE, 0.0), V3(WORLD_SIZE, 0.0, 0.0)};
    vec2 panel_tex_coords[4] = {V2(0.0, 0.0), V2(0.0, 1.0), V2(1.0, 1.0), V2(1.0, 0.0)};
    int3 sprite_indic[2] = { {0, 1, 3}, {1, 2, 3} };
    panel = {4, 2, panel_verts, NULL, NULL, sprite_indic, panel_tex_coords, NULL};

    vec3 player_verts[4] = {V3(-4.5, 0.0, 0.0), V3(-4.5, 20.0, 0.0), V3(4.5, 20.0, 0.0), V3(4.5, 0.0, 0.0)};
    vec3 neeble_verts[4] = {V3(-2.5, 0.0, 0.0), V3(-2.5, 5.0, 0.0), V3(2.5, 5.0, 0.0), V3(2.5, 0.0, 0.0)};
    vec3 hud_verts_TL[4] = {V3(0.0, 1.0, 0.0), V3(0.0, 0.0, 0.0), V3(1.0, 0.0, 0.0), V3(1.0, 1.0, 0.0)};
    vec2 window_radius = window_get_size() / 2;
    real window_unit = (sqrt(sqrt(window_radius.x * window_radius.y) * window_radius.y) / 8.0) * 0.135;
    vec2 viewport_unit = V2(window_unit * ((NUM_PLAYERS > 2) + 1), window_unit)* (NUM_PLAYERS > 2 ? 0.8 : 1);
    
    mat4 NDC_from_pixels;
    vec2 curr_window_radius;

    bool player_cam = true;
    MODE_2D = false;

    WORLD = create_world_mesh();

    int music_playing = 0;

    while (cow_begin_frame()) {
        curr_window_radius = window_get_size() / 2;
        NDC_from_pixels = M4_Translation(-1, 1) * M4_Scaling(2.0/window_get_size().x, -2.0/window_get_size().y);

        if (!r_equals(curr_window_radius.x, window_radius.x) || !r_equals(curr_window_radius.y, window_radius.y)) {
            window_radius = curr_window_radius;
            window_unit = (sqrt(sqrt(window_radius.x * window_radius.y) * float(window_radius.y)) / 8.0) * 0.135;
            viewport_unit = V2(window_unit * ((NUM_PLAYERS > 2) + 1), window_unit) * (NUM_PLAYERS > 2 ? 0.8 : 1);
        }
        if (globals.key_pressed['9']) {
            //music_playing = 0;
            //cs_music_stop(-1.0);
            vec2 coords[4];
            coords[0] = V2(4, 256 - 21)*(1.0/256.0);
            coords[1] = V2(4, 256 - 4)*(1.0/256.0);
            coords[2] = V2(66, 256 - 4)*(1.0/256.0);
            coords[3] = V2(66, 256 - 21)*(1.0/256.0);
            mesh_draw(NDC_from_pixels * M4_Translation(window_radius.x, window_radius.y),
                M4_Scaling(window_unit * 20, window_unit * 20), M4_Translation(-31.0/17.0, -0.5) * M4_Scaling(62.0/17.0, 1.0),
                2, sprite_indic, 4, hud_verts_TL, NULL, NULL, V3(1.0, 1.0, 0.0), coords, "wizard_sprites.png");
            
            for(int i = 0; i < NUM_PLAYERS; i++) {
                for(int j = 0; j < TOTAL_NEEBLES; j++) {
                    if (!neeble_list[i*TOTAL_NEEBLES + j].dead) kill_neeble(&neeble_list[i*TOTAL_NEEBLES + j]);
                }
            }
            initialize_game();
            vec3 new_panel_verts[4] = {V3(0.0, 0.0, 0.0), V3(0.0, WORLD_SIZE, 0.0), V3(WORLD_SIZE, WORLD_SIZE, 0.0), V3(WORLD_SIZE, 0.0, 0.0)};
            IndexedTriangleMesh3D new_panel = {4, 2, new_panel_verts, NULL, NULL, sprite_indic, panel_tex_coords, NULL};
            panel = new_panel;
            WORLD = create_world_mesh();

        }
        else if (global_time < 6) {
            if (global_time < 2.0) {
                vec2 coords[4];
                coords[0] = V2(4, 256 - 21)*(1.0/256.0);
                coords[1] = V2(4, 256 - 4)*(1.0/256.0);
                coords[2] = V2(66, 256 - 4)*(1.0/256.0);
                coords[3] = V2(66, 256 - 21)*(1.0/256.0);
                mesh_draw(NDC_from_pixels * M4_Translation(window_radius.x, window_radius.y),
                    M4_Scaling(window_unit * 20, window_unit * 20), M4_Translation(-31.0/17.0, -0.5) * M4_Scaling(62.0/17.0, 1.0),
                    2, sprite_indic, 4, hud_verts_TL, NULL, NULL, V3(1.0, 1.0, 0.0), coords, "wizard_sprites.png");
            }
            float window_width = window_get_size().x;
            float window_height = window_get_size().y;
            float w = MIN(window_width, window_height);
            float scale_x = w / window_width;
            float scale_y = w / window_height;
            draw_map_2D(M4_Scaling(1.1*scale_x/float(GRID_SIDE), 1.1*scale_y/float(GRID_SIDE)), 0.8, (int(float(GRID_SIDE*GRID_SIDE)*(MIN(5.0, global_time)-1.0)))/1.0);
            if (global_time > 3.0) {
                eso_begin(M4_Scaling(1.1*scale_x/float(GRID_SIDE), 1.1*scale_y/float(GRID_SIDE)), SOUP_POINTS, 0.33, true, true);
                eso_color(V3(1.0, 0.0, 1.0));
                for(int i = 0; i < NUM_PLAYERS; i++) {
                    if (((int)((global_time)*4*NUM_PLAYERS))%NUM_PLAYERS == i) eso_vertex(player_list[i].world_pos);
                }
                eso_end();
            }
        }
        else {
            mat4 P = camera_get_P(&camera);
            char teams_alive = 0;

            //if (globals.key_pressed['0']) player_cam = !player_cam;

            for (int h = 0; h < NUM_PLAYERS; h++) {
                if (globals.key_pressed['0'+(player_list[h].p_num)]) {
                    if (SHOW_CONTROLLERS > 0.0) {
                        if (player_list[h].controller == 5) player_list[h].controller = 0;
                        else player_list[h].controller += 1;
                    }
                    SHOW_CONTROLLERS = 3.0;
                }
                C_PLAYERS[h] = fps_camera_get_C(&player_list[h]);
                update_player(&player_list[h]);
                if (player_list[h].lives > 0) teams_alive |= (1 << player_list[h].skin);
            }
            for (int i = 0; i < NUM_PLAYERS; i++) {
                int num_neebles = 0;
                for(int j = 0; j < TOTAL_NEEBLES; j++) {
                    if (!neeble_list[i*(TOTAL_NEEBLES) + j].dead) {
                        update_neeble(&neeble_list[i*(TOTAL_NEEBLES) + j], i*(TOTAL_NEEBLES) + j);
                        num_neebles += 1;
                    }
                }
                player_list[i].neebles_stored = TOTAL_NEEBLES - num_neebles;
            }
            if (player_cam) {
                //glMatrixMode(GL_PROJECTION);
                glEnable(GL_SCISSOR_TEST); // https://registry.khronos.org/OpenGL-Refpages/es2.0/xhtml/glScissor.xml
                for (int i = 0; i < NUM_PLAYERS; i++) {
                    if (NUM_PLAYERS > 2) {
                        if (i % 2 == 0) glViewport(0, 0, window_radius.x, window_radius.y * 2);
                        else glViewport(window_radius.x, 0, window_radius.x, window_radius.y * 2);
                    }
                    else glViewport(0, 0, window_radius.x * 2, window_radius.y * 2);

                    update_hud(&player_list[i]);
                    bool bottom_screen = NUM_PLAYERS > 2 ? i / 2 : i % 2;
                    mat4 P_current;
                    if (NUM_PLAYERS > 2) P_current = M4_Translation(0.0, 0.5 - bottom_screen) * M4_Scaling(0.8, 0.4, 0.8) * P;
                    else P_current = M4_Translation(0.0, 0.5 - bottom_screen) * M4_Scaling(0.5, 0.5, 0.5) * P;
                    mat4 C_current = C_PLAYERS[i];
                    mat4 V_current = inverse(C_current);
    
                    if (!bottom_screen) glScissor(int(0), int(window_radius.y), int(window_radius.x*2), int(window_radius.y));
                    else glScissor(int(0), int(0), int(window_radius.x*2), int(window_radius.y));

                    {
                        WORLD.draw(P_current, V_current, M4_Identity(), monokai.blue);
                        draw_particles(P_current, V_current, C_current);
                        for(int k = 0; k < TOTAL_NEEBLES * NUM_PLAYERS; k++) {
                            if (!neeble_list[k].dead) {
                                draw_sprite_3D(&neeble_list[k], sprite_indic, neeble_verts, P_current, V_current, C_current);
                            }
                        }
                        for (int j = 0; j < NUM_PLAYERS; j++) {
                            if ((GAMEMODE == GM_CLASSIC || player_list[j].ghost || player_list[i].dead) &&
                            player_list[i].p_num != player_list[j].p_num && !player_list[j].dead) {
                                draw_sprite_3D(&player_list[j], sprite_indic, player_verts, P_current, V_current, C_current);
                            }
                        }

                        glDisable(GL_DEPTH_TEST);

                        vec2 coords[4];
                        //pfp
                        get_square_tex_coords(coords, player_list[i].hud_tl[0], 9, 256);
                        mesh_draw(NDC_from_pixels * M4_Translation(viewport_unit.x*1.5, viewport_unit.y*1.5),
                            M4_Translation(0, bottom_screen * window_radius.y), M4_Scaling(viewport_unit.x*9.5, viewport_unit.y*9.5),
                            2, sprite_indic, 4, hud_verts_TL, NULL, NULL, V3(1.0, 1.0, 0.0), coords, "wizard_sprites.png");

                        //alert
                        if (player_list[i].alert && (int)(global_time*15)%3 > 1) {
                            get_square_tex_coords(coords, V2(256 - 40, 256 - 3), 9, 256);
                            mesh_draw(NDC_from_pixels * M4_Translation(viewport_unit.x*11, viewport_unit.y*2),
                                M4_Translation(0, bottom_screen * window_radius.y), M4_Scaling(viewport_unit.x*5, viewport_unit.y * 5),
                                2, sprite_indic, 4, hud_verts_TL, NULL, NULL, V3(1.0, 1.0, 0.0), coords, "wizard_sprites.png");
                        }

                        //hearts
                        for (int j = 0; j < NUM_LIVES; j++) {
                            if (player_list[i].lives >= NUM_LIVES-j) {
                                get_square_tex_coords(coords, V2(95, 46+48*player_list[i].skin), 10, 256);
                            }
                            else {
                                get_square_tex_coords(coords, V2(256-14, 256-3), 10, 256);
                            }
                            mesh_draw(NDC_from_pixels * M4_Translation(window_radius.x * 2 - viewport_unit.x * 5.5 - viewport_unit.x * 5 * j, viewport_unit.y*1.5),
                                                M4_Translation(0, bottom_screen * window_radius.y), M4_Scaling(viewport_unit.x * 3.8, viewport_unit.y * 3.8),
                                                2, sprite_indic, 4, hud_verts_TL, NULL, NULL, V3(1.0, 1.0, 0.0), coords, "wizard_sprites.png");
                        }

                        //ghost clock
                        get_square_tex_coords(coords, player_list[i].hud_tl[1], 9, 256);
                        mesh_draw(NDC_from_pixels * M4_Translation(window_radius.x - viewport_unit.x * 2, viewport_unit.y*2),
                            M4_Translation(0, bottom_screen * window_radius.y), M4_Scaling(viewport_unit.x * 4, viewport_unit.y * 4),
                            2, sprite_indic, 4, hud_verts_TL, NULL, NULL, V3(1.0, 1.0, 0.0), coords, "wizard_sprites.png");

                        //controller
                        if (SHOW_CONTROLLERS > 0.0) {
                            if (player_list[i].controller > 1) {
                                get_square_tex_coords(coords, V2(177 + player_list[i].controller * 13, 256 - 54 - 11 * glfwJoystickPresent(player_list[i].controller - 2)), 9, 256);
                            }
                            else get_square_tex_coords(coords, V2(177 + player_list[i].controller * 13, 256 - 54), 9, 256);
                            mesh_draw(NDC_from_pixels * M4_Translation(viewport_unit.x * 1.5, window_radius.y - viewport_unit.y * 5.5),
                                M4_Translation(0, bottom_screen * window_radius.y), M4_Scaling(viewport_unit.x * 4, viewport_unit.y * 4),
                                2, sprite_indic, 4, hud_verts_TL, NULL, NULL, V3(1.0, 1.0, 0.0), coords, "wizard_sprites.png");
                        }

                        if (!player_list[i].dead) {
                            //crosshair
                            get_square_tex_coords(coords, V2(2, 209), 15, 256);
                            mesh_draw(NDC_from_pixels * M4_Translation(window_radius.x, window_radius.y*0.5),
                                M4_Translation(0, bottom_screen * window_radius.y), M4_Scaling(viewport_unit.x * 2, viewport_unit.y * 2) * M4_Translation(-0.5, -0.5, 0),
                                2, sprite_indic, 4, hud_verts_TL, NULL, NULL, V3(1.0, 1.0, 0.0), coords, "wizard_sprites.png");

                            //mesh_draw(NDC_from_pixels * M4_Translation(window_radius.x * 0.5, window_radius.y * 0.5), globals.Identity, M4_Translation(0, bottom_screen * window_radius.y),
                            //    2, sprite_indic, 4, hud_verts_TL, NULL, NULL, V3(1.0, 1.0, 0.0), NULL, "wizard_sprites.png");

                            //neebs
                            get_square_tex_coords(coords, player_list[i].hud_tl[2], 10, 256);
                            for(int j = 0; j < TOTAL_NEEBLES; j++) {
                                if (player_list[i].neebles_stored > j) {
                                    mesh_draw(NDC_from_pixels * M4_Translation(window_radius.x * 2 - viewport_unit.x * 5.5 - viewport_unit.x * 4.65 * j, viewport_unit.y*6.5),
                                                M4_Translation(0, bottom_screen * window_radius.y), M4_Scaling(viewport_unit.x * 3.7, viewport_unit.y * 3.7),
                                                2, sprite_indic, 4, hud_verts_TL, NULL, NULL, V3(1.0, 1.0, 0.0), coords, "wizard_sprites.png");
                                }
                                else break;
                            }
                        }
                        //skull
                        else if ((int)(global_time*10)%3 == 0) {
                            get_square_tex_coords(coords, V2(256 - 66, 256 - 3), 9, 256);
                            mesh_draw(NDC_from_pixels * M4_Translation(window_radius.x - viewport_unit.x * 5, window_radius.y * 0.5 - viewport_unit.y * 5),
                                M4_Translation(0, bottom_screen * window_radius.y), M4_Scaling(viewport_unit.x * 10, viewport_unit.y * 10),
                                2, sprite_indic, 4, hud_verts_TL, NULL, NULL, V3(1.0, 1.0, 0.0), coords, "wizard_sprites.png");
                        }

                        glEnable(GL_DEPTH_TEST);
                    }
                }
                glDisable(GL_SCISSOR_TEST);
                glViewport(0, 0, window_radius.x * 2, window_radius.y * 2);
                eso_begin(globals.Identity, SOUP_LINES, 2, false, true);
                eso_color(monokai.black);
                eso_vertex(-1, 0);
                eso_vertex(1, 0);
                if (NUM_PLAYERS > 2) {
                    eso_vertex(0, -1);
                    eso_vertex(0, 1);
                }
                eso_end();
                glDisable(GL_DEPTH_TEST);
                if ((teams_alive & (teams_alive - 1)) == 0) {
                    vec2 coords[4];
                    coords[0] = V2(4, 256 - 21)*(1.0/256.0);
                    coords[1] = V2(4, 256 - 4)*(1.0/256.0);
                    coords[2] = V2(66, 256 - 4)*(1.0/256.0);
                    coords[3] = V2(66, 256 - 21)*(1.0/256.0);
                    mesh_draw(NDC_from_pixels * M4_Translation(window_radius.x, window_radius.y),
                        M4_Scaling(window_unit * 20, window_unit * 20), M4_Translation(-31.0/17.0, -0.5) * M4_Scaling(62.0/17.0, 1.0),
                        2, sprite_indic, 4, hud_verts_TL, NULL, NULL, V3(1.0, 1.0, 0.0), coords, "wizard_sprites.png");
                    
                    if (music_playing != 2) {
                        //cs_music_stop(-1.0);
                        //sound_loop_music("neeb2_44.wav");
                        //music_playing = 2;
                    }
                }
                else if(music_playing != 1) {
                    //cs_music_stop(-1.0);
                    //sound_loop_music("neeb2_44.wav");
                    //music_playing = 1;
                }
                glEnable(GL_DEPTH_TEST);
            }
            else{
                glViewport(0, 0, window_radius.x * 2, window_radius.y * 2);
                mat4 V = camera_get_V(&camera);
                mat4 C = camera_get_C(&camera);
                camera_move(&camera);
                WORLD.draw(P, V, M4_Identity(), monokai.white);
                draw_particles(P, V, C);
                for (int i = 0; i < NUM_PLAYERS; i++) {
                    if (!player_list[i].dead) draw_sprite_3D(&player_list[i], sprite_indic, player_verts, P, V, C);
                    for(int j = 0; j < TOTAL_NEEBLES; j++) {
                        if (!neeble_list[i*(TOTAL_NEEBLES) + j].dead) {
                            draw_sprite_3D(&neeble_list[i*(TOTAL_NEEBLES) + j], sprite_indic, neeble_verts, P, V, C);
                        }
                    }
                }
            }
        }
        update_time();
    }
}
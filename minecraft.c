#include <windows.h>
#include <conio.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#define Y_PIXELS 180
#define X_PIXELS 900
#define Z_BLOCKS 10
#define Y_BLOCKS 20
#define EYE_HEIGHT 1.5
#define X_BLOCKS 20
#define VIEW_HEIGHT 0.7
#define VIEW_WIDTH 1
#define BLOCK_BORDER_SIZE 0.05

static DWORD old_stdin_mode, old_stdout_mode;

// vect represents a 3D vector in cartesian coordinate system
typedef struct Vector {
    float x;
    float y;
    float z;
} vect;

// vect2 represents a pair of angles, likely for describing orientation or spherical coordinates.
typedef struct Vector2 {
    float psi;
    float phi;
} vect2;

typedef struct Vector_vector2 {
    vect pos;
    vect2 view;
} player_pos_view;

// Prepare Windows console: disable line input & echo, enable ANSI on output
void init_terminal() {
    HANDLE hStdin  = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);

    GetConsoleMode(hStdin,  &old_stdin_mode);
    GetConsoleMode(hStdout, &old_stdout_mode);

    // disable echo and line buffering
    SetConsoleMode(hStdin,
        old_stdin_mode & ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT));

    // enable ANSI escape codes on output
    SetConsoleMode(hStdout,
        old_stdout_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    fflush(stdout);
}

// Restore original console modes
void restore_terminal() {
    HANDLE hStdin  = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleMode(hStdin,  old_stdin_mode);
    SetConsoleMode(hStdout, old_stdout_mode);
    printf("terminal restored");
}

// to cover all possible ASCII values
static char keystate[256] = { 0 };

// Poll any pending keypresses into keystate[]
void process_input() {
    memset(keystate, 0, sizeof(keystate));
    // _kbhit returns nonzero if a key is waiting
    while (_kbhit()) {
        int c = _getch();  // read one character (no echo)
        keystate[(unsigned char)c] = 1;
    }
}

// check if the key is pressed
int is_key_pressed(char key) {
    return keystate[(unsigned char)key];
}

// initialise a image in the program by creating a image buffer
char** init_picture() {
    char** picture = malloc(sizeof(char*) * Y_PIXELS);
    if (!picture) { perror("Failed to allocate picture"); exit(EXIT_FAILURE); }
    for (int i = 0; i < Y_PIXELS; i++) {
        picture[i] = malloc(sizeof(char) * X_PIXELS);
        if (!picture[i]) { perror("Failed to allocate picture row"); exit(EXIT_FAILURE); }
    }
    return picture;
}

// make/initialise a grid based block for the game
char*** init_blocks() {
    char*** blocks = malloc(sizeof(char**) * Z_BLOCKS);
    if (!blocks) { perror("Failed to allocate blocks"); exit(EXIT_FAILURE); }
    for (int i = 0; i < Z_BLOCKS; i++) {
        blocks[i] = malloc(sizeof(char*) * Y_BLOCKS);
        if (!blocks[i]) { perror("Failed to allocate blocks layer"); exit(EXIT_FAILURE); }
        for (int j = 0; j < Y_BLOCKS; j++) {
            blocks[i][j] = malloc(sizeof(char) * X_BLOCKS);
            if (!blocks[i][j]) { perror("Failed to allocate blocks row"); exit(EXIT_FAILURE); }
            for (int k = 0; k < X_BLOCKS; k++) {
                blocks[i][j][k] = ' ';
            }
        }
    }
    return blocks;
}

// initializes and returns the starting position and viewing angles of the player in the game
player_pos_view init_posview() {
    player_pos_view posview;
    posview.pos.x   = 5;
    posview.pos.y   = 5;
    posview.pos.z   = 4 + EYE_HEIGHT;
    posview.view.psi = 0;
    posview.view.phi = 0;
    return posview;
}

// convert the posview angles into vect
vect angles_to_vect(vect2 angles) {
    vect res;
    res.x = cosf(angles.psi) * cosf(angles.phi);
    res.y = cosf(angles.psi) * sinf(angles.phi);
    res.z = sinf(angles.psi);
    return res;
}

// vector addition code
vect vect_add(vect v1, vect v2) {
    vect res = { v1.x+v2.x, v1.y+v2.y, v1.z+v2.z };
    return res;
}

// scalar multiplication of a vectior v with constant a
vect vect_scale(float s, vect v) {
    vect res = { s*v.x, s*v.y, s*v.z };
    return res;
}

// vector subtraction code
vect vect_sub(vect v1, vect v2) {
    return vect_add(v1, vect_scale(-1, v2));
}

// finding the unit vector for the vector
void vect_normalize(vect* v) {
    float len = sqrtf(v->x*v->x + v->y*v->y + v->z*v->z);
    if (len > 0) {
        v->x /= len;
        v->y /= len;
        v->z /= len;
    }
}

// initializes a 2D array of direction vectors for each pixel on a screen
vect** init_directions(vect2 view) {
    view.psi -= VIEW_HEIGHT/2.0f;
    vect screen_down = angles_to_vect(view);
    view.psi += VIEW_HEIGHT;
    vect screen_up   = angles_to_vect(view);
    view.psi -= VIEW_HEIGHT/2.0f;
    view.phi -= VIEW_WIDTH/2.0f;
    vect screen_left  = angles_to_vect(view);
    view.phi += VIEW_WIDTH;
    vect screen_right = angles_to_vect(view);
    view.phi -= VIEW_WIDTH/2.0f;

    vect screen_mid_vert = vect_scale(0.5f, vect_add(screen_up,   screen_down));
    vect screen_mid_hor  = vect_scale(0.5f, vect_add(screen_left, screen_right));
    vect mid_to_left = vect_sub(screen_left,  screen_mid_hor);
    vect mid_to_up   = vect_sub(screen_up,    screen_mid_vert);

    vect** dir = malloc(sizeof(vect*) * Y_PIXELS);
    if (!dir) { perror("Failed to allocate directions"); exit(EXIT_FAILURE); }
    for (int i = 0; i < Y_PIXELS; i++) {
        dir[i] = malloc(sizeof(vect) * X_PIXELS);
        if (!dir[i]) { perror("Failed to allocate directions row"); exit(EXIT_FAILURE); }
    }
    for (int y = 0; y < Y_PIXELS; y++) {
        for (int x = 0; x < X_PIXELS; x++) {
            vect tmp = vect_add(vect_add(screen_mid_hor, mid_to_left), mid_to_up);
            tmp = vect_sub(tmp, vect_scale(((float)x/(X_PIXELS-1))*2, mid_to_left));
            tmp = vect_sub(tmp, vect_scale(((float)y/(Y_PIXELS-1))*2, mid_to_up));
            vect_normalize(&tmp);
            dir[y][x] = tmp;
        }
    }
    return dir;
}

// This function checks if a 3D point pos is outside the allowed block boundaries.
int ray_outside(vect pos) {
    return (pos.x<0||pos.x>=X_BLOCKS ||
            pos.y<0||pos.y>=Y_BLOCKS ||
            pos.z<0||pos.z>=Z_BLOCKS);
}

// This function checks if a position lies on the border between two or more blocks.
int on_block_border(vect pos) {
    int cnt = 0;
    if (fabsf(pos.x - roundf(pos.x)) < BLOCK_BORDER_SIZE) cnt++;
    if (fabsf(pos.y - roundf(pos.y)) < BLOCK_BORDER_SIZE) cnt++;
    if (fabsf(pos.z - roundf(pos.z)) < BLOCK_BORDER_SIZE) cnt++;
    return (cnt >= 2);
}

float minf(float a, float b) { return a < b ? a : b; }

/*
    it's a classic voxel ray traversal algorithm, used in things like 
    Minecraft-style rendering or ray marching in a voxel grid.
*/
char raytrace(vect pos, vect dir, char*** blocks) {
    const float eps = 0.01f;
    while (!ray_outside(pos)) {
        int x = (int)pos.x, y = (int)pos.y, z = (int)pos.z;
        if (z<0||z>=Z_BLOCKS||y<0||y>=Y_BLOCKS||x<0||x>=X_BLOCKS) break;
        char c = blocks[z][y][x];
        if (c != ' ') {
            return on_block_border(pos) ? '-' : c;
        }
        float dist = 2;
        if      (dir.x > eps)  dist = minf(dist, ((int)(pos.x+1)-pos.x)/dir.x);
        else if (dir.x < -eps) dist = minf(dist, ((int)pos.x -pos.x)/dir.x);
        if      (dir.y > eps)  dist = minf(dist, ((int)(pos.y+1)-pos.y)/dir.y);
        else if (dir.y < -eps) dist = minf(dist, ((int)pos.y -pos.y)/dir.y);
        if      (dir.z > eps)  dist = minf(dist, ((int)(pos.z+1)-pos.z)/dir.z);
        else if (dir.z < -eps) dist = minf(dist, ((int)pos.z -pos.z)/dir.z);

        pos = vect_add(pos, vect_scale(dist+eps, dir));
    }
    return ' ';
}

/*
    part of the ASCII raytracer pipeline that takes the player's position and view, 
    traces rays into the 3D world, and fills in a 2D ASCII picture.
*/
void get_picture(char** picture, player_pos_view posview, char*** blocks) {
    vect** directions = init_directions(posview.view);
    for (int y = 0; y < Y_PIXELS; y++) {
        for (int x = 0; x < X_PIXELS; x++) {
            picture[y][x] = raytrace(posview.pos, directions[y][x], blocks);
        }
    }
    for (int i = 0; i < Y_PIXELS; i++) free(directions[i]);
    free(directions);
}

void draw_ascii(char** picture) {
    fflush(stdout);
    // ANSI: move cursor to 0,0
    printf("\033[0;0H");
    for (int i = 0; i < Y_PIXELS; i++) {
        int current_color = 0;
        for (int j = 0; j < X_PIXELS; j++) {
            if (picture[i][j] == 'o' && current_color != 32) {
                printf("\x1B[32m"); current_color = 32;
            } else if (picture[i][j] != 'o' && current_color != 0) {
                printf("\x1B[0m");  current_color = 0;
            }
            putchar(picture[i][j]);
        }
        printf("\x1B[0m\n");
    }
}

// Function to update the player's position and viewing direction based on input
void update_pos_view(player_pos_view* posview, char*** blocks) {
    float move_eps = 0.30f;  // Movement speed
    float tilt_eps = 0.1f;   // Rotation speed

    int x = (int)posview->pos.x;
    int y = (int)posview->pos.y;
    int z = (int)(posview->pos.z - EYE_HEIGHT + 0.01f);

    // Push up/down to avoid embedding/floating
    if (x>=0&&x<X_BLOCKS&&y>=0&&y<Y_BLOCKS&&z>=0&&z<Z_BLOCKS && blocks[z][y][x] != ' ')
        posview->pos.z += 1;
    z = (int)(posview->pos.z - EYE_HEIGHT - 0.01f);
    if (x>=0&&x<X_BLOCKS&&y>=0&&y<Y_BLOCKS&&z>=0&&z<Z_BLOCKS && blocks[z][y][x] == ' ')
        posview->pos.z -= 1;

    // rotation
    if (is_key_pressed('w')) posview->view.psi += tilt_eps;
    if (is_key_pressed('s')) posview->view.psi -= tilt_eps;
    if (is_key_pressed('d')) posview->view.phi += tilt_eps;
    if (is_key_pressed('a')) posview->view.phi -= tilt_eps;
    // clamp pitch
    if (posview->view.psi >  M_PI/2) posview->view.psi =  M_PI/2;
    if (posview->view.psi < -M_PI/2) posview->view.psi = -M_PI/2;

    vect direction = angles_to_vect(posview->view);
    // movement
    if (is_key_pressed('i')) {
        posview->pos.x += move_eps * direction.x;
        posview->pos.y += move_eps * direction.y;
    }
    if (is_key_pressed('k')) {
        posview->pos.x -= move_eps * direction.x;
        posview->pos.y -= move_eps * direction.y;
    }
    if (is_key_pressed('j')) {
        posview->pos.x += move_eps * direction.y;
        posview->pos.y -= move_eps * direction.x;
    }
    if (is_key_pressed('l')) {
        posview->pos.x -= move_eps * direction.y;
        posview->pos.y += move_eps * direction.x;
    }

    // clamp to world
    if (posview->pos.x < 0) posview->pos.x = 0;
    if (posview->pos.x >= X_BLOCKS) posview->pos.x = X_BLOCKS - 0.01f;
    if (posview->pos.y < 0) posview->pos.y = 0;
    if (posview->pos.y >= Y_BLOCKS) posview->pos.y = Y_BLOCKS - 0.01f;
    if (posview->pos.z < EYE_HEIGHT) posview->pos.z = EYE_HEIGHT;
    if (posview->pos.z >= Z_BLOCKS) posview->pos.z = Z_BLOCKS - 0.01f;
}

// Raycast to find the first non-empty block in view
vect get_current_block(player_pos_view posview, char*** blocks) {
    vect pos = posview.pos;
    vect dir = angles_to_vect(posview.view);
    const float eps = 0.01f;

    while (!ray_outside(pos)) {
        int x = (int)pos.x, y = (int)pos.y, z = (int)pos.z;
        if (z<0||z>=Z_BLOCKS||y<0||y>=Y_BLOCKS||x<0||x>=X_BLOCKS) break;
        if (blocks[z][y][x] != ' ')
            return pos;
        float dist = 2;
        if      (dir.x > eps)  dist = minf(dist, ((int)(pos.x+1)-pos.x)/dir.x);
        else if (dir.x < -eps) dist = minf(dist, ((int)pos.x -pos.x)/dir.x);
        if      (dir.y > eps)  dist = minf(dist, ((int)(pos.y+1)-pos.y)/dir.y);
        else if (dir.y < -eps) dist = minf(dist, ((int)pos.y -pos.y)/dir.y);
        if      (dir.z > eps)  dist = minf(dist, ((int)(pos.z+1)-pos.z)/dir.z);
        else if (dir.z < -eps) dist = minf(dist, ((int)pos.z -pos.z)/dir.z);

        pos = vect_add(pos, vect_scale(dist+eps, dir));
    }
    return pos;
}

// Place a block adjacent to the looked-at block
void place_block(vect pos, char*** blocks, char block) {
    int x = (int)pos.x, y = (int)pos.y, z = (int)pos.z;
    if (z<0||z>=Z_BLOCKS||y<0||y>=Y_BLOCKS||x<0||x>=X_BLOCKS) return;

    float dists[6] = {
        fabsf(x+1 - pos.x),  fabsf(pos.x - x),
        fabsf(y+1 - pos.y),  fabsf(pos.y - y),
        fabsf(z+1 - pos.z),  fabsf(pos.z - z)
    };
    int min_idx = 0; float mind = dists[0];
    for (int i = 1; i < 6; i++) {
        if (dists[i] < mind) { mind = dists[i]; min_idx = i; }
    }
    switch (min_idx) {
        case 0: if (x+1 < X_BLOCKS) blocks[z][y][x+1] = block; break;
        case 1: if (x-1 >= 0)      blocks[z][y][x-1] = block; break;
        case 2: if (y+1 < Y_BLOCKS) blocks[z][y+1][x] = block; break;
        case 3: if (y-1 >= 0)      blocks[z][y-1][x] = block; break;
        case 4: if (z+1 < Z_BLOCKS) blocks[z+1][y][x] = block; break;
        case 5: if (z-1 >= 0)      blocks[z-1][y][x] = block; break;
    }
}

int main() {
    init_terminal();                         // Prepare terminal for drawing
    char** picture = init_picture();         // Create 2D array for ASCII rendering
    char*** blocks  = init_blocks();         // Initialize 3D block world

    // Create a flat ground of blocks ('@') in the lower levels
    for (int x = 0; x < X_BLOCKS; x++)
    for (int y = 0; y < Y_BLOCKS; y++)
    for (int z = 0; z < 4; z++)
        blocks[z][y][x] = '@';

    player_pos_view posview = init_posview(); // Initialize player position and view

    while (1) {
        process_input();                      // Read user input

        if (is_key_pressed('q')) break;      // Quit if 'q' is pressed

        update_pos_view(&posview, blocks);   // Move/rotate player

        vect current_block = get_current_block(posview, blocks);
        int have_current_block = !ray_outside(current_block);
        int cx = (int)current_block.x,
            cy = (int)current_block.y,
            cz = (int)current_block.z;
        char old_c = ' ', removed = 0;

        if (have_current_block &&
            cz>=0&&cz<Z_BLOCKS&&cy>=0&&cy<Y_BLOCKS&&cx>=0&&cx<X_BLOCKS) {
            old_c = blocks[cz][cy][cx];
            blocks[cz][cy][cx] = 'o';
            if (is_key_pressed('x')) {
                removed = 1;
                blocks[cz][cy][cx] = ' ';
            }
            if (is_key_pressed(' ')) {
                place_block(current_block, blocks, '@');
            }
        }

        get_picture(picture, posview, blocks);

        // Restore the original character if block wasn’t removed
        if (have_current_block && !removed &&
            cz>=0&&cz<Z_BLOCKS&&cy>=0&&cy<Y_BLOCKS&&cx>=0&&cx<X_BLOCKS) {
            blocks[cz][cy][cx] = old_c;
        }

        draw_ascii(picture);       // Render the ASCII screen
        Sleep(20);                 // Sleep for 20 ms (~50 FPS)
    }

    // Free allocated memory
    for (int i = 0; i < Y_PIXELS; i++) free(picture[i]);
    free(picture);
    for (int z = 0; z < Z_BLOCKS; z++) {
        for (int y = 0; y < Y_BLOCKS; y++) free(blocks[z][y]);
        free(blocks[z]);
    }
    free(blocks);

    restore_terminal();           // Reset terminal on exit
    return 0;
}

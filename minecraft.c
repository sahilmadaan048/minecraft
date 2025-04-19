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
#define EYE_HEIGHT 1.5f
#define X_BLOCKS 20
#define VIEW_HEIGHT 0.7f
#define VIEW_WIDTH 1
#define BLOCK_BORDER_SIZE 0.05f

static DWORD old_stdin_mode, old_stdout_mode;
static CONSOLE_CURSOR_INFO old_cursor_info;

// vect represents a 3D vector in cartesian coordinate system
typedef struct Vector {
    float x;
    float y;
    float z;
} vect;

// vect2 represents a pair of angles in spherical coordinates
typedef struct Vector2 {
    float psi;
    float phi;
} vect2;

// stores player position and view angles
typedef struct Vector_vector2 {
    vect pos;
    vect2 view;
} player_pos_view;

// Prepare Windows console: disable line input & echo, enable ANSI, hide cursor
void init_terminal() {
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);

    GetConsoleMode(hStdin, &old_stdin_mode);
    GetConsoleMode(hStdout, &old_stdout_mode);
    SetConsoleMode(hStdin,
        old_stdin_mode & ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT));
    SetConsoleMode(hStdout,
        old_stdout_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    // Hide console cursor
    GetConsoleCursorInfo(hStdout, &old_cursor_info);
    CONSOLE_CURSOR_INFO ci = old_cursor_info;
    ci.bVisible = FALSE;
    SetConsoleCursorInfo(hStdout, &ci);

    fflush(stdout);
}

// Restore original console modes and cursor visibility
void restore_terminal() {
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleMode(hStdin, old_stdin_mode);
    SetConsoleMode(hStdout, old_stdout_mode);
    SetConsoleCursorInfo(hStdout, &old_cursor_info);
    printf("terminal restored\n");
}

// to cover all possible ASCII values
static char keystate[256] = { 0 };

// Poll any pending keypresses into keystate[] (non-blocking)
void process_input() {
    memset(keystate, 0, sizeof(keystate));
    while (_kbhit()) {
        int c = _getch();  // read one character (no echo)
        keystate[(unsigned char)c] = 1;
    }
}

// check if the key is pressed
int is_key_pressed(char key) {
    return keystate[(unsigned char)key];
}

// initialise an image buffer
char** init_picture() {
    char** picture = malloc(sizeof(char*) * Y_PIXELS);
    if (!picture) { perror("Failed to allocate picture"); exit(EXIT_FAILURE); }
    for (int i = 0; i < Y_PIXELS; i++) {
        picture[i] = malloc(X_PIXELS);
        if (!picture[i]) { perror("Failed to allocate row"); exit(EXIT_FAILURE); }
    }
    return picture;
}

// initialise a 3D block grid
char*** init_blocks() {
    char*** blocks = malloc(sizeof(char**) * Z_BLOCKS);
    if (!blocks) { perror("Failed to allocate blocks"); exit(EXIT_FAILURE); }
    for (int z = 0; z < Z_BLOCKS; z++) {
        blocks[z] = malloc(sizeof(char*) * Y_BLOCKS);
        if (!blocks[z]) { perror("Failed to alloc layer"); exit(EXIT_FAILURE); }
        for (int y = 0; y < Y_BLOCKS; y++) {
            blocks[z][y] = malloc(X_BLOCKS);
            if (!blocks[z][y]) { perror("Failed to alloc row"); exit(EXIT_FAILURE); }
            for (int x = 0; x < X_BLOCKS; x++) blocks[z][y][x] = ' ';
        }
    }
    return blocks;
}

// initialize player position and view
player_pos_view init_posview() {
    player_pos_view pv;
    pv.pos.x = 5; pv.pos.y = 5; pv.pos.z = 4 + EYE_HEIGHT;
    pv.view.psi = 0; pv.view.phi = 0;
    return pv;
}

// convert spherical angles to direction vector
vect angles_to_vect(vect2 a) {
    vect r;
    r.x = cosf(a.psi) * cosf(a.phi);
    r.y = cosf(a.psi) * sinf(a.phi);
    r.z = sinf(a.psi);
    return r;
}

// vector addition
// typedef vect vect_add(vect a, vect b);
vect vect_add(vect a, vect b) { return (vect) { a.x + b.x, a.y + b.y, a.z + b.z }; }

// scalar multiplication
vect vect_scale(float s, vect v) { return (vect) { s* v.x, s* v.y, s* v.z }; }

// vector subtraction
vect vect_sub(vect a, vect b) { return vect_add(a, vect_scale(-1.0f, b)); }

// minimum of two floats
float minf(float a, float b) { return a < b ? a : b; }

// normalize vector to unit length
void vect_normalize(vect* v) {
    float len = sqrtf(v->x * v->x + v->y * v->y + v->z * v->z);
    if (len > 0.0f) { v->x /= len; v->y /= len; v->z /= len; }
}

// check if position is outside the voxel grid
int ray_outside(vect p) {
    return p.x < 0 || p.x >= X_BLOCKS ||
        p.y < 0 || p.y >= Y_BLOCKS ||
        p.z < 0 || p.z >= Z_BLOCKS;
}

// detect if ray is exactly on a block border
int on_block_border(vect p) {
    int cnt = 0;
    if (fabsf(p.x - roundf(p.x)) < BLOCK_BORDER_SIZE) cnt++;
    if (fabsf(p.y - roundf(p.y)) < BLOCK_BORDER_SIZE) cnt++;
    if (fabsf(p.z - roundf(p.z)) < BLOCK_BORDER_SIZE) cnt++;
    return cnt >= 2;
}

// initialize per-pixel direction vectors for the view frustum
vect** init_directions(vect2 view) {
    view.psi -= VIEW_HEIGHT / 2.0f;
    vect sd = angles_to_vect(view);

    view.psi += VIEW_HEIGHT;
    vect su = angles_to_vect(view);

    view.psi -= VIEW_HEIGHT / 2.0f;
    view.phi -= VIEW_WIDTH / 2.0f;
    vect sl = angles_to_vect(view);

    view.phi += VIEW_WIDTH;
    vect sr = angles_to_vect(view);

    view.phi -= VIEW_WIDTH / 2.0f;

    vect smv = vect_scale(0.5f, vect_add(su, sd));
    vect smh = vect_scale(0.5f, vect_add(sl, sr));
    vect ml = vect_sub(sl, smh);
    vect mu = vect_sub(su, smv);

    vect** dirs = malloc(sizeof(vect*) * Y_PIXELS);
    if (!dirs) { perror("Failed to alloc dirs"); exit(EXIT_FAILURE); }
    for (int y = 0; y < Y_PIXELS; y++) {
        dirs[y] = malloc(sizeof(vect) * X_PIXELS);
        if (!dirs[y]) { perror("Failed to alloc dir row"); exit(EXIT_FAILURE); }
        for (int x = 0; x < X_PIXELS; x++) {
            vect tmp = vect_add(vect_add(smh, ml), mu);
            tmp = vect_sub(tmp, vect_scale(((float)x / (X_PIXELS - 1)) * 2.0f, ml));
            tmp = vect_sub(tmp, vect_scale(((float)y / (Y_PIXELS - 1)) * 2.0f, mu));
            vect_normalize(&tmp);
            dirs[y][x] = tmp;
        }
    }
    return dirs;
}

// trace a single ray through the voxel grid
char raytrace(vect pos, vect dir, char*** blocks) {
    const float eps = 0.01f;
    while (!ray_outside(pos)) {
        int x = (int)pos.x;
        int y = (int)pos.y;
        int z = (int)pos.z;
        char c = blocks[z][y][x];
        if (c != ' ')
            return on_block_border(pos) ? '-' : c;

        float dist = 2.0f;
        if (dir.x > eps)  dist = minf(dist, ((int)(pos.x + 1) - pos.x) / dir.x);
        else if (dir.x < -eps) dist = minf(dist, ((int)pos.x - pos.x) / dir.x);
        if (dir.y > eps)  dist = minf(dist, ((int)(pos.y + 1) - pos.y) / dir.y);
        else if (dir.y < -eps) dist = minf(dist, ((int)pos.y - pos.y) / dir.y);
        if (dir.z > eps)  dist = minf(dist, ((int)(pos.z + 1) - pos.z) / dir.z);
        else if (dir.z < -eps) dist = minf(dist, ((int)pos.z - pos.z) / dir.z);

        pos = vect_add(pos, vect_scale(dist + eps, dir));
    }
    return ' ';
}

// fill ASCII frame buffer by raytracing each pixel
void get_picture(char** pic, player_pos_view pv, char*** blocks) {
    vect** dirs = init_directions(pv.view);
    for (int y = 0; y < Y_PIXELS; y++) {
        for (int x = 0; x < X_PIXELS; x++) {
            pic[y][x] = raytrace(pv.pos, dirs[y][x], blocks);
        }
        free(dirs[y]);
    }
    free(dirs);
}

// draw the ASCII frame to console
void draw_ascii(char** pic) {
    printf("\033[0;0H");
    fflush(stdout);
    for (int i = 0; i < Y_PIXELS; i++) {
        int current_color = 0;
        for (int j = 0; j < X_PIXELS; j++) {
            if (pic[i][j] == 'o' && current_color != 32) {
                printf("\x1B[32m");
                current_color = 32;
            }
            else if (pic[i][j] != 'o' && current_color != 0) {
                printf("\x1B[0m");
                current_color = 0;
            }
            putchar(pic[i][j]);
        }
        printf("\x1B[0m\n");
    }
}

//delete a block
void delete_block(player_pos_view* pv, char*** blocks) {
    vect pos = pv->pos;
    vect dir = angles_to_vect(pv->view);
    const float eps = 0.01f;
    while (!ray_outside(pos)) {
        int x = (int)pos.x, y = (int)pos.y, z = (int)pos.z;
        if (blocks[z][y][x] != ' ') {
            blocks[z][y][x] = ' ';
            break;
        }
        float d = 2.0f;
        if (dir.x > eps) d = minf(d, (((int)(pos.x + 1) - pos.x) / dir.x));
        else if (dir.x < -eps) d = minf(d, (((int)pos.x - pos.x) / dir.x));
        if (dir.y > eps) d = minf(d, (((int)(pos.y + 1) - pos.y) / dir.y));
        else if (dir.y < -eps) d = minf(d, (((int)pos.y - pos.y) / dir.y));
        if (dir.z > eps) d = minf(d, (((int)(pos.z + 1) - pos.z) / dir.z));
        else if (dir.z < -eps) d = minf(d, (((int)pos.z - pos.z) / dir.z));
        pos = vect_add(pos, vect_scale(d + eps, dir));
    }
}

// update player position & view based on input
void update_pos_view(player_pos_view* pv, char*** blocks) {
    float move_eps = 0.30f;
    float tilt_eps = 0.1f;
    int x = (int)pv->pos.x;
    int y = (int)pv->pos.y;
    int z = (int)(pv->pos.z - EYE_HEIGHT + 0.01f);
    if (x >= 0 && x < X_BLOCKS && y >= 0 && y < Y_BLOCKS && z >= 0 && z < Z_BLOCKS && blocks[z][y][x] != ' ')
        pv->pos.z++;
    z = (int)(pv->pos.z - EYE_HEIGHT - 0.01f);
    if (x >= 0 && x < X_BLOCKS && y >= 0 && y < Y_BLOCKS && z >= 0 && z < Z_BLOCKS && blocks[z][y][x] == ' ')
        pv->pos.z--;

    if (is_key_pressed('w')) pv->view.psi += tilt_eps;
    if (is_key_pressed('s')) pv->view.psi -= tilt_eps;
    if (is_key_pressed('d')) pv->view.phi += tilt_eps;
    if (is_key_pressed('a')) pv->view.phi -= tilt_eps;
    if (is_key_pressed('x')) {delete_block(pv, blocks);}

    if (pv->view.psi > M_PI / 2) pv->view.psi = M_PI / 2;
    if (pv->view.psi < -M_PI / 2) pv->view.psi = -M_PI / 2;

    vect dir = angles_to_vect(pv->view);
    if (is_key_pressed('i')) { pv->pos.x += move_eps * dir.x; pv->pos.y += move_eps * dir.y; }
    if (is_key_pressed('k')) { pv->pos.x -= move_eps * dir.x; pv->pos.y -= move_eps * dir.y; }
    if (is_key_pressed('j')) { pv->pos.x += move_eps * dir.y; pv->pos.y -= move_eps * dir.x; }
    if (is_key_pressed('l')) { pv->pos.x -= move_eps * dir.y; pv->pos.y += move_eps * dir.x; }

    if (pv->pos.x < 0) pv->pos.x = 0;
    if (pv->pos.x >= X_BLOCKS) pv->pos.x = X_BLOCKS - 0.01f;
    if (pv->pos.y < 0) pv->pos.y = 0;
    if (pv->pos.y >= Y_BLOCKS) pv->pos.y = Y_BLOCKS - 0.01f;
    if (pv->pos.z < EYE_HEIGHT) pv->pos.z = EYE_HEIGHT;
    if (pv->pos.z >= Z_BLOCKS) pv->pos.z = Z_BLOCKS - 0.01f;
}

// find first non-empty block in view
vect get_current_block(player_pos_view pv, char*** blocks) {
    vect pos = pv.pos;
    vect dir = angles_to_vect(pv.view);
    const float eps = 0.01f;
    while (!ray_outside(pos)) {
        int x = (int)pos.x;
        int y = (int)pos.y;
        int z = (int)pos.z;
        if (blocks[z][y][x] != ' ') return pos;
        float dist = 2.0f;
        if (dir.x > eps)  dist = minf(dist, ((int)(pos.x + 1) - pos.x) / dir.x);
        else if (dir.x < -eps) dist = minf(dist, ((int)pos.x - pos.x) / dir.x);
        if (dir.y > eps)  dist = minf(dist, ((int)(pos.y + 1) - pos.y) / dir.y);
        else if (dir.y < -eps) dist = minf(dist, ((int)pos.y - pos.y) / dir.y);
        if (dir.z > eps)  dist = minf(dist, ((int)(pos.z + 1) - pos.z) / dir.z);
        else if (dir.z < -eps) dist = minf(dist, ((int)pos.z - pos.z) / dir.z);
        pos = vect_add(pos, vect_scale(dist + eps, dir));
    }
    return pos;
}

// place a new block adjacent to the looked-at block
void place_block(vect p, char*** blocks, char b) {
    int x = (int)p.x, y = (int)p.y, z = (int)p.z;
    if (x < 0 || x >= X_BLOCKS || y < 0 || y >= Y_BLOCKS || z < 0 || z >= Z_BLOCKS) return;
    float d[6] = { fabsf(x + 1 - p.x), fabsf(p.x - x), fabsf(y + 1 - p.y), fabsf(p.y - y), fabsf(z + 1 - p.z), fabsf(p.z - z) };
    int mi = 0; float md = d[0]; for (int i = 1;i < 6;i++) if (d[i] < md) { md = d[i]; mi = i; }
    switch (mi) {
    case 0: if (x + 1 < X_BLOCKS) blocks[z][y][x + 1] = b; break;
    case 1: if (x - 1 >= 0)      blocks[z][y][x - 1] = b; break;
    case 2: if (y + 1 < Y_BLOCKS) blocks[z][y + 1][x] = b; break;
    case 3: if (y - 1 >= 0)      blocks[z][y - 1][x] = b; break;
    case 4: if (z + 1 < Z_BLOCKS) blocks[z + 1][y][x] = b; break;
    case 5: if (z - 1 >= 0)      blocks[z - 1][y][x] = b; break;
    }
}

int main() {
    init_terminal();
    char** picture = init_picture();
    char*** blocks = init_blocks();
    for (int x = 0;x < X_BLOCKS;x++) for (int y = 0;y < Y_BLOCKS;y++) for (int z = 0;z < 4;z++) blocks[z][y][x] = '@';
    player_pos_view pv = init_posview();
    while (1) {
        process_input();
        if (is_key_pressed('q')) break;
        update_pos_view(&pv, blocks);
        vect cb = get_current_block(pv, blocks);
        int have = !ray_outside(cb);
        int cx = (int)cb.x, cy = (int)cb.y, cz = (int)cb.z;
        char oldc = ' ', removed = 0;
        if (have && cx >= 0 && cx < X_BLOCKS && cy >= 0 && cy < Y_BLOCKS && cz >= 0 && cz < Z_BLOCKS) {
            oldc = blocks[cz][cy][cx];
            blocks[cz][cy][cx] = 'o';
            if (is_key_pressed('x')) { removed = 1; blocks[cz][cy][cx] = ' '; }
            if (is_key_pressed(' ')) place_block(cb, blocks, '@');
        }
        get_picture(picture, pv, blocks);
        if (have && !removed) blocks[cz][cy][cx] = oldc;
        draw_ascii(picture);
        Sleep(0);
    }
    for (int i = 0; i < Y_PIXELS; i++) free(picture[i]); free(picture);
    for (int z = 0; z < Z_BLOCKS; z++) { for (int y = 0; y < Y_BLOCKS; y++) free(blocks[z][y]); free(blocks[z]); }
    free(blocks);
    restore_terminal();
    return 0;
}
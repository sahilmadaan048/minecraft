#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#endif
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>

#define Y_PIXELS 180
#define X_PIXELS 900
#define Z_BLOCKS 10
#define Y_BLOCKS 20
#define EYE_HEIGHT 1.5
#define X_BLOCKS 20
#define VIEW_HEIGHT 0.7
#define VIEW_WIDTH 1
#define BLOCK_BORDER_SIZE 0.05

#ifdef _WIN32
static HANDLE hStdin;
static DWORD old_console_mode;
#else
static struct termios old_termios, new_termios;
#endif

// to cover all possible ASCII values
static char keystate[256] = { 0 };

// vect represents a 3D vector in cartesian coordinate system
typedef struct Vector {
    float x;
    float y;
    float z;
} vect;

// vect2 represents a pair of angles (spherical coordinates)
typedef struct Vector2 {
    float psi;
    float phi;
} vect2;

// combines position and view angles for the player
typedef struct Vector_vector2 {
    vect pos;
    vect2 view;
} player_pos_view;

#ifdef _WIN32
// Initialize Windows console for raw input
void init_terminal() {
    hStdin = GetStdHandle(STD_INPUT_HANDLE);
    GetConsoleMode(hStdin, &old_console_mode);
    DWORD mode = old_console_mode;
    // disable line input, echo, and quick edit
    mode &= ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_QUICK_EDIT_MODE);
    // enable window input to receive KEY_EVENTs
    mode |= ENABLE_WINDOW_INPUT;
    SetConsoleMode(hStdin, mode);
}

// Restore original console mode
void restore_terminal() {
    SetConsoleMode(hStdin, old_console_mode);
    printf("terminal restored\n");
}

// Read all pending key events, updating keystate instantly
void process_input() {
    INPUT_RECORD record;
    DWORD count;
    // reset keystate
    for (int i = 0; i < 256; i++) keystate[i] = 0;
    // read and handle all console input events
    while (PeekConsoleInput(hStdin, &record, 1, &count) && count > 0) {
        ReadConsoleInput(hStdin, &record, 1, &count);
        if (record.EventType == KEY_EVENT && record.Event.KeyEvent.bKeyDown) {
            char c = record.Event.KeyEvent.uChar.AsciiChar;
            if (c) keystate[(unsigned char)c] = 1;
        }
    }
}
#else
// POSIX terminal initialization for Unix
void init_terminal() {
    tcgetattr(STDIN_FILENO, &old_termios);
    new_termios = old_termios;
    // disable canonical mode and echo
    new_termios.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);
    // set non-blocking read
    fcntl(STDIN_FILENO, F_SETFL,
          fcntl(STDIN_FILENO, F_GETFL, 0) | O_NONBLOCK);
    fflush(stdout);
}

// Restore original terminal settings
void restore_terminal() {
    tcsetattr(STDIN_FILENO, TCSANOW, &old_termios);
    printf("terminal restored\n");
}

// Read available bytes into keystate
void process_input() {
    char c;
    for (int i = 0; i < 256; i++) keystate[i] = 0;
    while (read(STDIN_FILENO, &c, 1) > 0) {
        keystate[(unsigned char)c] = 1;
    }
}
#endif

// check if a specific key is pressed
int is_key_pressed(char key) {
    return keystate[(unsigned char)key];
}

// allocate the 2D ASCII frame buffer
char** init_picture() {
    char** picture = malloc(sizeof(char*) * Y_PIXELS);
    if (!picture) { perror("alloc pic"); exit(EXIT_FAILURE); }
    for (int i = 0; i < Y_PIXELS; i++) {
        picture[i] = malloc(X_PIXELS);
        if (!picture[i]) { perror("alloc row"); exit(EXIT_FAILURE); }
    }
    return picture;
}

// allocate the 3D block world
char*** init_blocks() {
    char*** blocks = malloc(Z_BLOCKS * sizeof(char**));
    if (!blocks) { perror("alloc blocks"); exit(EXIT_FAILURE); }
    for (int z = 0; z < Z_BLOCKS; z++) {
        blocks[z] = malloc(Y_BLOCKS * sizeof(char*));
        if (!blocks[z]) { perror("alloc layer"); exit(EXIT_FAILURE); }
        for (int y = 0; y < Y_BLOCKS; y++) {
            blocks[z][y] = malloc(X_BLOCKS);
            if (!blocks[z][y]) { perror("alloc row"); exit(EXIT_FAILURE); }
            for (int x = 0; x < X_BLOCKS; x++) blocks[z][y][x] = ' ';
        }
    }
    return blocks;
}

// initial player position and view
player_pos_view init_posview() {
    player_pos_view pv;
    pv.pos.x = 5; pv.pos.y = 5; pv.pos.z = 4 + EYE_HEIGHT;
    pv.view.psi = 0; pv.view.phi = 0;
    return pv;
}

// convert spherical angles to 3D direction vector
vect angles_to_vect(vect2 ang) {
    vect v;
    v.x = cos(ang.psi) * cos(ang.phi);
    v.y = cos(ang.psi) * sin(ang.phi);
    v.z = sin(ang.psi);
    return v;
}

// vector addition
vect vect_add(vect a, vect b) {
    return (vect){a.x + b.x, a.y + b.y, a.z + b.z};
}

// scale a vector by s
vect vect_scale(float s, vect v) {
    return (vect){s * v.x, s * v.y, s * v.z};
}

// subtract b from a
vect vect_sub(vect a, vect b) {
    return vect_add(a, vect_scale(-1, b));
}

// normalize a vector in-place
void vect_normalize(vect* v) {
    float len = sqrt(v->x*v->x + v->y*v->y + v->z*v->z);
    if (len > 0) { v->x/=len; v->y/=len; v->z/=len; }
}

// prepare ray directions for each pixel
vect** init_directions(vect2 view) {
    view.psi -= VIEW_HEIGHT/2;
    vect down = angles_to_vect(view);
    view.psi += VIEW_HEIGHT;
    vect up = angles_to_vect(view);
    view.psi -= VIEW_HEIGHT/2;
    view.phi -= VIEW_WIDTH/2;
    vect left = angles_to_vect(view);
    view.phi += VIEW_WIDTH;
    vect right = angles_to_vect(view);
    view.phi -= VIEW_WIDTH/2;
    vect mid_vert = vect_scale(0.5, vect_add(up, down));
    vect mid_hor = vect_scale(0.5, vect_add(left, right));
    vect to_left = vect_sub(left, mid_hor);
    vect to_up = vect_sub(up, mid_vert);

    vect** dirs = malloc(Y_PIXELS * sizeof(vect*));
    for (int y = 0; y < Y_PIXELS; y++) {
        dirs[y] = malloc(X_PIXELS * sizeof(vect));
        for (int x = 0; x < X_PIXELS; x++) {
            vect ray = vect_sub(
                vect_sub(vect_add(mid_hor, to_left), vect_scale((float)x/(X_PIXELS-1)*2, to_left)),
                vect_scale((float)y/(Y_PIXELS-1)*2, to_up)
            );
            vect_normalize(&ray);
            dirs[y][x] = ray;
        }
    }
    return dirs;
}

// check if a point leaves the world
int ray_outside(vect p) {
    return p.x<0||p.x>=X_BLOCKS||p.y<0||p.y>=Y_BLOCKS||p.z<0||p.z>=Z_BLOCKS;
}

// check if position is on block border
int on_block_border(vect p) {
    int cnt=0;
    if (fabs(p.x - round(p.x)) < BLOCK_BORDER_SIZE) cnt++;
    if (fabs(p.y - round(p.y)) < BLOCK_BORDER_SIZE) cnt++;
    if (fabs(p.z - round(p.z)) < BLOCK_BORDER_SIZE) cnt++;
    return cnt>=2;
}

// simple min
float minf(float a,float b){return a<b?a:b;}

// trace a ray until it hits a block or leaves
char raytrace(vect pos,vect dir,char*** blocks) {
    const float eps = 0.01f;
    while(!ray_outside(pos)){
        int x=(int)pos.x,y=(int)pos.y,z=(int)pos.z;
        if(z>=0&&z<Z_BLOCKS&&y>=0&&y<Y_BLOCKS&&x>=0&&x<X_BLOCKS){
            char c=blocks[z][y][x];
            if(c!=' '){ return on_block_border(pos)?'-':c; }
        }
        float dist=2;
        if(dir.x>eps) dist=minf(dist,((int)(pos.x+1)-pos.x)/dir.x);
        else if(dir.x<-eps) dist=minf(dist,((int)pos.x-pos.x)/dir.x);
        if(dir.y>eps) dist=minf(dist,((int)(pos.y+1)-pos.y)/dir.y);
        else if(dir.y<-eps) dist=minf(dist,((int)pos.y-pos.y)/dir.y);
        if(dir.z>eps) dist=minf(dist,((int)(pos.z+1)-pos.z)/dir.z);
        else if(dir.z<-eps) dist=minf(dist,((int)pos.z-pos.z)/dir.z);
        pos=vect_add(pos,vect_scale(dist+eps,dir));
    }
    return ' ';
}

// fill picture buffer by raytracing every pixel
void get_picture(char** pic, player_pos_view pv, char*** blocks){
    vect** dirs = init_directions(pv.view);
    for(int y=0;y<Y_PIXELS;y++)for(int x=0;x<X_PIXELS;x++)
        pic[y][x] = raytrace(pv.pos, dirs[y][x], blocks);
    for(int y=0;y<Y_PIXELS;y++) free(dirs[y]);
    free(dirs);
}

// render ASCII frame to console
void draw_ascii(char** pic){
    printf("\033[0;0H");
    for(int y=0;y<Y_PIXELS;y++){
        int cur=0;
        for(int x=0;x<X_PIXELS;x++){
            if(pic[y][x]=='o'&&cur!=32){printf("\x1B[32m");cur=32;} 
            else if(pic[y][x]!='o'&&cur!=0){printf("\x1B[0m");cur=0;}
            putchar(pic[y][x]);
        }
        printf("\x1B[0m\n");
    }
}

// update player based on input and collision
void update_pos_view(player_pos_view* pv,char*** blocks){
    float move=0.3f,rot=0.1f;
    int x=(int)pv->pos.x,y=(int)pv->pos.y,z=(int)(pv->pos.z-EYE_HEIGHT+0.01f);
    if(x>=0&&x<X_BLOCKS&&y>=0&&y<Y_BLOCKS&&z>=0&&z<Z_BLOCKS&&blocks[z][y][x]!=' ')pv->pos.z+=1;
    z=(int)(pv->pos.z-EYE_HEIGHT-0.01f);
    if(x>=0&&x<X_BLOCKS&&y>=0&&y<Y_BLOCKS&&z>=0&&z<Z_BLOCKS&&blocks[z][y][x]==' ')pv->pos.z-=1;
    if(is_key_pressed('w')) pv->view.psi+=rot;
    if(is_key_pressed('s')) pv->view.psi-=rot;
    if(is_key_pressed('d')) pv->view.phi+=rot;
    if(is_key_pressed('a')) pv->view.phi-=rot;
    if(pv->view.psi>M_PI/2)pv->view.psi=M_PI/2;
    if(pv->view.psi<-M_PI/2)pv->view.psi=-M_PI/2;
    vect dir=angles_to_vect(pv->view);
    if(is_key_pressed('i')){pv->pos.x+=move*dir.x;pv->pos.y+=move*dir.y;}
    if(is_key_pressed('k')){pv->pos.x-=move*dir.x;pv->pos.y-=move*dir.y;}
    if(is_key_pressed('j')){pv->pos.x+=move*dir.y;pv->pos.y-=move*dir.x;}
    if(is_key_pressed('l')){pv->pos.x-=move*dir.y;pv->pos.y+=move*dir.x;}
    if(pv->pos.x<0)pv->pos.x=0; if(pv->pos.x>=X_BLOCKS)pv->pos.x=X_BLOCKS-0.01f;
    if(pv->pos.y<0)pv->pos.y=0; if(pv->pos.y>=Y_BLOCKS)pv->pos.y=Y_BLOCKS-0.01f;
    if(pv->pos.z<EYE_HEIGHT)pv->pos.z=EYE_HEIGHT; if(pv->pos.z>=Z_BLOCKS)pv->pos.z=Z_BLOCKS-0.01f;
}

// cast a ray until a block is found, return hit position
vect get_current_block(player_pos_view pv,char*** blocks){
    vect pos=pv.pos; vect dir=angles_to_vect(pv.view); float eps=0.01f;
    while(!ray_outside(pos)){
        int x=(int)pos.x,y=(int)pos.y,z=(int)pos.z;
        if(z>=0&&z<Z_BLOCKS&&y>=0&&y<Y_BLOCKS&&x>=0&&x<X_BLOCKS&&blocks[z][y][x]!=' ')return pos;
        float dist=2;
        if(dir.x>eps)dist=minf(dist,((int)(pos.x+1)-pos.x)/dir.x);
        else if(dir.x<-eps)dist=minf(dist,((int)pos.x-pos.x)/dir.x);
        if(dir.y>eps)dist=minf(dist,((int)(pos.y+1)-pos.y)/dir.y);
        else if(dir.y<-eps)dist=minf(dist,((int)pos.y-pos.y)/dir.y);
        if(dir.z>eps)dist=minf(dist,((int)(pos.z+1)-pos.z)/dir.z);
        else if(dir.z<-eps)dist=minf(dist,((int)pos.z-pos.z)/dir.z);
        pos=vect_add(pos,vect_scale(dist+eps,dir));
    }
    return pos;
}

// place a new block adjacent to the looked-at block
void place_block(vect pos,char*** blocks,char block) {
    int x=(int)pos.x,y=(int)pos.y,z=(int)pos.z;
    if(x<0||x>=X_BLOCKS||y<0||y>=Y_BLOCKS||z<0||z>=Z_BLOCKS) return;
    float d[6] = {fabs(x+1-pos.x),fabs(pos.x-x),fabs(y+1-pos.y),fabs(pos.y-y),fabs(z+1-pos.z),fabs(pos.z-z)};
    int mi=0; for(int i=1;i<6;i++) if(d[i]<d[mi])mi=i;
    switch(mi){case 0: if(x+1<X_BLOCKS) blocks[z][y][x+1]=block;break;
                case 1: if(x-1>=0)      blocks[z][y][x-1]=block;break;
                case 2: if(y+1<Y_BLOCKS)blocks[z][y+1][x]=block;break;
                case 3: if(y-1>=0)      blocks[z][y-1][x]=block;break;
                case 4: if(z+1<Z_BLOCKS)blocks[z+1][y][x]=block;break;
                case 5: if(z-1>=0)      blocks[z-1][y][x]=block;break;}
}

int main() {
    init_terminal();
    char** picture = init_picture();
    char*** blocks  = init_blocks();
    for(int z=0;z<4;z++) for(int y=0;y<Y_BLOCKS;y++) for(int x=0;x<X_BLOCKS;x++)
        blocks[z][y][x]='@';
    player_pos_view pv = init_posview();
    while(1) {
        process_input(); if(is_key_pressed('q')) break;
        update_pos_view(&pv,blocks);
        vect cb = get_current_block(pv,blocks);
        int have = !ray_outside(cb);
        char saved=' '; int removed=0;
        if(have) {
            int cx=(int)cb.x,cy=(int)cb.y,cz=(int)cb.z;
            saved=blocks[cz][cy][cx]; blocks[cz][cy][cx]='o';
            if(is_key_pressed('x')) {removed=1; blocks[cz][cy][cx]=' ';}
            if(is_key_pressed(' ')) place_block(cb,blocks,'@');
        }
        get_picture(picture,pv,blocks);
        if(have && !removed) {
            int cx=(int)cb.x,cy=(int)cb.y,cz=(int)cb.z;
            blocks[cz][cy][cx]=saved;
        }
        draw_ascii(picture);
#ifdef _WIN32
        Sleep(20);
#else
        usleep(20000);
#endif
    }
    for(int i=0;i<Y_PIXELS;i++) free(picture[i]); free(picture);
    for(int z=0;z<Z_BLOCKS;z++){ for(int y=0;y<Y_BLOCKS;y++) free(blocks[z][y]); free(blocks[z]); }
    free(blocks);
    restore_terminal();
    return 0;
}

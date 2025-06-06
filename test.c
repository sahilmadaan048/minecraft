#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <termios.h>
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

static struct termios old_termios, new_termios;

// vect represents a 3D vector in cartesian coordinate system
typedef struct Vector {
    float x;
    float y;
    float z;
} vect;

// vect2 represents a pair of angles, likely for describing orientation or spherical coordinates. 
// The names psi and phi are used to describe the angles of a point in 3D space in spherical coordinate system
typedef struct Vector2 {
    float psi;
    float phi;
} vect2;

typedef struct Vector_vector2 {
    vect pos;
    vect2 view;
} player_pos_view;


void init_terminal() {
    //to store the old terminal settings and store them in old_termios
    tcgetattr(STDIN_FILENO, &old_termios);

    //create a copy of the current settings to modify
    new_termios = old_termios;

    //disables the canonical mode and echo
    /*
        diabling canonical mode will ensure that we are able to input character by character without the need to press ENTER each time

        disabling the echo mode means that the character input wont get printed on the terminal each time yoi press a key
    */
    new_termios.c_lflag &= ~(ICANON | ECHO);

    //applies the modified terminal settings immediately
    tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);

    //sets the stdin file descrioptor to non-blocking mode
    fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL, 0) | O_NONBLOCK);

    //rnsures that all output gets flushed ti the terminal screen for consistent behaviour
    fflush(stdout);
}

void restore_terminal() {
    //restores the old terminal settings stored in the old_termios variable
    tcsetattr(STDIN_FILENO, TCSANOW, &old_termios);
    printf("terminal restored");
}

//to cover all possible ASCII values
static char keystate[256] = { 0 };

void process_input() {
    char c;
    for (int i = 0; i < 256; i++) {
        keystate[i] = 0;
    }

    //reading input from the terminal byte by byte and stores it in the variable c
    while (read(STDIN_FILENO, &c, 1) > 0) {
        // printf("\ninput: %c", c);
        //get the ASCII for the input character
        unsigned char uc = (unsigned char)c;

        //kinda making a visited array to register a key with this ASCII 'uc' begin pressed
        keystate[uc] = 1;
    }
}

//check if the key is pressed
int is_key_pressed(char key) {
    return keystate[(unsigned char)key];
}

//initialise a image in the program by creating a image buffer
//make an array of size Y_PIXELS, each storing an array of characters of size X_PIXELS
char** init_picture() {
    char** picture = malloc(sizeof(char*) * Y_PIXELS);
    if (picture == NULL) {
        perror("Failed to allocate picture");
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < Y_PIXELS; i++) {
        picture[i] = malloc(sizeof(char) * X_PIXELS);
        if (picture[i] == NULL) {
            perror("Failed to allocate picture row");
            exit(EXIT_FAILURE);
        }
    }
    return picture;
}

//make/initialise a grid based block for the game
char*** init_blocks() {
    char*** blocks = malloc(sizeof(char**) * Z_BLOCKS);
    if (blocks == NULL) {
        perror("Failed to allocate blocks");
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < Z_BLOCKS; i++) {
        blocks[i] = malloc(sizeof(char*) * Y_BLOCKS);
        if (blocks[i] == NULL) {
            perror("Failed to allocate blocks layer");
            exit(EXIT_FAILURE);
        }
        for (int j = 0; j < Y_BLOCKS; j++) {
            blocks[i][j] = malloc(sizeof(char) * X_BLOCKS);
            if (blocks[i][j] == NULL) {
                perror("Failed to allocate blocks row");
                exit(EXIT_FAILURE);
            }
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
    posview.pos.x = 5;
    posview.pos.y = 5;
    posview.pos.z = 4 + EYE_HEIGHT;
    posview.view.psi = 0;
    posview.view.phi = 0;
    return posview;
}

//convert the posview angles into vect
vect angles_to_vect(vect2 angles) {
    vect res;
    res.x = cos(angles.psi) * cos(angles.phi);
    res.y = cos(angles.psi) * sin(angles.phi);
    res.z = sin(angles.psi);
    return res;
}

//vector addition code
vect vect_add(vect v1, vect v2) {
    vect res;
    res.x = v1.x + v2.x;
    res.y = v1.y + v2.y;
    res.z = v1.z + v2.z;
    return res;
}

//scalar multiplication of a vectior v with constant a
vect vect_scale(float s, vect v) {
    vect res = { s * v.x, s * v.y, s * v.z };
    return res;
}

//vector subtraction code
vect vect_sub(vect v1, vect v2) {
    vect v3 = vect_scale(-1, v2);
    return vect_add(v1, v3);
}

//finding the unit vector for the vector
void vect_normalize(vect* v) {
    float len = sqrt(v->x * v->x + v->y * v->y + v->z * v->z);
    if (len > 0) {
        v->x /= len;
        v->y /= len;
        v->z /= len;
    }
}

//initializes a 2D array of direction vectors for each pixel on a screen, based on a given camera/view orientation (vect2 view)
//chatgpt'ed this logic cus it was taking too long
vect** init_directions(vect2 view) {
    view.psi -= VIEW_HEIGHT / 2.0;
    vect screen_down = angles_to_vect(view);

    view.psi += VIEW_HEIGHT;
    vect screen_up = angles_to_vect(view);

    view.psi -= VIEW_HEIGHT / 2.0;
    view.phi -= VIEW_WIDTH / 2.0;
    vect screen_left = angles_to_vect(view);

    view.phi += VIEW_WIDTH;
    vect screen_right = angles_to_vect(view);

    view.phi -= VIEW_WIDTH / 2.0;

    vect screen_mid_vert = vect_scale(0.5, vect_add(screen_up, screen_down));
    vect screen_mid_hor = vect_scale(0.5, vect_add(screen_left, screen_right));
    vect mid_to_left = vect_sub(screen_left, screen_mid_hor);
    vect mid_to_up = vect_sub(screen_up, screen_mid_vert);

    vect** dir = malloc(sizeof(vect*) * Y_PIXELS);
    if (dir == NULL) {
        perror("Failed to allocate directions");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < Y_PIXELS; i++) {
        dir[i] = malloc(sizeof(vect) * X_PIXELS);
        if (dir[i] == NULL) {
            perror("Failed to allocate directions row");
            exit(EXIT_FAILURE);
        }
    }

    for (int y_pix = 0; y_pix < Y_PIXELS; y_pix++) {
        for (int x_pix = 0; x_pix < X_PIXELS; x_pix++) {
            vect tmp = vect_add(vect_add(screen_mid_hor, mid_to_left), mid_to_up);
            tmp = vect_sub(tmp, vect_scale(((float)x_pix / (X_PIXELS - 1)) * 2, mid_to_left));
            tmp = vect_sub(tmp, vect_scale(((float)y_pix / (Y_PIXELS - 1)) * 2, mid_to_up));
            vect_normalize(&tmp);
            dir[y_pix][x_pix] = tmp;
        }
    }
    return dir;
}

// This function checks if a 3D point pos is outside the allowed block boundaries.
int ray_outside(vect pos) {
    if (pos.x >= X_BLOCKS || pos.y >= Y_BLOCKS || pos.z >= Z_BLOCKS
        || pos.x < 0 || pos.y < 0 || pos.z < 0) {
        return 1;
    }
    return 0;
}

//This function checks if a position lies on the border between two or more blocks.
int on_block_border(vect pos) {
    int cnt = 0;
    if (fabsf(pos.x - roundf(pos.x)) < BLOCK_BORDER_SIZE) {
        cnt++;
    }
    if (fabsf(pos.y - roundf(pos.y)) < BLOCK_BORDER_SIZE) {
        cnt++;
    }
    if (fabsf(pos.z - roundf(pos.z)) < BLOCK_BORDER_SIZE) {
        cnt++;
    }

    // If the point is near the boundary in at least two dimensions, then it's on a block edge or corner (not just a face).
    if (cnt >= 2) {
        return 1;
    }
    return 0;
}

float min(float a, float b) {
    if (a < b)
        return a;
    return b;
}

/*
    it's a classic voxel ray traversal algorithm, used in things like 
    Minecraft-style rendering or ray marching in a voxel grid.
*/
char raytrace(vect pos, vect dir, char*** blocks) {
    float eps = 0.01;
    while (!ray_outside(pos)) {
        // Check bounds before accessing
        int z = (int)pos.z;
        int y = (int)pos.y;
        int x = (int)pos.x;
        
        if (z < 0 || z >= Z_BLOCKS || y < 0 || y >= Y_BLOCKS || x < 0 || x >= X_BLOCKS) {
            break;
        }
        
        char c = blocks[z][y][x];
        if (c != ' ') {
            if (on_block_border(pos)) {
                return '-';
            }
            else {
                return c;
            }
        }
        float dist = 2;
        if (dir.x > eps) {
            dist = min(dist, ((int)(pos.x + 1) - pos.x) / dir.x);
        }
        else if (dir.x < -eps) {
            dist = min(dist, ((int)pos.x - pos.x) / dir.x);
        }
        if (dir.y > eps) {
            dist = min(dist, ((int)(pos.y + 1) - pos.y) / dir.y);
        }
        else if (dir.y < -eps) {
            dist = min(dist, ((int)pos.y - pos.y) / dir.y);
        }
        if (dir.z > eps) {
            dist = min(dist, ((int)(pos.z + 1) - pos.z) / dir.z);
        }
        else if (dir.z < -eps) {
            dist = min(dist, ((int)pos.z - pos.z) / dir.z);
        }
        pos = vect_add(pos, vect_scale(dist + eps, dir));
    }
    return ' ';
}

/*
    part of the ASCII raytracer pipeline that takes the player's position and view, 
    traces rays into the 3D world, and fills in a 2D ASCII picture.
*/
void get_picture(char** picture, player_pos_view posview, char*** blocks) {
    vect** directions = init_directions(posview.view);
    for(int y = 0; y < Y_PIXELS; y++) {
        for(int x = 0; x < X_PIXELS; x++) {
            picture[y][x] = raytrace(posview.pos, directions[y][x], blocks);
        }
    }
    
    // Free directions memory
    for(int i = 0; i < Y_PIXELS; i++) {
        free(directions[i]);
    }
    free(directions);
}

/*
    performs ray tracing in a 3D voxel grid (blocks) to determine
    what the ray hits first when cast from a point pos in direction dir
*/
void draw_ascii(char** picture) {
    fflush(stdout);
    printf("\033[0;0H");
    for (int i = 0; i < Y_PIXELS; i++) {
        int current_color = 0;
        for (int j = 0; j < X_PIXELS; j++) {
            if (picture[i][j] == 'o' && current_color != 32) {
                printf("\x1B[32m");
                current_color = 32;
            }
            else if (picture[i][j] != 'o' && current_color != 0) {
                printf("\x1B[0m");
                current_color = 0;
            }
            printf("%c", picture[i][j]);
        }
        printf("\x1B[0m\n");
    }
}

// Function to update the player's position and viewing direction based on input
void update_pos_view(player_pos_view* posview, char*** blocks) {
    float move_eps = 0.30;  // Movement speed
    float tilt_eps = 0.1;   // Rotation speed
    
    // Check bounds before casting to int
    int x = (int)posview->pos.x;
    int y = (int)posview->pos.y;
    int z = (int)posview->pos.z - EYE_HEIGHT + 0.01;
    
    // Ensure we're within bounds
    if (x >= 0 && x < X_BLOCKS && y >= 0 && y < Y_BLOCKS && z >= 0 && z < Z_BLOCKS) {
        // Push player upward if embedded in a block
        if (blocks[z][y][x] != ' ') {
            posview->pos.z += 1;
        }
    }

    z = (int)posview->pos.z - EYE_HEIGHT - 0.01;
    if (x >= 0 && x < X_BLOCKS && y >= 0 && y < Y_BLOCKS && z >= 0 && z < Z_BLOCKS) {
        // Push player downward if floating above empty space
        if (blocks[z][y][x] == ' ') {
            posview->pos.z -= 1;
        }
    }

    // Adjust the view angles based on rotation key inputs
    if (is_key_pressed('w')) posview->view.psi += tilt_eps;
    if (is_key_pressed('s')) posview->view.psi -= tilt_eps;
    if (is_key_pressed('d')) posview->view.phi += tilt_eps;
    if (is_key_pressed('a')) posview->view.phi -= tilt_eps;

    // Clamp psi to prevent over-rotation
    if (posview->view.psi > M_PI/2) posview->view.psi = M_PI/2;
    if (posview->view.psi < -M_PI/2) posview->view.psi = -M_PI/2;

    // Calculate movement direction from angles
    vect direction = angles_to_vect(posview->view);

    // Move forward/backward/strafe using direction vector
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

    // Clamp position to world bounds
    if (posview->pos.x < 0) posview->pos.x = 0;
    if (posview->pos.x >= X_BLOCKS) posview->pos.x = X_BLOCKS - 0.01;
    if (posview->pos.y < 0) posview->pos.y = 0;
    if (posview->pos.y >= Y_BLOCKS) posview->pos.y = Y_BLOCKS - 0.01;
    if (posview->pos.z < EYE_HEIGHT) posview->pos.z = EYE_HEIGHT;
    if (posview->pos.z >= Z_BLOCKS) posview->pos.z = Z_BLOCKS - 0.01;
}

// Function to trace a ray from the player's view until it hits a non-empty block
vect get_current_block(player_pos_view posview, char*** blocks) {
    vect pos = posview.pos;
    vect dir = angles_to_vect(posview.view); // Get direction vector from view
    float eps = 0.01;

    // Keep moving until the ray exits the world
    while (!ray_outside(pos)) {
        int z = (int)pos.z;
        int y = (int)pos.y;
        int x = (int)pos.x;
        
        // Check bounds before accessing
        if (z < 0 || z >= Z_BLOCKS || y < 0 || y >= Y_BLOCKS || x < 0 || x >= X_BLOCKS) {
            break;
        }
        
        char c = blocks[z][y][x];
        if (c != ' ') {
            return pos;  // Found a block!
        }

        float dist = 2;

        // Calculate smallest distance to the next voxel boundary on each axis
        if (dir.x > eps) dist = min(dist, ((int)(pos.x + 1) - pos.x) / dir.x);
        else if (dir.x < -eps) dist = min(dist, ((int)pos.x - pos.x) / dir.x);
        if (dir.y > eps) dist = min(dist, ((int)(pos.y + 1) - pos.y) / dir.y);
        else if (dir.y < -eps) dist = min(dist, ((int)pos.y - pos.y) / dir.y);
        if (dir.z > eps) dist = min(dist, ((int)(pos.z + 1) - pos.z) / dir.z);
        else if (dir.z < -eps) dist = min(dist, ((int)pos.z - pos.z) / dir.z);

        // Move ray forward by calculated step
        pos = vect_add(pos, vect_scale(dist + eps, dir));
    }

    return pos; // Return last position before exiting bounds
}

// Function to place a new block adjacent to the one being looked at
void place_block(vect pos, char*** blocks, char block) {
    int x = (int)pos.x, y = (int)pos.y, z = (int)pos.z;
    
    // Check if the current position is within bounds
    if (z < 0 || z >= Z_BLOCKS || y < 0 || y >= Y_BLOCKS || x < 0 || x >= X_BLOCKS) {
        return;
    }

    // Compute distances to each face of the block to determine the nearest face
    float dists[6];
    dists[0] = fabsf(x + 1 - pos.x);  // +X face
    dists[1] = fabsf(pos.x - x);      // -X face
    dists[2] = fabsf(y + 1 - pos.y);  // +Y face
    dists[3] = fabsf(pos.y - y);      // -Y face
    dists[4] = fabsf(z + 1 - pos.z);  // +Z face
    dists[5] = fabsf(pos.z - z);      // -Z face

    // Find the index of the minimum distance face
    int min_idx = 0;
    float mindist = dists[0];
    for (int i = 0; i < 6; i++) {
        if (dists[i] < mindist) {
            mindist = dists[i];
            min_idx = i;
        }
    }

    // Place the new block on the nearest adjacent face if within bounds
    switch (min_idx) {
        case 0: 
            if (x + 1 < X_BLOCKS) blocks[z][y][x + 1] = block; 
            break;
        case 1: 
            if (x - 1 >= 0) blocks[z][y][x - 1] = block; 
            break;
        case 2: 
            if (y + 1 < Y_BLOCKS) blocks[z][y + 1][x] = block; 
            break;
        case 3: 
            if (y - 1 >= 0) blocks[z][y - 1][x] = block; 
            break;
        case 4: 
            if (z + 1 < Z_BLOCKS) blocks[z + 1][y][x] = block; 
            break;
        case 5: 
            if (z - 1 >= 0) blocks[z - 1][y][x] = block; 
            break;
    }
}

// Main game loop and setup
int main() {
    init_terminal();                         // Prepare terminal for drawing
    char** picture = init_picture();         // Create 2D array for ASCII rendering
    char*** blocks = init_blocks();          // Initialize 3D block world

    // Create a flat ground of blocks ('@') in the lower levels
    for (int x = 0; x < X_BLOCKS; x++) {
        for (int y = 0; y < Y_BLOCKS; y++) {
            for (int z = 0; z < 4; z++) {
                blocks[z][y][x] = '@';
            }
        }
    }

    player_pos_view posview = init_posview(); // Initialize player position and view

    while (1) {
        process_input();                      // Read user input (key states)

        if (is_key_pressed('q')) break;      // Quit if 'q' is pressed

        update_pos_view(&posview, blocks);    // Move/rotate player based on input

        vect current_block = get_current_block(posview, blocks); // Block being looked at
        int have_current_block = !ray_outside(current_block);
        int current_block_x = (int)current_block.x;
        int current_block_y = (int)current_block.y;
        int current_block_z = (int)current_block.z;
        char current_block_c = ' ';
        int removed = 0;

        if (have_current_block) {
            // Check bounds before accessing
            if (current_block_z >= 0 && current_block_z < Z_BLOCKS &&
                current_block_y >= 0 && current_block_y < Y_BLOCKS &&
                current_block_x >= 0 && current_block_x < X_BLOCKS) {
                
                current_block_c = blocks[current_block_z][current_block_y][current_block_x];

                // Highlight the block being looked at with 'o'
                blocks[current_block_z][current_block_y][current_block_x] = 'o';

                // Remove block if 'x' is pressed
                if (is_key_pressed('x')) {
                    removed = 1;
                    blocks[current_block_z][current_block_y][current_block_x] = ' ';
                }

                // Place a new block if space is pressed
                if (is_key_pressed(' ')) {
                    place_block(current_block, blocks, '@');
                }
            }
        }

        // Perform ray tracing to generate the updated ASCII picture
        get_picture(picture, posview, blocks);

        // Restore the original character of the block after rendering
        if (have_current_block && !removed) {
            if (current_block_z >= 0 && current_block_z < Z_BLOCKS &&
                current_block_y >= 0 && current_block_y < Y_BLOCKS &&
                current_block_x >= 0 && current_block_x < X_BLOCKS) {
                blocks[current_block_z][current_block_y][current_block_x] = current_block_c;
            }
        }

        draw_ascii(picture);       // Render the ASCII screen
        usleep(20000);             // Sleep for 20 ms to limit frame rate (~50 FPS)
    }

    // Free allocated memory
    for (int i = 0; i < Y_PIXELS; i++) {
        free(picture[i]);
    }
    free(picture);
    
    for (int i = 0; i < Z_BLOCKS; i++) {
        for (int j = 0; j < Y_BLOCKS; j++) {
            free(blocks[i][j]);
        }
        free(blocks[i]);
    }
    free(blocks);

    restore_terminal();           // Reset terminal on exit
    return 0;
}
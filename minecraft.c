#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#define X_PIXELS 900
#define Y_PIXELS 180
#define X_BLOCKS 20
#define Y_BLOCKS 20
#define Z_BLOCKS 10
#define EYE_HEIGHT 1.5
#define VIEW_HEIGHT 0.7
#define VIEW_WIDTH 1
#define BLOCK_BORDER_SIZE 0.05

static struct termios old_termios, new_termios;

//vect represents a 3D vector in cartesian coordinate system
typedef struct Vector {
    float x;
    float y;
    float z;
} vect;

//vect2 represents a pair of angles, likely for describing orientation or spherical coordinates. The names psi and phi are used to describe the angles of a point in 3D space in spherical coordinate system
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
    for(int i=0; i<256; i++) {
        keystate[i] = 0;
    }
    
    //reading input from the terminal byte by byte and stores it in the variable c
    while(read(STDIN_FILENO, &c, 1) > 0) {
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
    for(int i=0; i< Y_PIXELS; i++) {
        picture[i] = malloc(sizeof(char) * X_PIXELS);
    }
    return picture;
}

//make/initialise a grid based block for the game
char*** init_blocks() {
    char*** blocks = malloc(sizeof(char**)  * Z_BLOCKS);
    for(int i=0; i< Z_BLOCKS; i++) {
        blocks[i] = malloc(sizeof(char*) * Y_BLOCKS);
        for(int j=0; j< Y_BLOCKS; j++) {
            blocks[i][j] = malloc(sizeof(char) * X_BLOCKS);
            for(int k=0; k<X_BLOCKS; k++) {
                blocks[i][j][k] = ' ';
            }
        }
    }
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

int main() {
    init_terminal();
    char** picture = init_picture();
    char*** blocks = init_blocks(); 
    for(int i=0; i<X_BLOCKS; i++) {
        for(int j=0; j<Y_BLOCKS; j++) {
            for(int k=0; k<Z_BLOCKS; k++) {
                blocks[i][j][k] = '@';
            }
        }
    }
    player_pos_view posview = init_posview();   
    while(1) {
        //ready to process the input
        process_input();

        //exit in case 'q' is pressed by the user playing the game
        if(is_key_pressed('q')) {
            exit(0);
        }

        //on each key pressed update the player position and viewing angles
        update_pos_view(&posview, blocks);
    }
    restore_terminal();
    return 0;
}
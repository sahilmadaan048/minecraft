#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <termios.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>

static struct termios old_termios, new_termios;

typedef struct Vector {
    float x;
    float y;
    float z;
} vect;

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
        printf("\ninput: %c", c);
        //get the ASCII for the input character
        unsigned char uc = (unsigned char)c;
        
        //kinda making a visited array to register a key with this ASCII 'uc' begin pressed
        keystate[uc] = 1;
    }
}   

int main() {
    init_terminal();
    while(1) {
        process_input();
    }
    restore_terminal();
    return 0;
}
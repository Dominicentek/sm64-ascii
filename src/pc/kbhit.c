#include <termios.h>
#include <unistd.h>

static struct termios settings;
static struct termios initial;
static int peek_character = -1;

void kbhit_init() {
    tcgetattr(0, &initial);
    settings = initial;
    settings.c_lflag &= ~(ICANON | ECHO | ISIG);
    settings.c_cc[VMIN] = 1;
    settings.c_cc[VTIME] = 0;
    tcsetattr(0, TCSANOW, &settings);
}

void kbhit_deinit() {
    tcsetattr(0, TCSANOW, &initial);
}

int kbhit() {
    if (peek_character != -1) return 1;
    settings.c_cc[VMIN] = 0;
    tcsetattr(0, TCSANOW, &settings);
    unsigned char ch;
    int nread = read(0, &ch, 1);
    settings.c_cc[VMIN] = 1;
    tcsetattr(0, TCSANOW, &settings);
    if (nread == 1) {
        peek_character = ch;
        return 1;
    }
    return 0;
}

int getch() {
    char ch;
    if (peek_character != -1) {
        ch = peek_character;
        peek_character = -1;
    }
    else read(0, &ch, 1);
    return ch;
}
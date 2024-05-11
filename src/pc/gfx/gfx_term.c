#include <sys/types.h>
#define WAPI_TERM
#ifdef WAPI_TERM

#ifndef __linux__
#error "Not building for Linux"
#endif

#include "include/types.h"
#include "gfx_window_manager_api.h"
#include "gfx_pc.h"
#include "pc/pc_main.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include "pc/kbhit.h"
#include <unistd.h>
#include <time.h>

#include <GLFW/glfw3.h>
#include <GL/gl.h>
#include <GLES2/gl2.h>

static GLuint framebuffer_id = 0;
static GLuint rendertexture_id = 0;
static GLuint depthbuffer_id = 0;

static unsigned char* image = 0;
static unsigned char* flipped = 0;
static          char* ansi = 0;

static kb_callback_t kb_down;
static kb_callback_t kb_up;

static float prev_time = 0;

static int key_timers[256];

static GLFWwindow* window;

void gfx_term_init(const char *window_title) {
    kbhit_init();
    //setvbuf(stdout, NULL, _IONBF, 0);
    printf("\e]0;%s\a", window_title);
    fflush(stdout);
    memset(key_timers, 0, sizeof(key_timers));

    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);
    window = glfwCreateWindow(800, 600, "", NULL, NULL);
    glfwMakeContextCurrent(window);
}

void gfx_term_set_keyboard_callbacks(kb_callback_t on_key_down, kb_callback_t on_key_up, void (*on_all_keys_up)(void)) {
    kb_down = on_key_down;
    kb_up = on_key_up;
}

void gfx_term_main_loop(void (*run_one_game_iter)(void)) {
    run_one_game_iter();
}

void gfx_term_get_dimensions(uint32_t *width, uint32_t *height) {
    struct winsize w;
    ioctl(fileno(stdout), TIOCGWINSZ, &w);
    if (width) *width = w.ws_col;
    if (height) *height = w.ws_row * 2;
}

void gfx_term_handle_events() {
    for (int i = 0; i < 256; i++) {
        if (key_timers[i] > 0) {
            key_timers[i]--;
            if (key_timers[i] == 0 && kb_up) kb_up(i);
        }
    }
    while (kbhit()) {
        char c = getch();
        if (c == 3) game_exit();
        if (key_timers[c] == 0 && kb_down) kb_down(c);
        key_timers[c] = 5;
    }
}

bool gfx_term_start_frame() {
    glGenFramebuffers(1, &framebuffer_id);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_id);
    glGenTextures(1, &rendertexture_id);
    glBindTexture(GL_TEXTURE_2D, rendertexture_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, gfx_current_dimensions.width, gfx_current_dimensions.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rendertexture_id, 0);
    glGenRenderbuffers(1, &depthbuffer_id);
    glBindRenderbuffer(GL_RENDERBUFFER, depthbuffer_id);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, gfx_current_dimensions.width, gfx_current_dimensions.height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthbuffer_id);
    return true;
}

void gfx_term_swap_buffers_begin() {
    if (!image) image = (unsigned char*)malloc(1000 * 500 * 4);
    if (!flipped) flipped = (unsigned char*)malloc(1000 * 500 * 4);
    if (!ansi) ansi = (char*)malloc(1000 * 500 * 20);
    glBindTexture(GL_TEXTURE_2D, framebuffer_id);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
    glBindTexture(GL_TEXTURE_2D, 0);
    int ptr = 0;
    for (int y = gfx_current_dimensions.height - 1; y >= 0; y -= 2) {
        for (int x = 0; x < gfx_current_dimensions.width; x++) {
            int i = ((y - 0) * gfx_current_dimensions.width + x) * 4;
            int j = ((y - 1) * gfx_current_dimensions.width + x) * 4;
            ptr += sprintf(ansi + ptr, "\e[38;2;%d;%d;%dm\e[48;2;%d;%d;%dm▀", image[i + 0], image[i + 1], image[i + 2], image[j + 0], image[j + 1], image[j + 2]);
        }
    }
    fwrite("\e[2J\e[1;1H", 10, 1, stdout);
    fwrite(ansi, strlen(ansi), 1, stdout);
    fflush(stdout);
    glDeleteFramebuffers(1, &framebuffer_id);
    glDeleteRenderbuffers(1, &depthbuffer_id);
    glDeleteTextures(1, &rendertexture_id);
    glfwSwapBuffers(window);
}

void gfx_term_swap_buffers_end() {
    float curr = clock() / (float)CLOCKS_PER_SEC;
    float delta = curr - prev_time;
    if (delta < 1 / 30.f) usleep((1 / 30.f - delta) * 1000000);
    prev_time = curr;
}

double gfx_term_get_time() {
    return 0; // unimplemented
}

void gfx_term_shutdown() {
    kbhit_deinit();
    glfwTerminate();
    printf("\e[0m\n");
    fflush(stdout);
}

struct GfxWindowManagerAPI gfx_term = {
    gfx_term_init,
    gfx_term_set_keyboard_callbacks,
    gfx_term_main_loop,
    gfx_term_get_dimensions,
    gfx_term_handle_events,
    gfx_term_start_frame,
    gfx_term_swap_buffers_begin,
    gfx_term_swap_buffers_end,
    gfx_term_get_time,
    gfx_term_shutdown
};

#endif
#ifndef _ENGINE_H
#define _ENGINE_H

typedef struct engine_init_flags_t {
    unsigned int net_mode;
    char rec_file[255];
} engine_init_flags;

int engine_init(); // Init window, audiodevice, etc.
void engine_run(engine_init_flags *init_flags); // Run game
void engine_close(); // Kill window, audiodev

#endif // _ENGINE_H

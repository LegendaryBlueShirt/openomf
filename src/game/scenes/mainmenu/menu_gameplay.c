#include "game/scenes/mainmenu/menu_gameplay.h"

#include "game/common_defines.h"
#include "game/gui/gui.h"
#include "game/utils/settings.h"

void menu_gameplay_done(component *c, void *u) {
    menu *m = sizer_get_obj(c->parent);
    m->finished = 1;
}

void menu_gameplay_speed_slide(component *c, void *userdata, int pos) {
    scene *sc = userdata;
    game_state_set_speed(sc->gs, pos + 5);
}

component *menu_gameplay_create(scene *s) {
    const char *fightmode_opts[] = {"NORMAL", "HYPER"};
    const char *hazard_opts[] = {"OFF", "ON"};

    // Text config
    text_settings tconf;
    text_defaults(&tconf);
    tconf.font = FONT_BIG;
    tconf.halign = TEXT_CENTER;
    tconf.cforeground = TEXT_BRIGHT_GREEN;

    component *menu = menu_create(11);
    menu_attach(menu, label_create(&tconf, "GAMEPLAY"));
    menu_attach(menu, filler_create());
    menu_attach(menu, textslider_create_bind(&tconf, "SPEED",
                                             "Change the overall speed of the game. Press left and right to change.",
                                             10, 1, menu_gameplay_speed_slide, s, &settings_get()->gameplay.speed));
    menu_attach(menu, textselector_create_bind_opts(
                          &tconf, "FIGHT MODE",
                          "Fight mode can be either normal or hyper. Hyper mode will enhance your special moves. Check "
                          "the robot description section of help for more information.",
                          NULL, NULL, &settings_get()->gameplay.fight_mode, fightmode_opts, 2));
    menu_attach(menu, textslider_create_bind(&tconf, "POWER 1",
                                             "Change the power of player 1's hits and throws. This setting will take "
                                             "effect only in two player games. Press left and right to change.",
                                             7, 0, NULL, NULL, &settings_get()->gameplay.power1));
    menu_attach(menu, textslider_create_bind(&tconf, "POWER 2",
                                             "Change the power of player 1's hits and throws. This setting will take "
                                             "effect only in two player games. Press left and right to change.",
                                             7, 0, NULL, NULL, &settings_get()->gameplay.power2));
    menu_attach(menu, textselector_create_bind_opts(&tconf, "HAZARDS",
                                                    "Some arenas have dangerous environments: spikes, electrivity, "
                                                    "fighter planes, and the like. This option turns them on and off.",
                                                    NULL, NULL, &settings_get()->gameplay.hazards_on, hazard_opts, 2));
    menu_attach(menu,
                textselector_create_bind_opts(&tconf, "CPU:",
                                              "This determines how well the computer fights in a one player game. This "
                                              "has no effect on two player games. Press left and right to change.",
                                              NULL, NULL, &settings_get()->gameplay.difficulty, ai_difficulty_names,
                                              NUMBER_OF_AI_DIFFICULTY_TYPES));
    menu_attach(menu, textselector_create_bind_opts(&tconf, "",
                                                    "This will set matches so they are one round, best two out of "
                                                    "three rounds, or best three out of five rounds.",
                                                    NULL, NULL, &settings_get()->gameplay.rounds, round_type_names,
                                                    NUMBER_OF_ROUND_TYPES));
    menu_attach(menu,
                textbutton_create(&tconf, "DONE", "Go back to the main menu.", COM_ENABLED, menu_gameplay_done, NULL));
    return menu;
}

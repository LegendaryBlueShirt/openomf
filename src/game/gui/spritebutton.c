#include <stdlib.h>
#include <string.h>

#include "game/gui/sizer.h"
#include "game/gui/spritebutton.h"
#include "game/gui/widget.h"
#include "utils/allocator.h"
#include "utils/c_string_util.h"
#include "utils/log.h"
#include "video/video.h"

typedef struct spritebutton {
    char *text;
    text_settings tconf;
    surface *img;
    int active;

    spritebutton_click_cb click_cb;
    spritebutton_tick_cb tick_cb;
    spritebutton_focus_cb focus_cb;
    bool free_userdata;
    void *userdata;
} spritebutton;

static void spritebutton_render(component *c) {
    spritebutton *sb = widget_get_obj(c);
    text_mode mode = TEXT_UNSELECTED;
    if(c->is_disabled) {
        video_draw_offset(sb->img, c->x, c->y, 5, 0x5F);
        mode = TEXT_DISABLED;
    } else if(sb->active) {
        video_draw(sb->img, c->x, c->y);
        mode = TEXT_SELECTED;
    }
    if(sb->text) {
        text_render(&sb->tconf, mode, c->x, c->y, c->w, c->h, sb->text);
    }
}

static void spritebutton_free(component *c) {
    spritebutton *sb = widget_get_obj(c);
    if(sb->free_userdata) {
        omf_free(sb->userdata);
    }
    omf_free(sb->text);
    omf_free(sb);
}

static void spritebutton_tick(component *c) {
    spritebutton *sb = widget_get_obj(c);
    if(sb->active > 0) {
        sb->active--;
    }

    if(sb->tick_cb) {
        sb->tick_cb(c, sb->userdata);
    }
}

static void spritebutton_focus(component *c, bool focused) {
    spritebutton *sb = widget_get_obj(c);
    if(sb->focus_cb) {
        sb->focus_cb(c, focused, sb->userdata);
    }
}

static int spritebutton_action(component *c, int action) {
    spritebutton *sb = widget_get_obj(c);
    if(c->is_disabled) {
        return 1;
    }

    // Handle selection
    if(action == ACT_KICK || action == ACT_PUNCH) {
        if(sb->active >= 0) {
            sb->active = 10;
        }
        if(sb->click_cb) {
            sb->click_cb(c, sb->userdata);
        }
        return 0;
    }
    return 1;
}

component *spritebutton_create(const text_settings *tconf, const char *text, surface *img, int disabled,
                               spritebutton_click_cb cb, void *userdata) {
    component *c = widget_create();
    component_disable(c, disabled);

    c->supports_focus = 1;

    spritebutton *sb = omf_calloc(1, sizeof(spritebutton));
    if(text != NULL)
        sb->text = omf_strdup(text);
    memcpy(&sb->tconf, tconf, sizeof(text_settings));
    sb->click_cb = cb;
    sb->img = img;
    sb->userdata = userdata;
    widget_set_obj(c, sb);
    sb->tick_cb = NULL;
    sb->focus_cb = NULL;

    widget_set_render_cb(c, spritebutton_render);
    widget_set_action_cb(c, spritebutton_action);
    widget_set_focus_cb(c, spritebutton_focus);
    widget_set_tick_cb(c, spritebutton_tick);
    widget_set_free_cb(c, spritebutton_free);

    return c;
}

void spritebutton_set_tick_cb(component *c, spritebutton_tick_cb cb) {
    spritebutton *sb = widget_get_obj(c);
    sb->tick_cb = cb;
}

void spritebutton_set_focus_cb(component *c, spritebutton_focus_cb cb) {
    spritebutton *sb = widget_get_obj(c);
    sb->focus_cb = cb;
}

void spritebutton_set_always_display(component *c) {
    spritebutton *sb = widget_get_obj(c);
    sb->active = -1;
}

void spritebutton_set_free_userdata(component *c, bool free_userdata) {
    spritebutton *sb = widget_get_obj(c);
    sb->free_userdata = free_userdata;
}

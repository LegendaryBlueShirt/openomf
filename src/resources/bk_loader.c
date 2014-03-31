#include "resources/bk_loader.h"
#include <shadowdive/shadowdive.h>

int load_bk_file(bk *b, int id) {
    // Get directory + filename
    char *filename = get_path_by_id(id);

    // Load up BK file from libSD
    sd_bk_file *tmp = sd_bk_create();
    if(sd_bk_load(tmp, filename) != SD_SUCCESS) {
        sd_bk_delete(tmp);
        free(filename);
        return 1;
    }
    free(filename);

    // Convert
    bk_create(b, tmp);
    sd_bk_delete(tmp);
    return 0;
}

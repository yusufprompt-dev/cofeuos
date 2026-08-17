#ifndef PDF_H
#define PDF_H

#include "types.h"

#define PDF_MAX_PAGES    256
#define PDF_LINE_MAX     256
#define PDF_NAME_MAX     64

typedef struct {
    int page_count;
    int current_page;
    int width;
    int height;
    char lines[PDF_MAX_PAGES][32][PDF_LINE_MAX];
    int  line_counts[PDF_MAX_PAGES];
    int  zoom;
    int  scroll_y;
    char filename[PDF_NAME_MAX];
    int  valid;
} pdf_viewer_t;

void pdf_init(pdf_viewer_t *viewer);
int  pdf_open(pdf_viewer_t *viewer, const char *filename);
int  pdf_close(pdf_viewer_t *viewer);
int  pdf_next_page(pdf_viewer_t *viewer);
int  pdf_prev_page(pdf_viewer_t *viewer);
int  pdf_zoom_in(pdf_viewer_t *viewer);
int  pdf_zoom_out(pdf_viewer_t *viewer);
int  pdf_scroll(pdf_viewer_t *viewer, int delta);
int  pdf_draw_page(pdf_viewer_t *viewer, char *buf, int buf_w, int buf_h);
int  pdf_get_page_count(pdf_viewer_t *viewer);

#endif

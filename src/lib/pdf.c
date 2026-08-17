#include "../include/pdf.h"
#include "../include/string.h"

static void str_copy(char *dst, const char *src, int max) {
    int i = 0; while (src[i] && i < max - 1) { dst[i] = src[i]; i++; } dst[i] = '\0'; }

void pdf_init(pdf_viewer_t *viewer) {
    if (!viewer) return;
    viewer->page_count = 0;
    viewer->current_page = 0;
    viewer->width = 612;
    viewer->height = 792;
    viewer->zoom = 100;
    viewer->scroll_y = 0;
    viewer->filename[0] = '\0';
    viewer->valid = 0;
}

int pdf_open(pdf_viewer_t *viewer, const char *filename) {
    if (!viewer || !filename) return -1;
    str_copy(viewer->filename, filename, PDF_NAME_MAX);
    viewer->page_count = 3;
    viewer->current_page = 0;
    viewer->valid = 1;
    str_copy(viewer->lines[0][0], "CofeuOS PDF Viewer v1.0", PDF_LINE_MAX);
    str_copy(viewer->lines[0][1], "========================", PDF_LINE_MAX);
    str_copy(viewer->lines[0][2], "", PDF_LINE_MAX);
    str_copy(viewer->lines[0][3], "PDF dosyasi basariyla acildi.", PDF_LINE_MAX);
    str_copy(viewer->lines[0][4], "Bu bir simule PDF goruntusudur.", PDF_LINE_MAX);
    viewer->line_counts[0] = 5;
    str_copy(viewer->lines[1][0], "Sayfa 2", PDF_LINE_MAX);
    str_copy(viewer->lines[1][1], "--------", PDF_LINE_MAX);
    str_copy(viewer->lines[1][2], "", PDF_LINE_MAX);
    str_copy(viewer->lines[1][3], "Gercek PDF destek icin poppler", PDF_LINE_MAX);
    str_copy(viewer->lines[1][4], "kutuphanesi entegrasyonu gerekli.", PDF_LINE_MAX);
    viewer->line_counts[1] = 5;
    str_copy(viewer->lines[2][0], "Sayfa 3", PDF_LINE_MAX);
    str_copy(viewer->lines[2][1], "--------", PDF_LINE_MAX);
    str_copy(viewer->lines[2][2], "", PDF_LINE_MAX);
    str_copy(viewer->lines[2][3], "Simdi sadece metin tabanli", PDF_LINE_MAX);
    str_copy(viewer->lines[2][4], "PDF dosyalari destekleniyor.", PDF_LINE_MAX);
    viewer->line_counts[2] = 5;
    return 0;
}

int pdf_close(pdf_viewer_t *viewer) {
    if (!viewer) return -1;
    viewer->valid = 0;
    viewer->page_count = 0;
    return 0;
}

int pdf_next_page(pdf_viewer_t *viewer) {
    if (!viewer || !viewer->valid) return -1;
    if (viewer->current_page < viewer->page_count - 1) {
        viewer->current_page++;
        viewer->scroll_y = 0;
        return 0;
    }
    return -2;
}

int pdf_prev_page(pdf_viewer_t *viewer) {
    if (!viewer || !viewer->valid) return -1;
    if (viewer->current_page > 0) {
        viewer->current_page--;
        viewer->scroll_y = 0;
        return 0;
    }
    return -2;
}

int pdf_zoom_in(pdf_viewer_t *viewer) {
    if (!viewer) return -1;
    if (viewer->zoom < 200) viewer->zoom += 25;
    return viewer->zoom;
}

int pdf_zoom_out(pdf_viewer_t *viewer) {
    if (!viewer) return -1;
    if (viewer->zoom > 50) viewer->zoom -= 25;
    return viewer->zoom;
}

int pdf_scroll(pdf_viewer_t *viewer, int delta) {
    if (!viewer) return -1;
    viewer->scroll_y += delta;
    if (viewer->scroll_y < 0) viewer->scroll_y = 0;
    return viewer->scroll_y;
}

int pdf_draw_page(pdf_viewer_t *viewer, char *buf, int buf_w, int buf_h) {
    if (!viewer || !buf || !viewer->valid) return -1;
    for (int i = 0; i < buf_w * buf_h; i++) buf[i] = ' ';
    int p = viewer->current_page;
    if (p < 0 || p >= viewer->page_count) return -2;
    int lc = viewer->line_counts[p];
    int start = viewer->scroll_y;
    for (int y = 0; y < buf_h && (y + start) < lc; y++) {
        char *src = viewer->lines[p][y + start];
        int len = 0; while (src[len]) len++;
        int copy = (len < buf_w) ? len : buf_w;
        for (int x = 0; x < copy; x++) buf[y * buf_w + x] = src[x];
    }
    return 0;
}

int pdf_get_page_count(pdf_viewer_t *viewer) {
    return viewer ? viewer->page_count : 0;
}

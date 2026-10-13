/* See LICENSE file for copyright and license details. */
#ifndef SCROLL_AMOUNT
# define SCROLL_AMOUNT 30
#endif
#ifndef DRAG_ICON_SIZE
# define DRAG_ICON_SIZE 128
#endif
#ifndef PREVIEW_WIDTH
# define PREVIEW_WIDTH 192
#endif
#ifndef PREVIEW_HEIGHT
# define PREVIEW_HEIGHT 256
#endif
#ifndef INITIAL_COLLECTION_PANE_HEIGHT
# define INITIAL_COLLECTION_PANE_HEIGHT 160
#endif
#ifndef INITIAL_PROPERTY_PANE_HEIGHT
# define INITIAL_PROPERTY_PANE_HEIGHT 300
#endif

#if defined(__GNUC__)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wdeprecated-declarations"
# pragma GCC diagnostic ignored "-Wstrict-prototypes"
#endif
#include <gdk/gdkkeysyms.h>
#include <gexiv2/gexiv2.h>
#include <gtk/gtk.h>
#include <librsvg/rsvg.h>
#include <libsimple.h>
#include <libsimple-arg.h>
#include <math.h>
#if defined(__GNUC__)
# pragma GCC diagnostic pop
#endif

#if defined(__GNUC__)
# pragma GCC diagnostic ignored "-Wunsuffixed-float-constants"
# pragma GCC diagnostic ignored "-Wsuggest-attribute=const"
# pragma GCC diagnostic ignored "-Wsuggest-attribute=pure"
#endif

#if defined(__GNUC__)
# define PURE __attribute__((__pure__))
#else
# define PURE
#endif

#include "logical-scroll-pane.h"


#define FLOAT_EQ(A, B) ((A) >= (B) && (A) <= (B))

#define COMMA ,


enum toggle_group {
	GROUP_NONE,
	GROUP_ZOOM
};

enum zoom_mode {
	ZOOM_CUSTOM,
	ZOOM_NORMAL,
	ZOOM_FIT_WIDTH,
	ZOOM_FIT_HEIGHT,
	ZOOM_FIT_WINDOW,
	ZOOM_BEST_FIT
};

enum transformation {
	TRANS_NONE,          /* 1   */
	TRANS_CLOCKWISE,     /* r   */
	TRANS_180_DEGREES,   /* r²  */
	TRANS_ANTICLOCKWISE, /* r³  */
	TRANS_TRANSPOSE,     /* t   */
	TRANS_FLOP,          /* rt  */
	TRANS_ANTITRANSPOSE, /* r²t */
	TRANS_FLIP,          /* r³t */
	/*
	 * t² = r⁴ = 1
	 * trⁿ = r⁻ⁿt
	 */
	TRANS_NEGATIVE = 0x08
};

struct image {
	GdkPixbuf *raster;
	RsvgHandle *vector;
	int width;
	int height;
	int has_x_dpi;
	int has_y_dpi;
	int has_transformer;
	cairo_matrix_t transformer;
	double x_dpi;
	double y_dpi;
};

struct file {
	char *path;
	off_t size;
	struct image image;
	char **exif_tags;
	GExiv2Metadata *exif;
};

typedef double Zoom; /* TODO */


/* collection.c */
extern GtkWidget *collection_view;
GtkWidget *create_collection_pane(void);
void set_collection_pane_visible(int visible);
void navigate_collection_drag(void);
void collection_files_changed(void);
void update_collection(void);

/* dragging.c */
void set_up_dragging(void);

/* dropping.c */
size_t get_dropped_files(struct file **files_out, char **uris);
void set_up_dropping(void);

/* events.c */
void open_action(GtkAction *action, void *user);
void reload_action(GtkAction *action, void *user);
void print_action(GtkAction *action, void *user);
void properties_action(GtkAction *action, void *user);
void property_pane_action(GtkToggleAction *action, void *user);
void collection_pane_action(GtkToggleAction *action, void *user);
void preferences_action(GtkAction *action, void *user);
void close_action(GtkAction *action, void *user);
void fullscreen_action(GtkToggleAction *action, void *user);
void reset_transformations_action(GtkAction *action, void *user);
void rotate_clockwise_action(GtkAction *action, void *user);
void rotate_180_degrees_action(GtkAction *action, void *user);
void rotate_anticlockwise_action(GtkAction *action, void *user);
void flop_action(GtkAction *action, void *user);
void flip_action(GtkAction *action, void *user);
void transpose_action(GtkAction *action, void *user);
void antitranspose_action(GtkAction *action, void *user);
void invert_action(GtkToggleAction *action, void *user);
void zoom_in_action(GtkAction *action, void *user);
void zoom_out_action(GtkAction *action, void *user);
void normal_size_action(GtkToggleAction *action, void *user);
void width_fit_action(GtkToggleAction *action, void *user);
void height_fit_action(GtkToggleAction *action, void *user);
void window_fit_action(GtkToggleAction *action, void *user);
void best_fit_action(GtkToggleAction *action, void *user);
void next_action(GtkAction *action, void *user);
void previous_action(GtkAction *action, void *user);
void first_action(GtkAction *action, void *user);
void last_action(GtkAction *action, void *user);
int key_pressed(GtkWidget *widget, GdkEventKey *event, void *user);
int scroll_event(GtkWidget *widget, GdkEventScroll *event, void *user);

/* files.c */
extern struct file *files;
extern size_t nfiles;
extern size_t current_file;
void destroy_file(struct file *file);
void free_files(void);
size_t files_from_array(struct file *selected_files, size_t nselected_files);
size_t files_from_list(struct file **files_out, GSList *selected_files);
int set_files_from_argv(int argc, char **argv);
struct file *get_file(void);
int navigate_without_redraw(size_t jump, size_t move);

/* filters.c */
void add_image_filters(GtkFileChooser *chooser);
void free_image_filters(void);

/* image.c */
extern GdkPixbuf *transformed_raster;
void forget_transformed_raster(void);
int get_image(struct image *image);
int get_original_image(struct image *image);

/* opening.c */
int load_image(struct file *file, int errors_to_stderr);
GtkFileChooser *construct_chooser(void);

/* printing.c */
void open_print(struct image *image);

/* render.c */
GdkPixbuf *rasterise(RsvgHandle *vector, int width, int height);
void paint_image(cairo_t *cr, struct image *image);

/* preferences.c */
extern cairo_filter_t scaling_filter;
extern GdkColor chequer_colours[2];
extern uint16_t chequer_alphas[2];
extern int chequered_background;
extern int chequer_size;
void open_preferences(void);

/* properties.c */
GtkWidget *create_property_table(struct file *file);
void open_properties(struct file *file);

/* transform.c */
extern enum transformation trans;
int apply_transformations(struct image *image, struct image *image_out);
enum transformation reset_transformations_without_redraw(void);
int add_transformation_without_redraw(enum transformation augment);
int swaps_dimensions(enum transformation t);
void transform_point(enum transformation t, double *x, double *y, double *w, double *h);
int get_affine_matrix(cairo_matrix_t *matrix_out, enum transformation t, int width, int height);

/* ui.c */
extern GtkWidget *window;
extern GtkWidget *display_area;
extern GtkWidget *scroll;
extern GtkToggleAction *fullscreen;
void set_up_ui(void);
void set_actions_sensitive(void);
void clear_toggles(enum toggle_group group, GtkToggleAction *except);
void set_fullscreen_mode(int active);
void set_property_pane_visible(int visible);
void update_property_pane(void);
int activate_fullscreen(void *user);
int deactivate_fullscreen(void *user);
void set_statusbar_message(const char *message);
void free_ui(void);

/* zoom.c */
extern Zoom zoom;
extern enum zoom_mode zoom_mode;
int zoom_eq(Zoom a, Zoom b);
double get_zoom(Zoom z);
Zoom make_zoom(double s);
Zoom zoom_in(Zoom z);
Zoom zoom_out(Zoom z);
Zoom calculate_fit(struct image *image, int xfit, int yfit, double cap);
Zoom calculate_zoom_mode(struct image *image, enum zoom_mode mode);

/* gbild.c */
int expose_image(GtkWidget *widget, GdkEventExpose *event, void *user);
void add_images(struct file *new_files, size_t nnew_files);
void set_images_from_array(struct file *selected_files, size_t nselected_files);
void set_images_from_list(GSList *selected_files);
void scroll_by(double horizontally, double vertically);
void set_zoom_around_point(Zoom new_zoom, double x, double y);
void set_zoom_around_centre(Zoom new_zoom);
void set_zoom_and_recentre(Zoom new_zoom);
void reload_redraw(void);
void transform_redraw(enum transformation augment);
void navigate_redraw(void);
int recalculate_zoom_mode(void *user);

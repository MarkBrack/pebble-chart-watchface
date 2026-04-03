#include <pebble.h>

#define HOUR_SEGMENTS 12
#define MINUTE_BARS 6
#define BAR_HEIGHT 46
#define BAR_GAP 7
#define BAR_CORNER_RADIUS 4

static Window *s_main_window;
static Layer *s_canvas_layer;

static GColor s_bg_color(void) {
#ifdef PBL_COLOR
  return GColorWhite;
#else
  return GColorWhite;
#endif
}

static GColor s_axis_color(void) {
#ifdef PBL_COLOR
  return GColorLightGray;
#else
  return GColorDarkGray;
#endif
}

static GColor s_grid_color(void) {
#ifdef PBL_COLOR
  return GColorLightGray;
#else
  return GColorLightGray;
#endif
}

static GColor s_future_color(void) {
#ifdef PBL_COLOR
  return GColorChromeYellow;
#else
  return GColorLightGray;
#endif
}

static GColor s_past_color(void) {
#ifdef PBL_COLOR
  return GColorPictonBlue;
#else
  return GColorBlack;
#endif
}

static GColor s_bar_color(int index) {
#ifdef PBL_COLOR
  static const GColor colors[MINUTE_BARS] = {
    GColorVividCerulean,
    GColorCyan,
    GColorMediumSpringGreen,
    GColorChromeYellow,
    GColorRajah,
    GColorMelon
  };
  return colors[index % MINUTE_BARS];
#else
  (void)index;
  return GColorBlack;
#endif
}

static GColor s_text_color(void) {
  return GColorBlack;
}

static void draw_centered_text(GContext *ctx, const char *text, GFont font, GRect rect) {
  graphics_draw_text(ctx, text, font, rect, GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter, NULL);
}

static void format_time_label(char *buffer, size_t size, const struct tm *tick_time) {
  if (clock_is_24h_style()) {
    strftime(buffer, size, "%H:%M", tick_time);
  } else {
    strftime(buffer, size, "%I:%M", tick_time);
    if (buffer[0] == '0') {
      memmove(buffer, buffer + 1, strlen(buffer));
    }
  }
}

static void draw_time(GContext *ctx, GRect bounds, const struct tm *tick_time) {
  (void)bounds;
  graphics_context_set_text_color(ctx, s_text_color());

  char time_text[8];
  format_time_label(time_text, sizeof(time_text), tick_time);

  graphics_draw_text(ctx, time_text, fonts_get_system_font(FONT_KEY_BITHAM_42_LIGHT),
                     GRect(0, 8, bounds.size.w, 52),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

static void draw_pie_labels(GContext *ctx, GRect chart_rect, const struct tm *tick_time) {
  GPoint center = grect_center_point(&chart_rect);
  const int label_radius = (chart_rect.size.w / 2) + 20;
  int hour = tick_time->tm_hour % 12;
  int minute = tick_time->tm_min;
  int past_units = hour * 60 + minute;
  int32_t split_angle = (TRIG_MAX_ANGLE * past_units) / (12 * 60);

  graphics_context_set_text_color(ctx, GColorDarkGray);
  int32_t past_label_angle = split_angle / 2;
  int past_x = center.x + (sin_lookup(past_label_angle) * label_radius) / TRIG_MAX_RATIO;
  int past_y = center.y - (cos_lookup(past_label_angle) * label_radius) / TRIG_MAX_RATIO;
  draw_centered_text(ctx, "Past", fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                     GRect(past_x - 16, past_y - 8, 32, 16));

  int32_t future_label_angle = split_angle + ((TRIG_MAX_ANGLE - split_angle) / 2);
  int future_x = center.x + (sin_lookup(future_label_angle) * label_radius) / TRIG_MAX_RATIO;
  int future_y = center.y - (cos_lookup(future_label_angle) * label_radius) / TRIG_MAX_RATIO;
  future_x -= 8;
  draw_centered_text(ctx, "Future", fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                     GRect(future_x - 20, future_y - 8, 40, 16));
}

static void draw_hour_pie(GContext *ctx, GRect bounds, const struct tm *tick_time) {
  const int chart_size = 88;
  GRect chart_rect = GRect((bounds.size.w - chart_size) / 2, 62, chart_size, chart_size);
  const int hour = tick_time->tm_hour % 12;
  const int minute = tick_time->tm_min;
  const int thickness = chart_size / 2;
  int32_t split_angle = (TRIG_MAX_ANGLE * ((hour * 60) + minute)) / (12 * 60);

  graphics_context_set_fill_color(ctx, s_future_color());
  graphics_fill_radial(ctx, chart_rect, GOvalScaleModeFitCircle, thickness, 0, TRIG_MAX_ANGLE);

  if (split_angle > 0) {
    graphics_context_set_fill_color(ctx, s_past_color());
    graphics_fill_radial(ctx, chart_rect, GOvalScaleModeFitCircle, thickness, 0, split_angle);
  }

  graphics_context_set_stroke_color(ctx, s_axis_color());
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_circle(ctx, grect_center_point(&chart_rect), chart_size / 2);
  GPoint center = grect_center_point(&chart_rect);
  int radius = chart_size / 2;
  int split_x = center.x + (sin_lookup(split_angle) * radius) / TRIG_MAX_RATIO;
  int split_y = center.y - (cos_lookup(split_angle) * radius) / TRIG_MAX_RATIO;
  graphics_draw_line(ctx, center, GPoint(center.x, center.y - radius));
  graphics_draw_line(ctx, center, GPoint(split_x, split_y));
  draw_pie_labels(ctx, chart_rect, tick_time);
}

static void draw_minute_bars(GContext *ctx, GRect bounds, const struct tm *tick_time) {
  const int chart_top = 162;
  const int chart_bottom = 204;
  const int chart_left = 28;
  const int chart_right = bounds.size.w - 14;
  const int total_gap = BAR_GAP * (MINUTE_BARS - 1);
  const int bar_width = (chart_right - chart_left - total_gap) / MINUTE_BARS;
  const int current_bucket = tick_time->tm_min / 10;
  const int bucket_progress = tick_time->tm_min % 10;
  const int chart_height = chart_bottom - chart_top;

  graphics_context_set_stroke_color(ctx, s_grid_color());
  graphics_context_set_stroke_width(ctx, 1);
  for (int i = 0; i <= 2; i++) {
    int y = chart_top + (chart_height * i) / 2;
    graphics_draw_line(ctx, GPoint(chart_left, y), GPoint(chart_right, y));
  }

  graphics_context_set_stroke_color(ctx, s_axis_color());
  graphics_draw_line(ctx, GPoint(chart_left, chart_top), GPoint(chart_left, chart_bottom));
  graphics_draw_line(ctx, GPoint(chart_left, chart_bottom), GPoint(chart_right, chart_bottom));

  graphics_context_set_text_color(ctx, GColorDarkGray);
  draw_centered_text(ctx, "10", fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(0, chart_top - 7, chart_left - 4, 14));
  draw_centered_text(ctx, "5", fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(0, chart_top + (chart_height / 2) - 7, chart_left - 4, 14));
  draw_centered_text(ctx, "1", fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(0, chart_bottom - 11, chart_left - 4, 14));

  for (int i = 0; i < MINUTE_BARS; i++) {
    int x = chart_left + i * (bar_width + BAR_GAP);
    int filled_units = 0;
    if (i < current_bucket) {
      filled_units = 10;
    } else if (i == current_bucket) {
      filled_units = bucket_progress;
    }
    int fill_height = (chart_height * filled_units) / 10;
    GRect bar_rect = GRect(x, chart_bottom - fill_height, bar_width, fill_height);

    if (i < current_bucket) {
      graphics_context_set_fill_color(ctx, s_bar_color(i));
      graphics_fill_rect(ctx, bar_rect, BAR_CORNER_RADIUS, GCornersTop);
    } else if (i == current_bucket) {
      graphics_context_set_fill_color(ctx, s_bar_color(i));
      if (fill_height > 0) {
        graphics_fill_rect(ctx, bar_rect, BAR_CORNER_RADIUS, GCornersTop);
      }
    } else {
      graphics_context_set_fill_color(ctx, s_future_color());
      graphics_fill_rect(ctx, GRect(x, chart_bottom - 2, bar_width, 2), 0, GCornerNone);
    }

    graphics_context_set_stroke_color(ctx, s_axis_color());
    graphics_draw_rect(ctx, GRect(x, chart_top, bar_width, chart_height));

    char label[4];
    snprintf(label, sizeof(label), "%d", (i + 1) * 10);
    graphics_context_set_text_color(ctx, GColorDarkGray);
    graphics_draw_text(ctx, label, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                       GRect(x, chart_bottom + 2, bar_width, 12),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }
}

static void canvas_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  time_t now = time(NULL);
  struct tm *tick_time = localtime(&now);
  if (!tick_time) {
    return;
  }

  graphics_context_set_fill_color(ctx, s_bg_color());
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  draw_time(ctx, bounds, tick_time);
  draw_hour_pie(ctx, bounds, tick_time);
  draw_minute_bars(ctx, bounds, tick_time);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  (void)units_changed;
  (void)tick_time;
  layer_mark_dirty(s_canvas_layer);
}

static void main_window_load(Window *window) {
  GRect bounds = layer_get_bounds(window_get_root_layer(window));

  s_canvas_layer = layer_create(bounds);
  layer_set_update_proc(s_canvas_layer, canvas_update_proc);
  layer_add_child(window_get_root_layer(window), s_canvas_layer);
}

static void main_window_unload(Window *window) {
  (void)window;
  layer_destroy(s_canvas_layer);
}

static void init(void) {
  s_main_window = window_create();
  window_set_background_color(s_main_window, s_bg_color());
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload,
  });
  window_stack_push(s_main_window, true);

  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
}

static void deinit(void) {
  tick_timer_service_unsubscribe();
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}

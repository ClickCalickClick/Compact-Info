#include <pebble.h>
#include <string.h>
#include "config.h"

// UI Elements
static Window *s_main_window;
static Layer *s_canvas_layer;
static TextLayer *s_time_hour_layer;
static TextLayer *s_time_minute_layer;
static TextLayer *s_time_period_layer;
static TextLayer *s_weather_temp_layer;
static TextLayer *s_weather_condition_layer;
static TextLayer *s_battery_percent_layer;
static TextLayer *s_battery_status_layer;
static TextLayer *s_date_layer;
static BitmapLayer *s_weather_icon_layer;
static BitmapLayer *s_battery_icon_layer;
static GBitmap *s_weather_icon;
static GBitmap *s_battery_icon;

static int s_bounds_width = 0;
static int s_minute_line_y = 0;
static int s_minute_gap = 0;
static int s_minute_height = 0;
static int s_period_offset_y = 0;
static int s_period_height = 0;
static int s_content_x = 0;
static int s_content_width = 0;
static int s_icon_padding = 0;

// Settings with defaults
static bool s_use_words = true;
static bool s_is_24h = false;
static bool s_use_celsius = false;
static bool s_invert_colors = false;
static bool s_show_weather = true;
static bool s_show_battery = true;
static bool s_show_date = true;
static int s_color_theme __attribute__((unused)) = 0; // 0=default, 1=blue, 2=red, 3=green, 4=purple, 5=orange, 6=teal

// Data storage
static char s_time_buffer[64];
static char s_weather_temp_buffer[8];
static char s_weather_condition_buffer[32];
static char s_battery_percent_buffer[8];
static char s_battery_status_buffer[16];
static char s_date_buffer[32];

// Platform detection for dynamic sizing
static bool is_emery = false;

// Scale helper function
static int scale(int base_value) {
  return is_emery ? (base_value * 200 / 144) : base_value;
}
static void layout_time_line(void);
static void layout_weather_section_with_icon_size(GSize icon_size);
static void layout_battery_section_with_icon_size(GSize icon_size);
static GBitmap *scale_bitmap_to_fit(GBitmap *source, int max_dimension);
static GBitmap *create_scaled_icon(uint32_t resource_id, int max_dimension);
static void update_text_colors(void);


// Time word conversion arrays
static const char* const HOUR_WORDS[] = {
  "TWELVE", "ONE", "TWO", "THREE", "FOUR", "FIVE",
  "SIX", "SEVEN", "EIGHT", "NINE", "TEN", "ELEVEN"
};

static const char* const MINUTE_WORDS[] = {
  "ZERO", "ONE", "TWO", "THREE", "FOUR", "FIVE", "SIX", "SEVEN", "EIGHT", "NINE",
  "TEN", "ELEVEN", "TWELVE", "THIRTEEN", "FOURTEEN", "FIFTEEN", "SIXTEEN", "SEVENTEEN",
  "EIGHTEEN", "NINETEEN", "TWENTY", "TWENTY ONE", "TWENTY TWO", "TWENTY THREE",
  "TWENTY FOUR", "TWENTY FIVE", "TWENTY SIX", "TWENTY SEVEN", "TWENTY EIGHT",
  "TWENTY NINE", "THIRTY", "THIRTY ONE", "THIRTY TWO", "THIRTY THREE", "THIRTY FOUR",
  "THIRTY FIVE", "THIRTY SIX", "THIRTY SEVEN", "THIRTY EIGHT", "THIRTY NINE", "FORTY",
  "FORTY ONE", "FORTY TWO", "FORTY THREE", "FORTY FOUR", "FORTY FIVE", "FORTY SIX",
  "FORTY SEVEN", "FORTY EIGHT", "FORTY NINE", "FIFTY", "FIFTY ONE", "FIFTY TWO",
  "FIFTY THREE", "FIFTY FOUR", "FIFTY FIVE", "FIFTY SIX", "FIFTY SEVEN", "FIFTY EIGHT",
  "FIFTY NINE"
};

static void update_time() {
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);
  
  APP_LOG(APP_LOG_LEVEL_DEBUG, "update_time: use_words=%d, is_24h=%d", s_use_words, s_is_24h);
  
  if (s_use_words) {
    // Word-based time display
    int hour = tick_time->tm_hour;
    int minute = tick_time->tm_min;
    bool is_pm = hour >= 12;
    
    // Convert to 12-hour format for words
    if (hour > 12) hour -= 12;
    if (hour == 0) hour = 12;
    
    text_layer_set_text(s_time_hour_layer, HOUR_WORDS[hour % 12]);
    text_layer_set_text(s_time_minute_layer, MINUTE_WORDS[minute]);
    text_layer_set_text(s_time_period_layer, is_pm ? "PM" : "AM");
  } else {
    // Numeric time display
    if (s_is_24h) {
      strftime(s_time_buffer, sizeof(s_time_buffer), "%H:%M", tick_time);
      text_layer_set_text(s_time_hour_layer, s_time_buffer);
      text_layer_set_text(s_time_minute_layer, "");
      text_layer_set_text(s_time_period_layer, "");
    } else {
      strftime(s_time_buffer, sizeof(s_time_buffer), "%I:%M", tick_time);
      text_layer_set_text(s_time_hour_layer, s_time_buffer);
      text_layer_set_text(s_time_minute_layer, "");
      text_layer_set_text(s_time_period_layer, tick_time->tm_hour >= 12 ? "PM" : "AM");
    }
  }

  layout_time_line();
}

static void layout_time_line(void) {
  if (!s_time_minute_layer || !s_time_period_layer || s_bounds_width == 0) {
    return;
  }

  const char *minute_text = text_layer_get_text(s_time_minute_layer);
  const char *period_text = text_layer_get_text(s_time_period_layer);

  GFont minute_font = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  GFont period_font = fonts_get_system_font(FONT_KEY_GOTHIC_14);

  GSize minute_size = {0, 0};
  if (minute_text && minute_text[0]) {
    minute_size = graphics_text_layout_get_content_size(minute_text,
                                                       minute_font,
                                                       GRect(0, 0, s_bounds_width, s_minute_height ? s_minute_height * 2 : 60),
                                                       GTextOverflowModeTrailingEllipsis,
                                                       GTextAlignmentLeft);
    minute_size.w += scale(2);
    if (minute_size.w > s_bounds_width) {
      minute_size.w = s_bounds_width;
    }
  }

  GSize period_size = {0, 0};
  if (period_text && period_text[0]) {
    period_size = graphics_text_layout_get_content_size(period_text,
                                                        period_font,
                                                        GRect(0, 0, s_bounds_width, s_period_height ? s_period_height * 2 : 40),
                                                        GTextOverflowModeTrailingEllipsis,
                                                        GTextAlignmentLeft);
    period_size.w += scale(2);
    if (period_size.w > s_bounds_width) {
      period_size.w = s_bounds_width;
    }
  }

  Layer *minute_layer = text_layer_get_layer(s_time_minute_layer);
  Layer *period_layer = text_layer_get_layer(s_time_period_layer);

  // Determine spacing and total width
  int gap = s_minute_gap;
  if (gap == 0) {
    gap = scale(6);
  }
  if (minute_size.w == 0 || period_size.w == 0) {
    gap = 0;
  }

  int total_width = minute_size.w + gap + period_size.w;
  if (total_width > s_bounds_width) {
    total_width = s_bounds_width;
  }

  int start_x = (s_bounds_width - total_width) / 2;

  if (minute_size.w > 0) {
    layer_set_hidden(minute_layer, false);
    layer_set_frame(minute_layer, GRect(start_x, s_minute_line_y, minute_size.w, s_minute_height ? s_minute_height : scale(28)));
    start_x += minute_size.w + gap;
  } else {
    layer_set_hidden(minute_layer, true);
  }

  if (period_size.w > 0) {
    layer_set_hidden(period_layer, false);
    layer_set_frame(period_layer, GRect(start_x, s_minute_line_y + (s_period_offset_y ? s_period_offset_y : scale(8)), period_size.w, s_period_height ? s_period_height : scale(18)));
  } else {
    layer_set_hidden(period_layer, true);
  }
}

static void layout_weather_section_with_icon_size(GSize icon_size) {
  if (!s_weather_icon_layer) {
    return;
  }

  if (icon_size.w <= 0) {
    icon_size.w = scale(20);
  }
  if (icon_size.h <= 0) {
    icon_size.h = scale(20);
  }

  Layer *icon_layer = bitmap_layer_get_layer(s_weather_icon_layer);
  GRect icon_frame = layer_get_frame(icon_layer);
  icon_frame.origin.x = s_content_x;
  icon_frame.size.w = icon_size.w;
  icon_frame.size.h = icon_size.h;
  layer_set_frame(icon_layer, icon_frame);

  int text_start = icon_frame.origin.x + icon_size.w + s_icon_padding;
  int right_edge = s_content_x + s_content_width;
  if (text_start > right_edge) {
    text_start = right_edge;
  }

  int gap = scale(4);

  Layer *temp_layer = text_layer_get_layer(s_weather_temp_layer);
  GRect temp_frame = layer_get_frame(temp_layer);
  int temp_width = temp_frame.size.w > 0 ? temp_frame.size.w : scale(40);
  if (temp_width > right_edge - text_start) {
    temp_width = right_edge - text_start;
  }
  int temp_x = right_edge - temp_width;
  if (temp_x < text_start) {
    temp_x = text_start;
    temp_width = right_edge - text_start;
  }
  if (temp_width < 0) {
    temp_width = 0;
  }
  temp_frame.origin.x = temp_x;
  temp_frame.size.w = temp_width;
  layer_set_frame(temp_layer, temp_frame);

  Layer *condition_layer = text_layer_get_layer(s_weather_condition_layer);
  GRect condition_frame = layer_get_frame(condition_layer);
  condition_frame.origin.x = text_start;
  int condition_width = temp_x - text_start - gap;
  if (condition_width < scale(30)) {
    condition_width = temp_x - text_start;
  }
  if (condition_width < 0) {
    condition_width = 0;
  }
  condition_frame.size.w = condition_width;
  layer_set_frame(condition_layer, condition_frame);
}

static void layout_battery_section_with_icon_size(GSize icon_size) {
  if (!s_battery_icon_layer) {
    return;
  }

  if (icon_size.w <= 0) {
    icon_size.w = scale(20);
  }
  if (icon_size.h <= 0) {
    icon_size.h = scale(20);
  }

  Layer *icon_layer = bitmap_layer_get_layer(s_battery_icon_layer);
  GRect icon_frame = layer_get_frame(icon_layer);
  icon_frame.origin.x = s_content_x;
  icon_frame.size.w = icon_size.w;
  icon_frame.size.h = icon_size.h;
  layer_set_frame(icon_layer, icon_frame);

  int text_start = icon_frame.origin.x + icon_size.w + s_icon_padding;
  int right_edge = s_content_x + s_content_width;
  if (text_start > right_edge) {
    text_start = right_edge;
  }

  int gap = scale(4);

  Layer *percent_layer = text_layer_get_layer(s_battery_percent_layer);
  GRect percent_frame = layer_get_frame(percent_layer);
  int percent_width = percent_frame.size.w > 0 ? percent_frame.size.w : scale(40);
  if (percent_width > right_edge - text_start) {
    percent_width = right_edge - text_start;
  }
  int percent_x = right_edge - percent_width;
  if (percent_x < text_start) {
    percent_x = text_start;
    percent_width = right_edge - text_start;
  }
  if (percent_width < 0) {
    percent_width = 0;
  }
  percent_frame.origin.x = percent_x;
  percent_frame.size.w = percent_width;
  layer_set_frame(percent_layer, percent_frame);

  Layer *status_layer = text_layer_get_layer(s_battery_status_layer);
  GRect status_frame = layer_get_frame(status_layer);
  status_frame.origin.x = text_start;
  int status_width = percent_x - text_start - gap;
  if (status_width < scale(30)) {
    status_width = percent_x - text_start;
  }
  if (status_width < 0) {
    status_width = 0;
  }
  status_frame.size.w = status_width;
  layer_set_frame(status_layer, status_frame);
}

static inline uint8_t read_bit(const uint8_t *data, int row_bytes, int x, int y) {
  return (data[y * row_bytes + x / 8] >> (7 - (x % 8))) & 1;
}

static inline void write_bit(uint8_t *data, int row_bytes, int x, int y, uint8_t value) {
  uint8_t *byte = &data[y * row_bytes + x / 8];
  uint8_t mask = 1 << (7 - (x % 8));
  if (value) {
    *byte |= mask;
  } else {
    *byte &= (uint8_t)(~mask);
  }
}

static GBitmap *scale_bitmap_to_fit(GBitmap *source, int max_dimension) {
  if (!source || max_dimension <= 0) {
    return source;
  }

  GRect src_bounds = gbitmap_get_bounds(source);
  int src_w = src_bounds.size.w;
  int src_h = src_bounds.size.h;
  int max_src_dim = src_w > src_h ? src_w : src_h;
  if (max_src_dim <= max_dimension) {
    return source;
  }

  if (max_dimension < 1) {
    max_dimension = 1;
  }

  int scaled_w = (src_w * max_dimension) / max_src_dim;
  int scaled_h = (src_h * max_dimension) / max_src_dim;
  if (scaled_w < 1) scaled_w = 1;
  if (scaled_h < 1) scaled_h = 1;

  GBitmapFormat format = gbitmap_get_format(source);
  GBitmap *scaled = gbitmap_create_blank(GSize(scaled_w, scaled_h), format);
  if (!scaled) {
    return source;
  }

  if (format == GBitmapFormat8Bit) {
    gbitmap_set_palette(scaled, gbitmap_get_palette(source), false);
  }

  uint8_t *src_data = gbitmap_get_data(source);
  uint8_t *dst_data = gbitmap_get_data(scaled);
  int src_row_bytes = gbitmap_get_bytes_per_row(source);
  int dst_row_bytes = gbitmap_get_bytes_per_row(scaled);

  memset(dst_data, 0, (size_t)dst_row_bytes * scaled_h);

  for (int y = 0; y < scaled_h; y++) {
    int src_y = (y * src_h) / scaled_h;
    for (int x = 0; x < scaled_w; x++) {
      int src_x = (x * src_w) / scaled_w;

      switch (format) {
        case GBitmapFormat1Bit:
        case GBitmapFormat1BitPalette: {
          uint8_t bit = read_bit(src_data, src_row_bytes, src_x, src_y);
          write_bit(dst_data, dst_row_bytes, x, y, bit);
          break;
        }
        case GBitmapFormat8Bit: {
          dst_data[y * dst_row_bytes + x] = src_data[src_y * src_row_bytes + src_x];
          break;
        }
        default:
          // Unsupported format, return original
          gbitmap_destroy(scaled);
          return source;
      }
    }
  }

  return scaled;
}

static GBitmap *create_scaled_icon(uint32_t resource_id, int max_dimension) {
  GBitmap *original = gbitmap_create_with_resource(resource_id);
  if (!original) {
    return NULL;
  }

  GBitmap *scaled = scale_bitmap_to_fit(original, max_dimension);
  if (scaled != original) {
    gbitmap_destroy(original);
    return scaled;
  }
  return original;
}

static void update_date() {
  if (!s_show_date) return;
  
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);
  
  // Format: "Friday, November 8th"
  static char day_buffer[16];
  static char month_buffer[16];
  
  strftime(day_buffer, sizeof(day_buffer), "%A", tick_time);
  strftime(month_buffer, sizeof(month_buffer), "%B", tick_time);
  
  int day = tick_time->tm_mday;
  const char *suffix = "th";
  if (day == 1 || day == 21 || day == 31) suffix = "st";
  else if (day == 2 || day == 22) suffix = "nd";
  else if (day == 3 || day == 23) suffix = "rd";
  
  snprintf(s_date_buffer, sizeof(s_date_buffer), "%s, %s %d%s", 
           day_buffer, month_buffer, day, suffix);
  text_layer_set_text(s_date_layer, s_date_buffer);
}

static void update_battery() {
  if (!s_show_battery) return;
  
  BatteryChargeState charge_state = battery_state_service_peek();
  int percent = charge_state.charge_percent;
  
  snprintf(s_battery_percent_buffer, sizeof(s_battery_percent_buffer), "%d%%", percent);
  text_layer_set_text(s_battery_percent_layer, s_battery_percent_buffer);
  
  // Update battery status text and icon
  uint32_t battery_icon_id;
  
  if (percent == 100) {
    snprintf(s_battery_status_buffer, sizeof(s_battery_status_buffer), "Full");
    battery_icon_id = s_invert_colors ? RESOURCE_ID_ICON_BATTERY_FULL_WHITE : RESOURCE_ID_ICON_BATTERY_FULL;
  } else if (percent >= 80) {
    snprintf(s_battery_status_buffer, sizeof(s_battery_status_buffer), "Great");
    battery_icon_id = s_invert_colors ? RESOURCE_ID_ICON_BATTERY_FULL_WHITE : RESOURCE_ID_ICON_BATTERY_FULL;
  } else if (percent >= 50) {
    snprintf(s_battery_status_buffer, sizeof(s_battery_status_buffer), "Good");
    battery_icon_id = s_invert_colors ? RESOURCE_ID_ICON_BATTERY_GOOD_WHITE : RESOURCE_ID_ICON_BATTERY_GOOD;
  } else if (percent >= 20) {
    snprintf(s_battery_status_buffer, sizeof(s_battery_status_buffer), "Low");
    battery_icon_id = s_invert_colors ? RESOURCE_ID_ICON_BATTERY_LOW_WHITE : RESOURCE_ID_ICON_BATTERY_LOW;
  } else {
    snprintf(s_battery_status_buffer, sizeof(s_battery_status_buffer), "Low");
    battery_icon_id = s_invert_colors ? RESOURCE_ID_ICON_BATTERY_WARNING_WHITE : RESOURCE_ID_ICON_BATTERY_WARNING;
  }
  
  text_layer_set_text(s_battery_status_layer, s_battery_status_buffer);
  
  // Update battery icon
  if (s_battery_icon) {
    gbitmap_destroy(s_battery_icon);
  }
  s_battery_icon = gbitmap_create_with_resource(battery_icon_id);
  bitmap_layer_set_bitmap(s_battery_icon_layer, s_battery_icon);
}static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time();
  
  // Update weather every 30 minutes
  if (tick_time->tm_min % 30 == 0) {
    // Request weather update from phone
    DictionaryIterator *iter;
    app_message_outbox_begin(&iter);
    dict_write_uint8(iter, MESSAGE_KEY_Temperature, 1);
    app_message_outbox_send();
  }
}

static void battery_callback(BatteryChargeState state) {
  update_battery();
}

static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "=== inbox_received_callback START ===");
  
  // Read weather data from phone
  Tuple *temp_tuple = dict_find(iterator, MESSAGE_KEY_Temperature);
  Tuple *condition_tuple = dict_find(iterator, MESSAGE_KEY_Condition);
  Tuple *icon_tuple = dict_find(iterator, MESSAGE_KEY_WeatherIcon);
  
  if (temp_tuple && s_show_weather) {
    snprintf(s_weather_temp_buffer, sizeof(s_weather_temp_buffer), "%d°%c", 
             (int)temp_tuple->value->int32, s_use_celsius ? 'C' : 'F');
    text_layer_set_text(s_weather_temp_layer, s_weather_temp_buffer);
  }
  
  if (condition_tuple && s_show_weather) {
    snprintf(s_weather_condition_buffer, sizeof(s_weather_condition_buffer), "%s", 
             condition_tuple->value->cstring);
    text_layer_set_text(s_weather_condition_layer, s_weather_condition_buffer);
  }
  
  if (icon_tuple && s_show_weather) {
    // Update weather icon based on condition
    if (s_weather_icon) {
      gbitmap_destroy(s_weather_icon);
    }
    
    int icon_id = (int)icon_tuple->value->int32;
    uint32_t resource_id;
    
    // Choose icon based on weather condition and invert setting
    if (s_invert_colors) {
      resource_id = RESOURCE_ID_ICON_CLOUD_WHITE; // Default white
      switch(icon_id) {
        case 0: resource_id = RESOURCE_ID_ICON_SUN_WHITE; break;
        case 1: resource_id = RESOURCE_ID_ICON_CLOUD_WHITE; break;
        case 2: resource_id = RESOURCE_ID_ICON_RAIN_WHITE; break;
        case 3: resource_id = RESOURCE_ID_ICON_SNOW_WHITE; break;
        case 4: resource_id = RESOURCE_ID_ICON_THUNDER_WHITE; break;
      }
    } else {
      resource_id = RESOURCE_ID_ICON_CLOUD; // Default black
      switch(icon_id) {
        case 0: resource_id = RESOURCE_ID_ICON_SUN; break;
        case 1: resource_id = RESOURCE_ID_ICON_CLOUD; break;
        case 2: resource_id = RESOURCE_ID_ICON_RAIN; break;
        case 3: resource_id = RESOURCE_ID_ICON_SNOW; break;
        case 4: resource_id = RESOURCE_ID_ICON_THUNDER; break;
      }
    }
    
    s_weather_icon = gbitmap_create_with_resource(resource_id);
    bitmap_layer_set_bitmap(s_weather_icon_layer, s_weather_icon);
  }
  
  // Read settings
  APP_LOG(APP_LOG_LEVEL_DEBUG, "=== Reading Settings ===");
  
  Tuple *temp_unit_tuple = dict_find(iterator, MESSAGE_KEY_TemperatureUnit);
  if (temp_unit_tuple) {
    s_use_celsius = temp_unit_tuple->value->int32 == 1;
    APP_LOG(APP_LOG_LEVEL_DEBUG, "TemperatureUnit: %d (Celsius: %d)", (int)temp_unit_tuple->value->int32, s_use_celsius);
  }
  
  Tuple *time_format_tuple = dict_find(iterator, MESSAGE_KEY_TimeFormat);
  if (time_format_tuple) {
    int format = (int)time_format_tuple->value->int32;
    s_use_words = (format == 0);
    s_is_24h = (format == 2);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "TimeFormat: %d (use_words: %d, is_24h: %d)", format, s_use_words, s_is_24h);
    update_time();
  }
  
  // Check for InvertColors
  Tuple *invert_colors_tuple = dict_find(iterator, MESSAGE_KEY_InvertColors);
  if (invert_colors_tuple) {
    s_invert_colors = invert_colors_tuple->value->int32 == 1;
    APP_LOG(APP_LOG_LEVEL_DEBUG, "InvertColors: %d (s_invert_colors: %d)", (int)invert_colors_tuple->value->int32, s_invert_colors);
    update_text_colors();
    layer_mark_dirty(s_canvas_layer);
  } else {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "InvertColors: NOT FOUND in message");
  }
  
  // Check for ColorTheme
  Tuple *color_theme_tuple = dict_find(iterator, MESSAGE_KEY_ColorTheme);
  if (color_theme_tuple) {
    s_color_theme = (int)color_theme_tuple->value->int32;
    APP_LOG(APP_LOG_LEVEL_DEBUG, "ColorTheme: %d", s_color_theme);
    layer_mark_dirty(s_canvas_layer);
  } else {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "ColorTheme: NOT FOUND in message");
  }
  
  Tuple *show_weather_tuple = dict_find(iterator, MESSAGE_KEY_ShowWeather);
  if (show_weather_tuple) {
    s_show_weather = show_weather_tuple->value->int32 == 1;
    APP_LOG(APP_LOG_LEVEL_DEBUG, "ShowWeather: %d", s_show_weather);
    layer_set_hidden(bitmap_layer_get_layer(s_weather_icon_layer), !s_show_weather);
    layer_set_hidden(text_layer_get_layer(s_weather_temp_layer), !s_show_weather);
    layer_set_hidden(text_layer_get_layer(s_weather_condition_layer), !s_show_weather);
  }
  
  Tuple *show_battery_tuple = dict_find(iterator, MESSAGE_KEY_ShowBattery);
  if (show_battery_tuple) {
    s_show_battery = show_battery_tuple->value->int32 == 1;
    APP_LOG(APP_LOG_LEVEL_DEBUG, "ShowBattery: %d", s_show_battery);
    layer_set_hidden(bitmap_layer_get_layer(s_battery_icon_layer), !s_show_battery);
    layer_set_hidden(text_layer_get_layer(s_battery_percent_layer), !s_show_battery);
    layer_set_hidden(text_layer_get_layer(s_battery_status_layer), !s_show_battery);
  }
  
  Tuple *show_date_tuple = dict_find(iterator, MESSAGE_KEY_ShowDate);
  if (show_date_tuple) {
    s_show_date = show_date_tuple->value->int32 == 1;
    APP_LOG(APP_LOG_LEVEL_DEBUG, "ShowDate: %d", s_show_date);
    layer_set_hidden(text_layer_get_layer(s_date_layer), !s_show_date);
    if (s_show_date) update_date();
  }
  
  APP_LOG(APP_LOG_LEVEL_DEBUG, "=== inbox_received_callback END ===");
}

static void update_text_colors(void) {
  // Set text colors based on invert setting
  GColor primary_text_color = s_invert_colors ? GColorWhite : GColorBlack;
  GColor secondary_text_color = s_invert_colors ? GColorLightGray : GColorDarkGray;
  
  // Update all text layers
  text_layer_set_text_color(s_time_hour_layer, primary_text_color);
  text_layer_set_text_color(s_time_minute_layer, primary_text_color);
  text_layer_set_text_color(s_time_period_layer, secondary_text_color);
  text_layer_set_text_color(s_weather_temp_layer, primary_text_color);
  text_layer_set_text_color(s_weather_condition_layer, secondary_text_color);
  text_layer_set_text_color(s_battery_percent_layer, primary_text_color);
  text_layer_set_text_color(s_battery_status_layer, secondary_text_color);
  text_layer_set_text_color(s_date_layer, secondary_text_color);
  
  // Reload icons with correct color version
  update_battery();
  
  // Force weather icon refresh by requesting new weather data
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);
  dict_write_uint8(iter, MESSAGE_KEY_Temperature, 1);
  app_message_outbox_send();
}static void canvas_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  
  // Set colors based on theme and invert setting
  GColor card_color = GColorWhite;
  
  if (s_invert_colors) {
    card_color = GColorBlack;
  }
  
  #ifdef PBL_COLOR
  // Apply color theme (only on color displays)
  if (s_color_theme > 0 && !s_invert_colors) {
    switch(s_color_theme) {
      case 1: card_color = GColorBlue; break;
      case 2: card_color = GColorRed; break;
      case 3: card_color = GColorGreen; break;
      case 4: card_color = GColorPurple; break;
      case 5: card_color = GColorOrange; break;
      case 6: card_color = GColorTiffanyBlue; break; // Teal
    }
  }
  #endif
  
  // Draw background - fill entire screen with card color (no border)
  graphics_context_set_fill_color(ctx, card_color);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
}

static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  
  // Detect platform for scaling
  is_emery = (bounds.size.w == 200);
  
  // Create canvas layer for background and card
  s_canvas_layer = layer_create(bounds);
  layer_set_update_proc(s_canvas_layer, canvas_update_proc);
  layer_add_child(window_layer, s_canvas_layer);
  
  // Calculate positions (scaled for emery) - reduced margins
  int padding = scale(5);
  int content_x = padding;  // Reduced from 25 to 5
  int content_width = bounds.size.w - (2 * padding);
  s_content_x = content_x;
  s_content_width = content_width;
  s_icon_padding = padding;
  
  // Time section (top) - stacked and centered, closer to top
  int time_y = scale(5);
  s_bounds_width = bounds.size.w;
  s_minute_gap = scale(6);
  
  // Hour on top, centered
  s_time_hour_layer = text_layer_create(GRect(0, time_y, bounds.size.w, scale(32)));
  text_layer_set_background_color(s_time_hour_layer, GColorClear);
  text_layer_set_text_color(s_time_hour_layer, GColorBlack);
  text_layer_set_font(s_time_hour_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  text_layer_set_text_alignment(s_time_hour_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_time_hour_layer));
  
  // Minute and AM/PM on same line, centered as a dynamically sized group
  int minute_line_y = time_y + scale(30);
  s_minute_line_y = minute_line_y;
  s_minute_height = scale(28);
  s_period_offset_y = scale(8);
  s_period_height = scale(18);
  
  s_time_minute_layer = text_layer_create(GRect(0, minute_line_y, bounds.size.w, s_minute_height));
  text_layer_set_background_color(s_time_minute_layer, GColorClear);
  text_layer_set_text_color(s_time_minute_layer, GColorBlack);
  text_layer_set_font(s_time_minute_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_time_minute_layer, GTextAlignmentLeft);
  layer_add_child(window_layer, text_layer_get_layer(s_time_minute_layer));
  
  s_time_period_layer = text_layer_create(GRect(0, minute_line_y + s_period_offset_y, bounds.size.w, s_period_height));
  text_layer_set_background_color(s_time_period_layer, GColorClear);
  text_layer_set_text_color(s_time_period_layer, GColorDarkGray);
  text_layer_set_font(s_time_period_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_time_period_layer, GTextAlignmentLeft);
  layer_add_child(window_layer, text_layer_get_layer(s_time_period_layer));
  
  // Weather section
  int weather_y = time_y + scale(65);
  
  int icon_base = scale(14);

  s_weather_icon_layer = bitmap_layer_create(GRect(content_x, weather_y, icon_base, icon_base));
  bitmap_layer_set_background_color(s_weather_icon_layer, GColorClear);
  bitmap_layer_set_compositing_mode(s_weather_icon_layer, GCompOpSet);
  layer_add_child(window_layer, bitmap_layer_get_layer(s_weather_icon_layer));
  
  s_weather_condition_layer = text_layer_create(GRect(content_x + icon_base + scale(5), weather_y - scale(2), 
                                                       scale(80), scale(20)));
  text_layer_set_background_color(s_weather_condition_layer, GColorClear);
  text_layer_set_text_color(s_weather_condition_layer, GColorDarkGray);
  text_layer_set_font(s_weather_condition_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text(s_weather_condition_layer, "Loading...");
  layer_add_child(window_layer, text_layer_get_layer(s_weather_condition_layer));
  
  s_weather_temp_layer = text_layer_create(GRect(bounds.size.w - scale(45), weather_y - scale(2), 
                                                  scale(40), scale(20)));
  text_layer_set_background_color(s_weather_temp_layer, GColorClear);
  text_layer_set_text_color(s_weather_temp_layer, GColorBlack);
  text_layer_set_font(s_weather_temp_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(s_weather_temp_layer, GTextAlignmentRight);
  layer_add_child(window_layer, text_layer_get_layer(s_weather_temp_layer));
  
  // Battery section
  int battery_y = weather_y + scale(25);
  
  s_battery_icon_layer = bitmap_layer_create(GRect(content_x, battery_y, icon_base, icon_base));
  bitmap_layer_set_background_color(s_battery_icon_layer, GColorClear);
  bitmap_layer_set_compositing_mode(s_battery_icon_layer, GCompOpSet);
  // Icon will be set by update_battery()
  layer_add_child(window_layer, bitmap_layer_get_layer(s_battery_icon_layer));
  
  s_battery_status_layer = text_layer_create(GRect(content_x + icon_base + scale(5), battery_y - scale(2), 
                                                    scale(80), scale(20)));
  text_layer_set_background_color(s_battery_status_layer, GColorClear);
  text_layer_set_text_color(s_battery_status_layer, GColorDarkGray);
  text_layer_set_font(s_battery_status_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  layer_add_child(window_layer, text_layer_get_layer(s_battery_status_layer));
  
  s_battery_percent_layer = text_layer_create(GRect(bounds.size.w - scale(45), battery_y - scale(2), 
                                                     scale(40), scale(20)));
  text_layer_set_background_color(s_battery_percent_layer, GColorClear);
  text_layer_set_text_color(s_battery_percent_layer, GColorBlack);
  text_layer_set_font(s_battery_percent_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(s_battery_percent_layer, GTextAlignmentRight);
  layer_add_child(window_layer, text_layer_get_layer(s_battery_percent_layer));
  
  // Date section
  int date_y = battery_y + scale(30);
  
  s_date_layer = text_layer_create(GRect(content_x, date_y, content_width, scale(18)));
  text_layer_set_background_color(s_date_layer, GColorClear);
  text_layer_set_text_color(s_date_layer, GColorDarkGray);
  text_layer_set_font(s_date_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_date_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_date_layer));
  
  // Ensure text/icon colors respect current invert setting at startup
  update_text_colors();

  // Initialize displays
  update_time();
  update_date();
  update_battery();
  
  // Request initial weather data
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);
  dict_write_uint8(iter, MESSAGE_KEY_Temperature, 1);
  app_message_outbox_send();
}

static void main_window_unload(Window *window) {
  layer_destroy(s_canvas_layer);
  text_layer_destroy(s_time_hour_layer);
  text_layer_destroy(s_time_minute_layer);
  text_layer_destroy(s_time_period_layer);
  text_layer_destroy(s_weather_temp_layer);
  text_layer_destroy(s_weather_condition_layer);
  text_layer_destroy(s_battery_percent_layer);
  text_layer_destroy(s_battery_status_layer);
  text_layer_destroy(s_date_layer);
  bitmap_layer_destroy(s_weather_icon_layer);
  bitmap_layer_destroy(s_battery_icon_layer);
  
  if (s_weather_icon) {
    gbitmap_destroy(s_weather_icon);
  }
  if (s_battery_icon) {
    gbitmap_destroy(s_battery_icon);
  }
}

static void init() {
  // Create main window
  s_main_window = window_create();
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });
  window_stack_push(s_main_window, true);
  
  // Register services
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  battery_state_service_subscribe(battery_callback);
  
  // Register AppMessage callbacks
  app_message_register_inbox_received(inbox_received_callback);
  app_message_open(256, 256);
}

static void deinit() {
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}

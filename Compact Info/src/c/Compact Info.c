#include <pebble.h>
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
    battery_icon_id = RESOURCE_ID_ICON_BATTERY_FULL;
  } else if (percent >= 80) {
    snprintf(s_battery_status_buffer, sizeof(s_battery_status_buffer), "Great");
    battery_icon_id = RESOURCE_ID_ICON_BATTERY_FULL;
  } else if (percent >= 50) {
    snprintf(s_battery_status_buffer, sizeof(s_battery_status_buffer), "Good");
    battery_icon_id = RESOURCE_ID_ICON_BATTERY_GOOD;
  } else if (percent >= 20) {
    snprintf(s_battery_status_buffer, sizeof(s_battery_status_buffer), "Low");
    battery_icon_id = RESOURCE_ID_ICON_BATTERY_LOW;
  } else {
    snprintf(s_battery_status_buffer, sizeof(s_battery_status_buffer), "Low");
    battery_icon_id = RESOURCE_ID_ICON_BATTERY_WARNING;
  }
  
  text_layer_set_text(s_battery_status_layer, s_battery_status_buffer);
  
  // Update battery icon
  if (s_battery_icon) {
    gbitmap_destroy(s_battery_icon);
  }
  s_battery_icon = gbitmap_create_with_resource(battery_icon_id);
  bitmap_layer_set_bitmap(s_battery_icon_layer, s_battery_icon);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
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
    uint32_t resource_id = RESOURCE_ID_ICON_CLOUD; // Default
    
    switch(icon_id) {
      case 0: resource_id = RESOURCE_ID_ICON_SUN; break;
      case 1: resource_id = RESOURCE_ID_ICON_CLOUD; break;
      case 2: resource_id = RESOURCE_ID_ICON_RAIN; break;
      case 3: resource_id = RESOURCE_ID_ICON_SNOW; break;
      case 4: resource_id = RESOURCE_ID_ICON_THUNDER; break;
    }
    
    s_weather_icon = gbitmap_create_with_resource(resource_id);
    bitmap_layer_set_bitmap(s_weather_icon_layer, s_weather_icon);
  }
  
  // Read settings
  Tuple *temp_unit_tuple = dict_find(iterator, MESSAGE_KEY_TemperatureUnit);
  if (temp_unit_tuple) {
    s_use_celsius = temp_unit_tuple->value->int32 == 1;
  }
  
  Tuple *time_format_tuple = dict_find(iterator, MESSAGE_KEY_TimeFormat);
  if (time_format_tuple) {
    int format = (int)time_format_tuple->value->int32;
    s_use_words = (format == 0);
    s_is_24h = (format == 2);
    update_time();
  }
  
  Tuple *show_weather_tuple = dict_find(iterator, MESSAGE_KEY_ShowWeather);
  if (show_weather_tuple) {
    s_show_weather = show_weather_tuple->value->int32 == 1;
    layer_set_hidden(bitmap_layer_get_layer(s_weather_icon_layer), !s_show_weather);
    layer_set_hidden(text_layer_get_layer(s_weather_temp_layer), !s_show_weather);
    layer_set_hidden(text_layer_get_layer(s_weather_condition_layer), !s_show_weather);
  }
  
  Tuple *show_battery_tuple = dict_find(iterator, MESSAGE_KEY_ShowBattery);
  if (show_battery_tuple) {
    s_show_battery = show_battery_tuple->value->int32 == 1;
    layer_set_hidden(bitmap_layer_get_layer(s_battery_icon_layer), !s_show_battery);
    layer_set_hidden(text_layer_get_layer(s_battery_percent_layer), !s_show_battery);
    layer_set_hidden(text_layer_get_layer(s_battery_status_layer), !s_show_battery);
  }
  
  Tuple *show_date_tuple = dict_find(iterator, MESSAGE_KEY_ShowDate);
  if (show_date_tuple) {
    s_show_date = show_date_tuple->value->int32 == 1;
    layer_set_hidden(text_layer_get_layer(s_date_layer), !s_show_date);
    if (s_show_date) update_date();
  }
}

static void canvas_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  
  // Set colors based on theme and invert setting
  GColor bg_color = GColorBlack;
  GColor card_color = GColorWhite;
  
  if (s_invert_colors) {
    bg_color = GColorWhite;
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
  
  // Draw background
  graphics_context_set_fill_color(ctx, bg_color);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  
  // Draw main card with rounded corners
  int card_margin = scale(10);
  int card_radius = scale(8);
  GRect card_rect = GRect(card_margin, card_margin, 
                          bounds.size.w - (2 * card_margin), 
                          bounds.size.h - (2 * card_margin));
  
  graphics_context_set_fill_color(ctx, card_color);
  graphics_fill_rect(ctx, card_rect, card_radius, GCornersAll);
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
  
  // Calculate positions (scaled for emery)
  int card_margin = scale(10);
  int content_x = card_margin + scale(15);
  int content_width = bounds.size.w - (2 * card_margin) - (2 * scale(15));
  
  // Time section (top)
  int time_y = card_margin + scale(15);
  
  s_time_hour_layer = text_layer_create(GRect(content_x, time_y, content_width, scale(30)));
  text_layer_set_background_color(s_time_hour_layer, GColorClear);
  text_layer_set_text_color(s_time_hour_layer, GColorBlack);
  text_layer_set_font(s_time_hour_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  text_layer_set_text_alignment(s_time_hour_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_time_hour_layer));
  
  s_time_minute_layer = text_layer_create(GRect(content_x, time_y + scale(30), content_width, scale(25)));
  text_layer_set_background_color(s_time_minute_layer, GColorClear);
  text_layer_set_text_color(s_time_minute_layer, GColorBlack);
  text_layer_set_font(s_time_minute_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_time_minute_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_time_minute_layer));
  
  s_time_period_layer = text_layer_create(GRect(content_x, time_y + scale(55), content_width, scale(18)));
  text_layer_set_background_color(s_time_period_layer, GColorClear);
  text_layer_set_text_color(s_time_period_layer, GColorDarkGray);
  text_layer_set_font(s_time_period_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_time_period_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_time_period_layer));
  
  // Weather section
  int weather_y = time_y + scale(80);
  int icon_size = scale(20);
  
  s_weather_icon_layer = bitmap_layer_create(GRect(content_x, weather_y, icon_size, icon_size));
  bitmap_layer_set_background_color(s_weather_icon_layer, GColorClear);
  layer_add_child(window_layer, bitmap_layer_get_layer(s_weather_icon_layer));
  
  s_weather_condition_layer = text_layer_create(GRect(content_x + icon_size + scale(5), weather_y - scale(2), 
                                                       scale(60), scale(18)));
  text_layer_set_background_color(s_weather_condition_layer, GColorClear);
  text_layer_set_text_color(s_weather_condition_layer, GColorDarkGray);
  text_layer_set_font(s_weather_condition_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text(s_weather_condition_layer, "Loading...");
  layer_add_child(window_layer, text_layer_get_layer(s_weather_condition_layer));
  
  s_weather_temp_layer = text_layer_create(GRect(content_x + icon_size + scale(68), weather_y - scale(2), 
                                                  scale(40), scale(18)));
  text_layer_set_background_color(s_weather_temp_layer, GColorClear);
  text_layer_set_text_color(s_weather_temp_layer, GColorBlack);
  text_layer_set_font(s_weather_temp_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD));
  text_layer_set_text_alignment(s_weather_temp_layer, GTextAlignmentRight);
  layer_add_child(window_layer, text_layer_get_layer(s_weather_temp_layer));
  
  // Battery section
  int battery_y = weather_y + scale(25);
  
  s_battery_icon_layer = bitmap_layer_create(GRect(content_x, battery_y, icon_size, icon_size));
  bitmap_layer_set_background_color(s_battery_icon_layer, GColorClear);
  // Icon will be set by update_battery()
  layer_add_child(window_layer, bitmap_layer_get_layer(s_battery_icon_layer));
  
  s_battery_status_layer = text_layer_create(GRect(content_x + icon_size + scale(5), battery_y - scale(2), 
                                                    scale(60), scale(18)));
  text_layer_set_background_color(s_battery_status_layer, GColorClear);
  text_layer_set_text_color(s_battery_status_layer, GColorDarkGray);
  text_layer_set_font(s_battery_status_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  layer_add_child(window_layer, text_layer_get_layer(s_battery_status_layer));
  
  s_battery_percent_layer = text_layer_create(GRect(content_x + icon_size + scale(68), battery_y - scale(2), 
                                                     scale(40), scale(18)));
  text_layer_set_background_color(s_battery_percent_layer, GColorClear);
  text_layer_set_text_color(s_battery_percent_layer, GColorBlack);
  text_layer_set_font(s_battery_percent_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD));
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

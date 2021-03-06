#include <pebble.h>
#include <nightstand/nightstand.h>

static Layer *background_layer;
static Window *window;
static AppTimer *animation_timer;
static TextLayer *time_layer;
bool animation_is_running;
static GPoint center;
bool bluetooth_connected;

#define NUM_PARTICLES 150
#define NUM_HISTORY 10
#define MAX_ANIM_COUNT 150
static int max_anim_count;

// Preferences
#define KEY_SETTINGS 1
typedef struct Settings{
  bool firework_on_minute;
  bool bluetooth_status;
  bool nightstand_mode;
} Settings;

static Settings s_settings;

typedef struct Particle {
  GPoint Pos; //Position of the particle
  GPoint PosHistory[NUM_HISTORY];
  GPoint Vel; //Velocity of the particle
  int age; //Current age of the particle
  int LifeSpan; //Age after which the particle dies
} Particle;

static Particle particles[NUM_PARTICLES];
static int16_t anim_count;

void unobstructed_change(AnimationProgress progress, void* data) {
  GRect unobstructed_bounds = layer_get_unobstructed_bounds(window_get_root_layer(window));
  GRect bounds = layer_get_bounds(window_get_root_layer(window));
  int offset = (bounds.size.h-unobstructed_bounds.size.h)/2;
  // update layer positions
  bounds.origin.y+=(watch_info_get_model() == WATCH_INFO_MODEL_PEBBLE_STEEL) ? 60:48;
  bounds.origin.y-=offset;
  layer_set_frame(text_layer_get_layer(time_layer),bounds);
}

static void loadSettings(void) {
  if(persist_exists(KEY_SETTINGS)) {
    persist_read_data(KEY_SETTINGS,&s_settings,sizeof(Settings));
  } else {
    s_settings.bluetooth_status=true;
    s_settings.nightstand_mode=true;
    s_settings.firework_on_minute=true;
  }
}

static void initParticle(int i,GPoint center) {
  particles[i].Pos.x = rand()%20+center.x;
  particles[i].Pos.y = rand()%20+center.y;
  particles[i].Vel.x = rand()%5-2;
  particles[i].Vel.y = -rand()%20;
  particles[i].age = 0;
  particles[i].LifeSpan = rand()%100;
  for(int j=0;j<NUM_HISTORY;j++)
    particles[i].PosHistory[j].x=-1; // do not draw
}

static void initParticles() {
  GRect bounds = layer_get_bounds(window_get_root_layer(window));
  center.x=rand()%bounds.size.w;
  center.y=rand()%bounds.size.h;
  for(int i=0;i<NUM_PARTICLES;i++) {
    initParticle(i,center);
  }
}

static void animate(void *data){
  if(++anim_count<max_anim_count)
    animation_timer = app_timer_register(33,animate,NULL);
  else
  {
    animation_is_running=false;
    if(!s_settings.firework_on_minute)
      max_anim_count=0;
  }
  layer_mark_dirty(background_layer);
}

static void start_animation() {
  if(animation_is_running)
    return;
  anim_count=0;
  initParticles();
  animation_is_running=true;
  animate(NULL);
}

static void in_received_handler(DictionaryIterator *iter, void *context) {

  Tuple *t = dict_find(iter, MESSAGE_KEY_minute);
  if(t) {
    s_settings.firework_on_minute = t->value->int32 == 1;
  }
  if((t = dict_find(iter, MESSAGE_KEY_bluetooth)))
    s_settings.bluetooth_status = t->value->int32 == 1;
  if((t = dict_find(iter, MESSAGE_KEY_nightstand)))
    s_settings.nightstand_mode = t->value->int32 == 1;

  if(!animation_is_running)
    max_anim_count = s_settings.firework_on_minute?MAX_ANIM_COUNT:0;
  layer_mark_dirty(background_layer);
}

static void handle_tick(struct tm *tick_time, TimeUnits units_changed) {
  bool update_time = true;
  if(s_settings.nightstand_mode) {
    update_time = !nightstand_window_update();
  }
  if(update_time) {
    static char time_text[] = "00:00";

    char *time_format;

    if (clock_is_24h_style()) {
      time_format = "%R";
    } else {
      time_format = "%I:%M";
    }

    strftime(time_text, sizeof(time_text), time_format, tick_time);

    // Kludge to handle lack of non-padded hour format string
    // for twelve hour clock.
    if (!clock_is_24h_style() && (time_text[0] == '0')) {
      memmove(time_text, &time_text[1], sizeof(time_text) - 1);
    }

    text_layer_set_text(time_layer, time_text);
    start_animation();
  }
}

static void accel_tap_handler(AccelAxisType axis, int32_t direction) {
  // Process tap on ACCEL_AXIS_X, ACCEL_AXIS_Y or ACCEL_AXIS_Z
  // Direction is 1 or -1
  // blink if enabled
  max_anim_count = MAX_ANIM_COUNT;
  start_animation();
}

static void bluetooth_handler(bool connected) {
  bluetooth_connected = connected;
  start_animation();
  if(!connected && s_settings.bluetooth_status)
    vibes_double_pulse();
}

static void background_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_unobstructed_bounds(layer);
  graphics_context_set_fill_color(ctx,GColorBlack);
  graphics_fill_rect(ctx,bounds,0,GCornerNone);
  graphics_context_set_stroke_color(ctx,GColorWhite);
#ifdef PBL_COLOR
  graphics_context_set_antialiased(ctx, true); // more performance
  if (bluetooth_connected) {
    if(anim_count<10)
    graphics_context_set_stroke_color(ctx,GColorRed);
    else if(anim_count<30)
    graphics_context_set_stroke_color(ctx,GColorOrange);
    else if(anim_count<60)
    graphics_context_set_stroke_color(ctx,GColorRajah);
    else if(anim_count<70)
    graphics_context_set_stroke_color(ctx,GColorChromeYellow);
    else if(anim_count<90)
    graphics_context_set_stroke_color(ctx,GColorYellow);
  }
  else {
    if(anim_count<10)
    graphics_context_set_stroke_color(ctx,GColorBlue);
    else if(anim_count<30)
    graphics_context_set_stroke_color(ctx,GColorBlueMoon);
    else if(anim_count<60)
    graphics_context_set_stroke_color(ctx,GColorCobaltBlue);
    else if(anim_count<70)
    graphics_context_set_stroke_color(ctx,GColorVividCerulean);
    else if(anim_count<90)
    graphics_context_set_stroke_color(ctx,GColorPictonBlue);
  }
#endif

  for(int i=0;i<NUM_PARTICLES;i++) {
    if(particles[i].age<0)
      continue;
    particles[i].Vel.y += 1;
    particles[i].Pos.x += particles[i].Vel.x;
    particles[i].Pos.y += particles[i].Vel.y;
    // insert into history
    memmove(&particles[i].PosHistory[1],&particles[i].PosHistory[0],sizeof(GPoint)*(NUM_HISTORY-1));
    particles[i].PosHistory[0]=particles[i].Pos;
    for(int j=0;j<NUM_HISTORY-1;j++)
      if(particles[i].PosHistory[j+1].x>0)
        graphics_draw_line(ctx,particles[i].PosHistory[j],particles[i].PosHistory[j+1]);
      else
        break;
    particles[i].age++;
    //if(particles[i].Pos.y>168 || particles[i].Pos.y<0) particles[i].Vel.y*=-1;
    //if(particles[i].Pos.x>144 || particles[i].Pos.x<0) particles[i].Vel.x*=-1;
    if(particles[i].age>particles[i].LifeSpan)
      particles[i].age=-1;
  }

}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect unobstructed_bounds = layer_get_unobstructed_bounds(window_get_root_layer(window));
  GRect bounds = layer_get_bounds(window_get_root_layer(window));
  int offset = (bounds.size.h-unobstructed_bounds.size.h)/2;

  // init background
  background_layer = layer_create(bounds);
  layer_set_update_proc(background_layer, background_update_proc);
  layer_add_child(window_layer, background_layer);
  srand(time(NULL));

  // init time

  bounds.origin.y+=(watch_info_get_model() == WATCH_INFO_MODEL_PEBBLE_STEEL) ? 60:48;
#ifdef PBL_PLATFORM_EMERY
  bounds.origin.y+=30;
#endif
  bounds.origin.y-=offset;
  time_layer = text_layer_create(bounds);
  text_layer_set_text_color(time_layer,GColorWhite);
  text_layer_set_background_color(time_layer,GColorClear);
  text_layer_set_text_alignment(time_layer,GTextAlignmentCenter);
  text_layer_set_font(time_layer,fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_DIGITAL_62)));
  text_layer_set_text(time_layer,"00:00");
  layer_add_child(window_layer,text_layer_get_layer(time_layer));

  // force update
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  handle_tick(t, MINUTE_UNIT);
  tick_timer_service_subscribe(MINUTE_UNIT, handle_tick);

  accel_tap_service_subscribe(accel_tap_handler);
  bluetooth_connection_service_subscribe(bluetooth_handler);
  bluetooth_handler(bluetooth_connection_service_peek());
}

static void window_unload(Window *window) {
  layer_destroy(background_layer);
  tick_timer_service_unsubscribe();
}

static void init(void) {
  app_message_open(300,100);
  app_message_register_inbox_received(in_received_handler);
  loadSettings();
  max_anim_count = MAX_ANIM_COUNT;
  window = window_create();
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  nightstand_window_init();
  UnobstructedAreaHandlers handlers = {
    // .will_change = unobstructed_will_change,
    .change = unobstructed_change
    // .did_change = unobstructed_did_change
  };
  unobstructed_area_service_subscribe(handlers, NULL);
  // Push the window onto the stack
  const bool animated = true;
  window_stack_push(window, animated);

}

static void deinit(void) {
  window_destroy(window);
  persist_write_data(KEY_SETTINGS,&s_settings,sizeof(Settings));
  nightstand_window_deinit();
}


int main(void) {
  init();
  app_event_loop();
  deinit();
}

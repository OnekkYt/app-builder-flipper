// School Break Countdown - Flipper Zero App
// Schedule (24h):
//   07:30-08:15  Class
//   08:15-08:20  BREAK
//   08:20-09:05  Class
//   09:05-09:10  BREAK
//   09:10-09:55  Class
//   09:55-10:05  BREAK
//   10:05-10:50  Class
//   10:50-11:00  BREAK
//   11:00-11:45  Class
//   11:45-12:00  BREAK
//   12:00-12:45  Class
//   12:45-12:55  BREAK
//   12:55-13:40  Class
//   13:40-13:45  BREAK
//   13:45-14:30  Class
//   14:30-14:35  BREAK
//   14:35-15:20  Class (last)

#include <furi.h>
#include <gui/gui.h>
#include <gui/elements.h>
#include <input/input.h>
#include <notification/notification_messages.h>
#include <furi_hal_rtc.h>
#include <stdlib.h>
#include <stdio.h>

#define TAG "SchoolBreak"

// ── Schedule definition ─────────────────────────────────────────────────────

typedef struct {
    uint8_t start_h, start_m;
    uint8_t end_h,   end_m;
    bool    is_break;
} Slot;

static const Slot SCHEDULE[] = {
    {7,  30,  8, 15, false},  // Class
    {8,  15,  8, 20, true },  // Break
    {8,  20,  9,  5, false},  // Class
    {9,   5,  9, 10, true },  // Break
    {9,  10,  9, 55, false},  // Class
    {9,  55, 10,  5, true },  // Break
    {10,  5, 10, 50, false},  // Class
    {10, 50, 11,  0, true },  // Break
    {11,  0, 11, 45, false},  // Class
    {11, 45, 12,  0, true },  // Break
    {12,  0, 12, 45, false},  // Class
    {12, 45, 12, 55, true },  // Break
    {12, 55, 13, 40, false},  // Class
    {13, 40, 13, 45, true },  // Break
    {13, 45, 14, 30, false},  // Class
    {14, 30, 14, 35, true },  // Break
    {14, 35, 15, 20, false},  // Last class
};
static const int SCHEDULE_LEN = sizeof(SCHEDULE) / sizeof(SCHEDULE[0]);

// ── App state ────────────────────────────────────────────────────────────────

typedef struct {
    FuriMessageQueue* queue;
    ViewPort*         viewport;
    Gui*              gui;
    NotificationApp*  notif;

    // Cached per-render
    int  current_slot;   // index into SCHEDULE, -1 = before/after day
    int  mins_remaining; // to end of current slot
    int  secs_remaining;
    bool school_over;
    bool before_school;
} AppState;

// ── Helpers ──────────────────────────────────────────────────────────────────

static int slot_start_mins(const Slot* s) { return s->start_h * 60 + s->start_m; }
static int slot_end_mins  (const Slot* s) { return s->end_h   * 60 + s->end_m;   }

// Returns the slot index the current time falls in, or -1
static int find_current_slot(int now_mins) {
    for(int i = 0; i < SCHEDULE_LEN; i++) {
        if(now_mins >= slot_start_mins(&SCHEDULE[i]) &&
           now_mins <  slot_end_mins  (&SCHEDULE[i])) {
            return i;
        }
    }
    return -1;
}

// ── Draw callback ────────────────────────────────────────────────────────────

static void draw_callback(Canvas* canvas, void* ctx) {
    AppState* app = ctx;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    // ── Header bar ──────────────────────────────────────────────────────────
    canvas_draw_box(canvas, 0, 0, 128, 12);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, "SCHOOL TIMER");
    canvas_set_color(canvas, ColorBlack);

    // ── Get live RTC time ────────────────────────────────────────────────────
    FuriHalRtcDateTime dt;
    furi_hal_rtc_get_datetime(&dt);
    int now_mins = dt.hour * 60 + dt.minute;
    int now_secs = dt.hour * 3600 + dt.minute * 60 + dt.second;

    // ── Classify current moment ──────────────────────────────────────────────
    int first_start = slot_start_mins(&SCHEDULE[0]);
    int last_end    = slot_end_mins  (&SCHEDULE[SCHEDULE_LEN - 1]);

    bool school_over   = (now_mins >= last_end);
    bool before_school = (now_mins < first_start);

    int current_slot = find_current_slot(now_mins);

    char line1[32], line2[32], line3[32], time_str[16];

    // Current clock
    snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d",
             dt.hour, dt.minute, dt.second);

    if(before_school) {
        // Time until school starts
        int end_secs = first_start * 60;
        int diff = end_secs - now_secs;
        if(diff < 0) diff = 0;
        int h = diff / 3600, m = (diff % 3600) / 60, s = diff % 60;

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 14, AlignCenter, AlignTop, time_str);

        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 26, AlignCenter, AlignTop, "Before school");

        snprintf(line1, sizeof(line1), "Starts in  %02d:%02d:%02d", h, m, s);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignTop, line1);

    } else if(school_over) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 14, AlignCenter, AlignTop, time_str);

        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignTop, "School's over!");
        canvas_draw_str_aligned(canvas, 64, 44, AlignCenter, AlignTop, "Go home :)");

    } else if(current_slot == -1) {
        // Between slots (shouldn't normally happen with this schedule)
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 14, AlignCenter, AlignTop, time_str);
        canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignTop, "Between slots");

    } else {
        const Slot* slot = &SCHEDULE[current_slot];
        int slot_end_s = slot_end_mins(slot) * 60;
        int diff = slot_end_s - now_secs;
        if(diff < 0) diff = 0;
        int h = diff / 3600, m = (diff % 3600) / 60, s = diff % 60;

        // Find next break for "next break in" when in a class
        int next_break_slot = -1;
        if(!slot->is_break) {
            for(int i = current_slot + 1; i < SCHEDULE_LEN; i++) {
                if(SCHEDULE[i].is_break) {
                    next_break_slot = i;
                    break;
                }
            }
        }

        // ── Big countdown ────────────────────────────────────────────────────
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 14, AlignCenter, AlignTop, time_str);

        if(slot->is_break) {
            // ON BREAK — invert box to highlight
            canvas_draw_rbox(canvas, 2, 24, 124, 16, 4);
            canvas_set_color(canvas, ColorWhite);
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str_aligned(canvas, 64, 26, AlignCenter, AlignTop, "** ON BREAK **");
            canvas_set_color(canvas, ColorBlack);

            snprintf(line1, sizeof(line1), "Ends in  %02d:%02d", m, s);
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str_aligned(canvas, 64, 44, AlignCenter, AlignTop, line1);

            // Next class
            if(current_slot + 1 < SCHEDULE_LEN) {
                snprintf(line2, sizeof(line2), "Next class: %02d:%02d",
                         SCHEDULE[current_slot+1].start_h,
                         SCHEDULE[current_slot+1].start_m);
                canvas_draw_str_aligned(canvas, 64, 55, AlignCenter, AlignTop, line2);
            }

        } else {
            // IN CLASS
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str_aligned(canvas, 64, 24, AlignCenter, AlignTop, "IN CLASS");

            // Large MM:SS countdown
            snprintf(line1, sizeof(line1), "%02d:%02d", m, s);
            canvas_set_font(canvas, FontBigNumbers);
            canvas_draw_str_aligned(canvas, 64, 33, AlignCenter, AlignTop, line1);

            // "Next break at HH:MM" at bottom
            if(next_break_slot != -1) {
                snprintf(line2, sizeof(line2), "Break @ %02d:%02d",
                         SCHEDULE[next_break_slot].start_h,
                         SCHEDULE[next_break_slot].start_m);
                canvas_set_font(canvas, FontSecondary);
                canvas_draw_str_aligned(canvas, 64, 55, AlignCenter, AlignTop, line2);
            } else {
                canvas_set_font(canvas, FontSecondary);
                canvas_draw_str_aligned(canvas, 64, 55, AlignCenter, AlignTop, "Last class!");
            }
        }
    }

    // ── Bottom divider ───────────────────────────────────────────────────────
    canvas_draw_line(canvas, 0, 62, 127, 62);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignTop, "BACK to exit");
}

// ── Input callback ────────────────────────────────────────────────────────────

static void input_callback(InputEvent* event, void* ctx) {
    AppState* app = ctx;
    furi_message_queue_put(app->queue, event, 0);
}

// ── Main ──────────────────────────────────────────────────────────────────────

int32_t school_break_timer_app(void* p) {
    UNUSED(p);

    AppState* app = malloc(sizeof(AppState));
    app->queue    = furi_message_queue_alloc(8, sizeof(InputEvent));

    app->viewport = view_port_alloc();
    view_port_draw_callback_set(app->viewport, draw_callback, app);
    view_port_input_callback_set(app->viewport, input_callback, app);

    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->viewport, GuiLayerFullscreen);

    app->notif = furi_record_open(RECORD_NOTIFICATION);

    InputEvent event;
    bool running = true;

    while(running) {
        // Redraw every second via timeout
        if(furi_message_queue_get(app->queue, &event, 1000) == FuriStatusOk) {
            if(event.type == InputTypeShort && event.key == InputKeyBack) {
                running = false;
            }
        }
        view_port_update(app->viewport);
    }

    // Cleanup
    gui_remove_view_port(app->gui, app->viewport);
    view_port_free(app->viewport);
    furi_message_queue_free(app->queue);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    free(app);

    return 0;
}

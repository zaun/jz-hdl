#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"
#include <SDL3/SDL.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <cmath>

/* ---- JZW data model ---- */

struct Signal {
    int         id;
    std::string name;
    std::string scope;
    int         width;
    std::string type;
    std::string display_name; /* scope.name */
    bool        visible;      /* shown in waveform area */
    bool        expanded;     /* show individual bits (width > 1 only) */
};

struct ValueChange {
    int64_t     time;
    std::string value;
};

struct Annotation {
    int64_t     time;
    std::string type;       /* "mark", "alert", "trace" */
    int         signal_id;  /* -1 for global */
    std::string message;
    std::string color;
    int64_t     end_time;   /* 0 if not a range */
};

struct ClockInfo {
    std::string name;
    int64_t     period_ps;
    int64_t     phase_ps;
    int64_t     jitter_pp_ps;
    double      jitter_sigma_ps;
    double      drift_max_ppm;
    double      drift_actual_ppm;
    double      drifted_period_ps;
};

struct JZWFile {
    std::string                          filename;
    std::string                          module_name;
    int64_t                              sim_end_time = 0;
    int64_t                              display_start_time = 0; /* first trace=on time */
    int64_t                              tick_ps = 1;
    std::vector<Signal>                  signals;
    std::map<int, std::vector<ValueChange>> changes; /* signal_id -> changes */
    std::vector<Annotation>              annotations;
    std::vector<ClockInfo>               clocks;

    bool load(const char *path);
    const char *value_at(int signal_id, int64_t time) const;
};

bool JZWFile::load(const char *path)
{
    sqlite3 *db = nullptr;
    if (sqlite3_open_v2(path, &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
        fprintf(stderr, "Failed to open %s: %s\n", path, sqlite3_errmsg(db));
        sqlite3_close(db);
        return false;
    }

    filename = path;

    /* Read meta */
    {
        sqlite3_stmt *stmt;
        sqlite3_prepare_v2(db, "SELECT key, value FROM meta", -1, &stmt, nullptr);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char *k = (const char *)sqlite3_column_text(stmt, 0);
            const char *v = (const char *)sqlite3_column_text(stmt, 1);
            if (strcmp(k, "sim_end_time") == 0) sim_end_time = atoll(v);
            else if (strcmp(k, "tick_ps") == 0) tick_ps = atoll(v);
            else if (strcmp(k, "module_name") == 0) module_name = v;
        }
        sqlite3_finalize(stmt);
    }

    /* Read signals */
    {
        sqlite3_stmt *stmt;
        sqlite3_prepare_v2(db,
            "SELECT id, name, scope, width, type FROM signals ORDER BY id",
            -1, &stmt, nullptr);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Signal s;
            s.id    = sqlite3_column_int(stmt, 0);
            s.name  = (const char *)sqlite3_column_text(stmt, 1);
            s.scope = (const char *)sqlite3_column_text(stmt, 2);
            s.width = sqlite3_column_int(stmt, 3);
            s.type  = (const char *)sqlite3_column_text(stmt, 4);
            s.display_name = s.scope + "." + s.name;
            s.visible = true;
            s.expanded = false;
            signals.push_back(s);
        }
        sqlite3_finalize(stmt);
    }

    /* Read all value changes */
    {
        sqlite3_stmt *stmt;
        sqlite3_prepare_v2(db,
            "SELECT time, signal_id, value FROM changes ORDER BY signal_id, time",
            -1, &stmt, nullptr);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int64_t t  = sqlite3_column_int64(stmt, 0);
            int     id = sqlite3_column_int(stmt, 1);
            const char *v = (const char *)sqlite3_column_text(stmt, 2);
            changes[id].push_back({t, v});
        }
        sqlite3_finalize(stmt);
    }

    /* Read annotations */
    {
        sqlite3_stmt *stmt;
        int rc = sqlite3_prepare_v2(db,
            "SELECT time, type, signal_id, message, color, end_time "
            "FROM annotations ORDER BY time",
            -1, &stmt, nullptr);
        if (rc == SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                Annotation a;
                a.time      = sqlite3_column_int64(stmt, 0);
                a.type      = (const char *)sqlite3_column_text(stmt, 1);
                a.signal_id = sqlite3_column_type(stmt, 2) == SQLITE_NULL
                              ? -1 : sqlite3_column_int(stmt, 2);
                const char *msg = (const char *)sqlite3_column_text(stmt, 3);
                a.message   = msg ? msg : "";
                const char *col = (const char *)sqlite3_column_text(stmt, 4);
                a.color     = col ? col : "";
                a.end_time  = sqlite3_column_type(stmt, 5) == SQLITE_NULL
                              ? 0 : sqlite3_column_int64(stmt, 5);
                annotations.push_back(a);
            }
            sqlite3_finalize(stmt);
        }
    }

    /* Read clocks table (may not exist in older JZW files) */
    {
        sqlite3_stmt *stmt;
        int rc = sqlite3_prepare_v2(db,
            "SELECT name, period_ps, phase_ps, jitter_pp_ps, "
            "jitter_sigma_ps, drift_max_ppm, drift_actual_ppm, drifted_period_ps "
            "FROM clocks ORDER BY id",
            -1, &stmt, nullptr);
        if (rc == SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                ClockInfo c;
                const char *n = (const char *)sqlite3_column_text(stmt, 0);
                c.name             = n ? n : "";
                c.period_ps        = sqlite3_column_int64(stmt, 1);
                c.phase_ps         = sqlite3_column_int64(stmt, 2);
                c.jitter_pp_ps     = sqlite3_column_int64(stmt, 3);
                c.jitter_sigma_ps  = sqlite3_column_double(stmt, 4);
                c.drift_max_ppm    = sqlite3_column_double(stmt, 5);
                c.drift_actual_ppm = sqlite3_column_double(stmt, 6);
                c.drifted_period_ps = sqlite3_column_double(stmt, 7);
                clocks.push_back(c);
            }
            sqlite3_finalize(stmt);
        }
    }

    sqlite3_close(db);

    /* Compute display_start_time: if trace starts off, skip to first trace=on */
    {
        bool trace_off_at_start = false;
        for (const auto &a : annotations) {
            if (a.type == "trace") {
                if (a.time == 0 && a.message == "off") {
                    trace_off_at_start = true;
                } else if (trace_off_at_start && a.message == "on") {
                    display_start_time = a.time;
                    break;
                }
            }
        }
    }

    return true;
}

const char *JZWFile::value_at(int signal_id, int64_t time) const
{
    auto it = changes.find(signal_id);
    if (it == changes.end() || it->second.empty()) return "?";

    const auto &vc = it->second;
    /* Binary search for last change <= time */
    int lo = 0, hi = (int)vc.size() - 1, best = 0;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (vc[mid].time <= time) { best = mid; lo = mid + 1; }
        else hi = mid - 1;
    }
    return vc[best].value.c_str();
}

/* ---- Zoom levels ---- */

struct ZoomLevel {
    const char *label;
    double      ps_per_pixel; /* picoseconds per pixel */
};

static const ZoomLevel zoom_levels[] = {
    {"1ns",    1000.0   /  1.0},    /*    1 ps/px -> 1ns across 1000px */
    {"2ns",    2000.0   /  1000.0},
    {"5ns",    5000.0   /  1000.0},
    {"10ns",   10000.0  /  1000.0},
    {"20ns",   20000.0  /  1000.0},
    {"50ns",   50000.0  /  1000.0},
    {"100ns",  100000.0 /  1000.0},
    {"200ns",  200000.0 /  1000.0},
    {"500ns",  500000.0 /  1000.0},
    {"1us",    1e6      /  1000.0},
    {"2us",    2e6      /  1000.0},
    {"5us",    5e6      /  1000.0},
    {"10us",   1e7      /  1000.0},
    {"20us",   2e7      /  1000.0},
    {"50us",   5e7      /  1000.0},
    {"100us",  1e8      /  1000.0},
    {"200us",  2e8      /  1000.0},
    {"500us",  5e8      /  1000.0},
    {"1ms",    1e9      /  1000.0},
    {"2ms",    2e9      /  1000.0},
    {"5ms",    5e9      /  1000.0},
    {"10ms",   1e10     /  1000.0},
    {"20ms",   2e10     /  1000.0},
    {"50ms",   5e10     /  1000.0},
    {"100ms",  1e11     /  1000.0},
};
static const int num_zoom_levels = sizeof(zoom_levels) / sizeof(zoom_levels[0]);

/* ---- Helpers ---- */

static uint64_t binstr_to_u64(const char *s)
{
    uint64_t v = 0;
    while (*s) {
        v = (v << 1) | (*s == '1' ? 1 : 0);
        s++;
    }
    return v;
}

static void format_time(char *buf, size_t bufsz, int64_t ps)
{
    if (ps >= 1000000000LL)
        snprintf(buf, bufsz, "%.3f ms", ps / 1e9);
    else if (ps >= 1000000LL)
        snprintf(buf, bufsz, "%.3f us", ps / 1e6);
    else if (ps >= 1000LL)
        snprintf(buf, bufsz, "%.3f ns", ps / 1e3);
    else
        snprintf(buf, bufsz, "%lld ps", (long long)ps);
}

static void format_value(char *buf, size_t bufsz, const char *binval, int width)
{
    uint64_t val = binstr_to_u64(binval);
    if (width == 1)
        snprintf(buf, bufsz, "%llu", (unsigned long long)val);
    else if (width <= 4)
        snprintf(buf, bufsz, "%llX", (unsigned long long)val);
    else if (width <= 8)
        snprintf(buf, bufsz, "0x%02llX", (unsigned long long)val);
    else if (width <= 16)
        snprintf(buf, bufsz, "0x%04llX", (unsigned long long)val);
    else
        snprintf(buf, bufsz, "0x%llX", (unsigned long long)val);
}

/* ---- Icon drawing ---- */

static void draw_fit_icon(ImDrawList *dl, ImVec2 pos, float size, ImU32 col)
{
    /* Recreates fit-horizontal icon: |<--->| scaled to size x size */
    float s = size / 64.0f;
    float x = pos.x, y = pos.y;

    /* Left vertical bar */
    dl->AddLine(ImVec2(x + 1*s, y + 8*s),  ImVec2(x + 1*s, y + 56*s), col, 1.5f);
    /* Right vertical bar */
    dl->AddLine(ImVec2(x + 63*s, y + 8*s), ImVec2(x + 63*s, y + 56*s), col, 1.5f);
    /* Horizontal line */
    dl->AddLine(ImVec2(x + 9*s, y + 32*s), ImVec2(x + 55*s, y + 32*s), col, 1.5f);
    /* Left arrowhead */
    dl->AddLine(ImVec2(x + 16*s, y + 25*s), ImVec2(x + 9*s, y + 32*s), col, 1.5f);
    dl->AddLine(ImVec2(x + 9*s, y + 32*s),  ImVec2(x + 16*s, y + 39*s), col, 1.5f);
    /* Right arrowhead */
    dl->AddLine(ImVec2(x + 48*s, y + 25*s), ImVec2(x + 55*s, y + 32*s), col, 1.5f);
    dl->AddLine(ImVec2(x + 55*s, y + 32*s), ImVec2(x + 48*s, y + 39*s), col, 1.5f);
}

/* Magnifying glass icon with +/- inside the lens.
   Based on search-svgrepo-com.svg (viewBox 0 0 488.4 488.4):
   - Circle center ~(203, 203), radius ~179 (the lens)
   - Handle extends to bottom-right ~(476, 476) at 45 degrees */

static void draw_zoom_icon(ImDrawList *dl, ImVec2 pos, float size, ImU32 col, bool plus)
{
    float s = size / 64.0f;
    float x = pos.x, y = pos.y;

    /* Lens circle: center at (26, 26), radius 20 (mapped from SVG proportions) */
    float cx = x + 26.0f * s;
    float cy = y + 26.0f * s;
    float r  = 20.0f * s;
    dl->AddCircle(ImVec2(cx, cy), r, col, 0, 2.0f * s);

    /* Handle: from circle edge at 45 degrees to corner */
    float diag = 0.7071f; /* cos(45) */
    float hx0 = cx + r * diag;
    float hy0 = cy + r * diag;
    float hx1 = x + 58.0f * s;
    float hy1 = y + 58.0f * s;
    dl->AddLine(ImVec2(hx0, hy0), ImVec2(hx1, hy1), col, 3.0f * s);

    /* +/- inside the lens */
    float sign_len = 9.0f * s;
    /* Horizontal bar (both + and -) */
    dl->AddLine(ImVec2(cx - sign_len, cy), ImVec2(cx + sign_len, cy), col, 2.0f * s);
    if (plus) {
        /* Vertical bar (+ only) */
        dl->AddLine(ImVec2(cx, cy - sign_len), ImVec2(cx, cy + sign_len), col, 2.0f * s);
    }
}

/* Clock icon: circle with clock hands at ~10:10 position
   Based on clock-circle-svgrepo-com.svg (viewBox 0 0 24 24) */

static void draw_clock_icon(ImDrawList *dl, ImVec2 pos, float size, ImU32 col)
{
    float s = size / 24.0f;
    float cx = pos.x + 12.0f * s;
    float cy = pos.y + 12.0f * s;
    float r  = 10.5f * s;

    /* Circle */
    dl->AddCircle(ImVec2(cx, cy), r, col, 0, 1.8f * s);

    /* Hour hand (pointing ~10 o'clock: up-left) */
    dl->AddLine(ImVec2(cx, cy), ImVec2(cx - 3.0f * s, cy - 5.0f * s), col, 2.0f * s);

    /* Minute hand (pointing ~2 o'clock: up-right) */
    dl->AddLine(ImVec2(cx, cy), ImVec2(cx + 3.0f * s, cy - 6.5f * s), col, 1.5f * s);
}

/* Cursor tag icon: backspace-key shape with a number inside.
   Based on delete-svgrepo-com.svg (viewBox 0 0 489.425 489.425):
   - Rounded rect on the right with a pointed arrow on the left */

static void draw_cursor_tag_icon(ImDrawList *dl, ImVec2 pos, float size, ImU32 col,
                                  const char *label, ImFont *font)
{
    float s = size / 64.0f;
    float x = pos.x, y = pos.y;

    /* Key points mapped from SVG to 64x64 coordinate space:
       Arrow tip at left center, rounded rect on right */
    float arrow_x = x + 2.0f * s;    /* left arrow tip */
    float mid_y   = y + 32.0f * s;   /* vertical center */
    float left_x  = x + 18.0f * s;   /* where rect starts */
    float right_x = x + 62.0f * s;   /* right edge */
    float top_y   = y + 8.0f * s;
    float bot_y   = y + 56.0f * s;
    float r       = 8.0f * s;        /* corner rounding */

    /* Build the outline as a polyline:
       arrow tip -> top-left -> top-right (rounded) -> bottom-right (rounded) -> bottom-left -> arrow tip */
    ImVec2 pts[] = {
        {arrow_x, mid_y},             /* arrow tip */
        {left_x,  top_y},             /* top-left */
        {right_x - r, top_y},         /* top-right before curve */
        {right_x, top_y + r},         /* top-right after curve */
        {right_x, bot_y - r},         /* bottom-right before curve */
        {right_x - r, bot_y},         /* bottom-right after curve */
        {left_x,  bot_y},             /* bottom-left */
    };
    dl->AddPolyline(pts, 7, col, ImDrawFlags_Closed, 1.5f * s);

    /* Draw the number centered in the rect area */
    if (label && font) {
        float rect_cx = (left_x + right_x) * 0.5f;
        float rect_cy = mid_y;
        ImVec2 text_sz = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.0f, label);
        dl->AddText(font, font->FontSize,
                    ImVec2(rect_cx - text_sz.x * 0.5f, rect_cy - text_sz.y * 0.5f),
                    col, label);
    }
}

/* ---- Annotation color mapping ---- */

static ImU32 annotation_color(const std::string &name)
{
    if (name == "RED")    return IM_COL32(255, 80, 80, 255);
    if (name == "ORANGE") return IM_COL32(255, 160, 50, 255);
    if (name == "YELLOW") return IM_COL32(255, 255, 80, 255);
    if (name == "GREEN")  return IM_COL32(80, 220, 80, 255);
    if (name == "BLUE")   return IM_COL32(80, 140, 255, 255);
    if (name == "PURPLE") return IM_COL32(180, 100, 255, 255);
    if (name == "CYAN")   return IM_COL32(80, 220, 220, 255);
    if (name == "WHITE")  return IM_COL32(220, 220, 220, 255);
    return IM_COL32(180, 180, 180, 255); /* default gray */
}

/* ---- Drawing waveforms ---- */

static ImU32 signal_color(const Signal &sig)
{
    if (sig.type == "clock") return IM_COL32(100, 200, 255, 255);
    if (sig.type == "tap")   return IM_COL32(255, 200, 75, 255);
    return IM_COL32(0, 200, 80, 255);
}

static void draw_waveform(ImDrawList *dl, const Signal &sig,
                           const std::vector<ValueChange> &vc,
                           float x0, float y0, float w, float h,
                           double ps_per_px, int64_t scroll_ps)
{
    if (vc.empty()) return;

    int64_t t_start = scroll_ps;
    int64_t t_end   = scroll_ps + (int64_t)(w * ps_per_px);

    ImU32 col_wave = signal_color(sig);
    ImU32 col_text  = IM_COL32(200, 200, 200, 255);

    float mid_y = y0 + h * 0.5f;
    float hi_y  = y0 + 2.0f;
    float lo_y  = y0 + h - 2.0f;

    /* Binary search for first change at or before t_start */
    auto find_start = [&]() -> int {
        int lo = 0, hi = (int)vc.size() - 1, best = 0;
        while (lo <= hi) {
            int mid = (lo + hi) / 2;
            if (vc[mid].time <= t_start) { best = mid; lo = mid + 1; }
            else hi = mid - 1;
        }
        return best;
    };

    /* Minimum time span per pixel — used for decimation */
    int64_t ps_per_2px = (int64_t)(ps_per_px * 2.0);

    if (sig.width == 1) {
        /* Digital waveform: step transitions */
        int start_idx = find_start();

        for (int i = start_idx; i < (int)vc.size(); i++) {
            if (vc[i].time > t_end) break;

            int64_t t0 = vc[i].time;
            int64_t t1 = (i + 1 < (int)vc.size()) ? vc[i + 1].time : t_end;

            /* Decimation: if many transitions fit in < 2 pixels, draw a filled
               bar to represent activity and skip ahead */
            if (i + 2 < (int)vc.size() && (vc[i + 2].time - t0) < ps_per_2px) {
                /* Find the extent of this dense cluster */
                int64_t cluster_start = t0;
                int j = i;
                while (j + 1 < (int)vc.size() &&
                       (vc[j + 1].time - vc[j].time) < ps_per_2px) {
                    j++;
                    if (vc[j].time > t_end) break;
                }
                int64_t cluster_end = vc[j].time;
                int64_t next_t = (j + 1 < (int)vc.size()) ? vc[j + 1].time : t_end;
                if (cluster_end < t_start) cluster_start = t_start;

                float cx1 = x0 + (float)((std::max(cluster_start, t_start) - scroll_ps) / ps_per_px);
                float cx2 = x0 + (float)((std::min(cluster_end, t_end) - scroll_ps) / ps_per_px);
                if (cx2 - cx1 < 1.0f) cx2 = cx1 + 1.0f;

                /* Draw filled activity bar */
                dl->AddRectFilled(ImVec2(cx1, hi_y), ImVec2(cx2, lo_y),
                                  (col_wave & 0x00FFFFFF) | 0x60000000);
                dl->AddLine(ImVec2(cx1, hi_y), ImVec2(cx2, hi_y), col_wave, 1.0f);
                dl->AddLine(ImVec2(cx1, lo_y), ImVec2(cx2, lo_y), col_wave, 1.0f);

                /* Draw the final stable value after the cluster */
                bool end_val = (vc[j].value == "1");
                float end_x = cx2;
                float end_x2 = x0 + (float)((std::min(next_t, t_end) - scroll_ps) / ps_per_px);
                if (end_x2 > end_x) {
                    float ey = end_val ? hi_y : lo_y;
                    dl->AddLine(ImVec2(end_x, ey), ImVec2(end_x2, ey), col_wave, 1.5f);
                }

                i = j; /* skip ahead */
                continue;
            }

            if (t1 > t_end) t1 = t_end;
            if (t0 < t_start) t0 = t_start;

            float x1 = x0 + (float)((t0 - scroll_ps) / ps_per_px);
            float x2 = x0 + (float)((t1 - scroll_ps) / ps_per_px);
            bool val = (vc[i].value == "1");

            float y_val = val ? hi_y : lo_y;
            dl->AddLine(ImVec2(x1, y_val), ImVec2(x2, y_val), col_wave, 1.5f);

            /* Transition edge */
            if (i + 1 < (int)vc.size() && vc[i + 1].time <= t_end) {
                bool next_val = (vc[i + 1].value == "1");
                float ny = next_val ? hi_y : lo_y;
                dl->AddLine(ImVec2(x2, y_val), ImVec2(x2, ny), col_wave, 1.5f);
            }
        }
    } else {
        /* Multi-bit: bus-style with hex value labels */
        int start_idx = find_start();

        for (int i = start_idx; i < (int)vc.size(); i++) {
            if (vc[i].time > t_end) break;

            int64_t t0 = vc[i].time;
            int64_t t1 = (i + 1 < (int)vc.size()) ? vc[i + 1].time : t_end;

            /* Decimation: skip sub-pixel changes */
            if (i + 2 < (int)vc.size() && (vc[i + 2].time - t0) < ps_per_2px) {
                int j = i;
                while (j + 1 < (int)vc.size() &&
                       (vc[j + 1].time - vc[j].time) < ps_per_2px) {
                    j++;
                    if (vc[j].time > t_end) break;
                }
                int64_t cs = std::max(t0, t_start);
                int64_t ce = std::min(vc[j].time, t_end);
                float cx1 = x0 + (float)((cs - scroll_ps) / ps_per_px);
                float cx2 = x0 + (float)((ce - scroll_ps) / ps_per_px);
                if (cx2 - cx1 < 1.0f) cx2 = cx1 + 1.0f;
                dl->AddRectFilled(ImVec2(cx1, hi_y), ImVec2(cx2, lo_y),
                                  (col_wave & 0x00FFFFFF) | 0x60000000);
                dl->AddLine(ImVec2(cx1, hi_y), ImVec2(cx2, hi_y), col_wave, 1.0f);
                dl->AddLine(ImVec2(cx1, lo_y), ImVec2(cx2, lo_y), col_wave, 1.0f);

                /* Draw stable value after cluster as a bus segment */
                int64_t next_t = (j + 1 < (int)vc.size()) ? vc[j + 1].time : t_end;
                if (next_t > ce && ce < t_end) {
                    float ex1 = cx2;
                    float ex2 = x0 + (float)((std::min(next_t, t_end) - scroll_ps) / ps_per_px);
                    if (ex2 > ex1 + 1.0f) {
                        float dw = std::min(4.0f, (ex2 - ex1) * 0.3f);
                        ImVec2 pts[6] = {
                            {ex1 + dw, hi_y}, {ex2 - dw, hi_y}, {ex2, mid_y},
                            {ex2 - dw, lo_y}, {ex1 + dw, lo_y}, {ex1, mid_y}
                        };
                        dl->AddPolyline(pts, 6, col_wave, ImDrawFlags_Closed, 1.5f);

                        uint64_t val = binstr_to_u64(vc[j].value.c_str());
                        char label[32];
                        if (sig.width <= 4)
                            snprintf(label, sizeof(label), "%llX", (unsigned long long)val);
                        else if (sig.width <= 8)
                            snprintf(label, sizeof(label), "0x%02llX", (unsigned long long)val);
                        else if (sig.width <= 16)
                            snprintf(label, sizeof(label), "0x%04llX", (unsigned long long)val);
                        else
                            snprintf(label, sizeof(label), "0x%llX", (unsigned long long)val);
                        float text_w = ImGui::CalcTextSize(label).x;
                        float avail = ex2 - ex1 - dw * 2 - 4;
                        if (avail > text_w) {
                            dl->AddText(ImVec2(ex1 + dw + 2, mid_y - 6), col_text, label);
                        }
                    }
                }

                i = j;
                continue;
            }

            if (t1 > t_end) t1 = t_end;
            if (t0 < t_start) t0 = t_start;

            float x1 = x0 + (float)((t0 - scroll_ps) / ps_per_px);
            float x2 = x0 + (float)((t1 - scroll_ps) / ps_per_px);

            /* Diamond transition markers */
            float dw = std::min(4.0f, (x2 - x1) * 0.3f);
            ImVec2 pts[6] = {
                {x1 + dw, hi_y}, {x2 - dw, hi_y}, {x2, mid_y},
                {x2 - dw, lo_y}, {x1 + dw, lo_y}, {x1, mid_y}
            };
            dl->AddPolyline(pts, 6, col_wave, ImDrawFlags_Closed, 1.5f);

            /* Hex value label */
            uint64_t val = binstr_to_u64(vc[i].value.c_str());
            char label[32];
            if (sig.width <= 4)
                snprintf(label, sizeof(label), "%llX", (unsigned long long)val);
            else if (sig.width <= 8)
                snprintf(label, sizeof(label), "0x%02llX", (unsigned long long)val);
            else if (sig.width <= 16)
                snprintf(label, sizeof(label), "0x%04llX", (unsigned long long)val);
            else
                snprintf(label, sizeof(label), "0x%llX", (unsigned long long)val);

            float text_w = ImGui::CalcTextSize(label).x;
            float avail = x2 - x1 - dw * 2 - 4;
            if (avail > text_w) {
                float tx = x1 + dw + 2;
                dl->AddText(ImVec2(tx, mid_y - 6), col_text, label);
            }
        }
    }
}

/* ---- Main ---- */

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "Usage: jz-viewer <file.jzw>\n");
        return 1;
    }

    JZWFile jzw;
    if (!jzw.load(argv[1])) {
        return 1;
    }

    /* Extract display filename */
    const char *display_name = strrchr(argv[1], '/');
    display_name = display_name ? display_name + 1 : argv[1];

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow(
        "JZ-HDL Waveform Viewer",
        1280, 720,
        SDL_WINDOW_RESIZABLE);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, NULL);
    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        return 1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();
    ImGui::GetStyle().FrameRounding = 3.0f;

    /* Load default font at two sizes: 13px (default) and 15px (toolbar) */
    ImFont *font_default = io.Fonts->AddFontDefault();
    ImFontConfig cfg;
    cfg.SizePixels = 16.0f;
    ImFont *font_toolbar = io.Fonts->AddFontDefault(&cfg);

    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    /* State */
    int    zoom_idx   = 9;  /* start at 1us */
    float  sidebar_w  = 220.0f;
    int64_t scroll_ps = jzw.display_start_time;
    float  scroll_y   = 0.0f;
    float  row_height = 30.0f;
    float  toolbar_h  = 44.0f; /* 36px buttons + 4px top/bottom padding */
    bool   dragging_sidebar = false;
    float  sidebar_min_w = 120.0f;
    float  sidebar_max_w = 500.0f;
    float  splitter_w = 4.0f;

    /* Cursor state */
    int64_t cursor_ps = -1;       /* C1 time (-1 = not placed) */
    int64_t cursor2_ps = -1;      /* C2 time */
    int64_t cursor3_ps = -1;      /* C3 time */
    int64_t cursor4_ps = -1;      /* C4 time */
    bool    cursor_visible = false;
    bool    cursor2_visible = false;
    bool    cursor3_visible = false;
    bool    cursor4_visible = false;
    int     active_cursor = 0;    /* 0=none, 1=C1, 2=C2, 3=C3, 4=C4 */

    /* Drag-reorder state */
    int drag_src_idx = -1;        /* index in jzw.signals being dragged */
    bool dragging_signal = false;

    /* Clock dialog state */
    bool show_clock_dialog = false;

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT)
                running = false;
            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                event.window.windowID == SDL_GetWindowID(window))
                running = false;
        }

        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        const ImGuiViewport *viewport = ImGui::GetMainViewport();
        ImVec2 wp = viewport->WorkPos;
        ImVec2 ws = viewport->WorkSize;

        /* ---- Toolbar ---- */
        ImGui::PushFont(font_toolbar);
        float toolbar_pad_y = 4.0f;
        float toolbar_btn_sz = 36.0f;
        ImGui::SetNextWindowPos(wp);
        ImGui::SetNextWindowSize(ImVec2(ws.x, toolbar_h));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(ImGui::GetStyle().WindowPadding.x, toolbar_pad_y));
        ImGui::Begin("##Toolbar", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                     ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoBringToFrontOnFocus);

        /* Vertically center text within the 42px button row */
        float text_vcenter_y = toolbar_pad_y + (toolbar_btn_sz - ImGui::GetTextLineHeight()) * 0.5f;

        /* Place an invisible dummy to establish the 42px line height without scrolling */
        ImGui::Dummy(ImVec2(0, toolbar_btn_sz));
        ImGui::SameLine(0, 0);
        ImGui::SetCursorPosX(ImGui::GetStyle().WindowPadding.x);

        /* Stacked filename and module name */
        {
            ImDrawList *dl = ImGui::GetWindowDrawList();
            ImVec2 wpos = ImGui::GetWindowPos();
            float x0 = wpos.x + ImGui::GetCursorPosX();
            float line_h = ImGui::GetTextLineHeight();
            float gap = 1.0f;
            float stack_h = line_h * 2 + gap;
            float y_top = wpos.y + toolbar_pad_y + (toolbar_btn_sz - stack_h) * 0.5f;

            /* Filename (top) */
            dl->AddText(ImVec2(x0, y_top), ImGui::GetColorU32(ImGuiCol_Text),
                        display_name);
            float name_w = ImGui::CalcTextSize(display_name).x;

            /* Module name (bottom, dimmed) */
            float mod_w = 0.0f;
            if (!jzw.module_name.empty()) {
                char mod_buf[256];
                snprintf(mod_buf, sizeof(mod_buf), "%s", jzw.module_name.c_str());
                dl->AddText(ImVec2(x0, y_top + line_h + gap),
                            ImGui::GetColorU32(ImGuiCol_TextDisabled), mod_buf);
                mod_w = ImGui::CalcTextSize(mod_buf).x;
            }

            float text_block_w = (name_w > mod_w) ? name_w : mod_w;

            /* Vertical separator line */
            float sep_x = x0 + text_block_w + 8.0f;
            float sep_y0 = wpos.y + toolbar_pad_y + 2.0f;
            float sep_y1 = wpos.y + toolbar_pad_y + toolbar_btn_sz - 2.0f;
            dl->AddLine(ImVec2(sep_x, sep_y0), ImVec2(sep_x, sep_y1),
                        ImGui::GetColorU32(ImGuiCol_Separator), 1.0f);

            /* Advance cursor past the stacked text + separator */
            ImGui::Dummy(ImVec2(text_block_w + 12.0f, 0));
            ImGui::SameLine(0, 4.0f);
        }
        ImGui::SetCursorPosY(text_vcenter_y);

        /* Cursor info in toolbar (stacked pairs) */
        {
            ImDrawList *dl = ImGui::GetWindowDrawList();
            ImVec2 wpos = ImGui::GetWindowPos();
            float line_h = ImGui::GetTextLineHeight();
            float gap = 1.0f;
            float stack_h = line_h * 2 + gap;
            float y_top = wpos.y + toolbar_pad_y + (toolbar_btn_sz - stack_h) * 0.5f;
            float y_bot = y_top + line_h + gap;
            char tbuf[64], tbuf2[64];

            bool c1_on = cursor_visible && cursor_ps >= 0;
            bool c2_on = cursor2_visible && cursor2_ps >= 0;
            bool c3_on = cursor3_visible && cursor3_ps >= 0;
            bool c4_on = cursor4_visible && cursor4_ps >= 0;

            /* C1/C2 stacked pair */
            if (c1_on || c2_on) {
                ImGui::SameLine();
                float x0 = wpos.x + ImGui::GetCursorPosX();
                float block_w = 0.0f;

                if (c1_on) {
                    format_time(tbuf, sizeof(tbuf), cursor_ps);
                    char label[80]; snprintf(label, sizeof(label), "C1: %s", tbuf);
                    dl->AddText(ImVec2(x0, y_top),
                                ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 0.4f, 1.0f)), label);
                    float w = ImGui::CalcTextSize(label).x;
                    if (w > block_w) block_w = w;
                }
                if (c2_on) {
                    format_time(tbuf, sizeof(tbuf), cursor2_ps);
                    char label[80]; snprintf(label, sizeof(label), "C2: %s", tbuf);
                    dl->AddText(ImVec2(x0, y_bot),
                                ImGui::ColorConvertFloat4ToU32(ImVec4(0.4f, 1.0f, 1.0f, 1.0f)), label);
                    float w = ImGui::CalcTextSize(label).x;
                    if (w > block_w) block_w = w;
                }

                ImGui::Dummy(ImVec2(block_w, 0));

                /* Hover tooltip with C1-C2 delta */
                if (c1_on && c2_on) {
                    ImVec2 block_min(x0, y_top);
                    ImVec2 block_max(x0 + block_w, y_bot + line_h);
                    if (ImGui::IsMouseHoveringRect(block_min, block_max)) {
                        int64_t delta = cursor2_ps - cursor_ps;
                        if (delta < 0) delta = -delta;
                        format_time(tbuf, sizeof(tbuf), delta);
                        ImGui::SetTooltip("C1-C2 dt: %s", tbuf);
                    }
                }
            }

            /* Separator between pairs */
            if ((c1_on || c2_on) && (c3_on || c4_on)) {
                ImGui::SameLine();
                float sx = wpos.x + ImGui::GetCursorPosX();
                float sep_y0 = wpos.y + toolbar_pad_y + 2.0f;
                float sep_y1 = wpos.y + toolbar_pad_y + toolbar_btn_sz - 2.0f;
                dl->AddLine(ImVec2(sx, sep_y0), ImVec2(sx, sep_y1),
                            ImGui::GetColorU32(ImGuiCol_Separator), 1.0f);
                ImGui::Dummy(ImVec2(4.0f, 0));
            }

            /* C3/C4 stacked pair */
            if (c3_on || c4_on) {
                ImGui::SameLine();
                float x0 = wpos.x + ImGui::GetCursorPosX();
                float block_w = 0.0f;

                if (c3_on) {
                    format_time(tbuf, sizeof(tbuf), cursor3_ps);
                    char label[80]; snprintf(label, sizeof(label), "C3: %s", tbuf);
                    dl->AddText(ImVec2(x0, y_top),
                                ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 0.6f, 1.0f, 1.0f)), label);
                    float w = ImGui::CalcTextSize(label).x;
                    if (w > block_w) block_w = w;
                }
                if (c4_on) {
                    format_time(tbuf, sizeof(tbuf), cursor4_ps);
                    char label[80]; snprintf(label, sizeof(label), "C4: %s", tbuf);
                    dl->AddText(ImVec2(x0, y_bot),
                                ImGui::ColorConvertFloat4ToU32(ImVec4(0.6f, 1.0f, 0.6f, 1.0f)), label);
                    float w = ImGui::CalcTextSize(label).x;
                    if (w > block_w) block_w = w;
                }

                ImGui::Dummy(ImVec2(block_w, 0));

                /* Hover tooltip with C3-C4 delta */
                if (c3_on && c4_on) {
                    ImVec2 block_min(x0, y_top);
                    ImVec2 block_max(x0 + block_w, y_bot + line_h);
                    if (ImGui::IsMouseHoveringRect(block_min, block_max)) {
                        int64_t delta = cursor4_ps - cursor3_ps;
                        if (delta < 0) delta = -delta;
                        format_time(tbuf, sizeof(tbuf), delta);
                        ImGui::SetTooltip("C3-C4 dt: %s", tbuf);
                    }
                }
            }
        }

        /* Zoom control on the right side */
        {
            const char *zoom_label = zoom_levels[zoom_idx].label;
            /* Fixed-width label area: "Zoom: 100ms/div" is the widest */
            float label_w = ImGui::CalcTextSize("100ms").x + 16.0f;
            float zoom_ctrl_w = toolbar_btn_sz * 2 + label_w;
            float total_w = zoom_ctrl_w + ImGui::GetStyle().ItemSpacing.x * 5 + toolbar_btn_sz * 7;
            ImGui::SameLine(ws.x - total_w - 4.0f);
            ImGui::SetCursorPosY(toolbar_pad_y);
            float zoom_icon_sz = toolbar_btn_sz - 10.0f;
            float tag_icon_sz = toolbar_btn_sz - 10.0f;

            /* Cursor 1 toggle button */
            {
                bool c1_on = (active_cursor == 1);
                ImVec2 c1_pos = ImGui::GetCursorScreenPos();

                if (c1_on) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetColorU32(ImGuiCol_ButtonActive));
                }
                if (ImGui::Button("##C1", ImVec2(toolbar_btn_sz, toolbar_btn_sz))) {
                    active_cursor = c1_on ? 0 : 1;
                }
                if (c1_on) {
                    ImGui::PopStyleColor();
                }
                ImU32 c1_col = (ImGui::IsItemHovered() || c1_on)
                    ? IM_COL32(255, 255, 100, 255)
                    : IM_COL32(180, 180, 180, 255);
                draw_cursor_tag_icon(ImGui::GetWindowDrawList(),
                    ImVec2(c1_pos.x + (toolbar_btn_sz - tag_icon_sz) * 0.5f,
                           c1_pos.y + (toolbar_btn_sz - tag_icon_sz) * 0.5f),
                    tag_icon_sz, c1_col, "1", font_toolbar);
            }

            /* Cursor 2 toggle button */
            ImGui::SameLine(0, 0);
            ImGui::SetCursorPosY(toolbar_pad_y);
            {
                bool c2_on = (active_cursor == 2);
                ImVec2 c2_pos = ImGui::GetCursorScreenPos();

                if (c2_on) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetColorU32(ImGuiCol_ButtonActive));
                }
                if (ImGui::Button("##C2", ImVec2(toolbar_btn_sz, toolbar_btn_sz))) {
                    active_cursor = c2_on ? 0 : 2;
                }
                if (c2_on) {
                    ImGui::PopStyleColor();
                }
                ImU32 c2_col = (ImGui::IsItemHovered() || c2_on)
                    ? IM_COL32(100, 255, 255, 255)
                    : IM_COL32(180, 180, 180, 255);
                draw_cursor_tag_icon(ImGui::GetWindowDrawList(),
                    ImVec2(c2_pos.x + (toolbar_btn_sz - tag_icon_sz) * 0.5f,
                           c2_pos.y + (toolbar_btn_sz - tag_icon_sz) * 0.5f),
                    tag_icon_sz, c2_col, "2", font_toolbar);
            }

            /* Cursor 3 toggle button */
            ImGui::SameLine();
            ImGui::SetCursorPosY(toolbar_pad_y);
            {
                bool c3_on = (active_cursor == 3);
                ImVec2 c3_pos = ImGui::GetCursorScreenPos();

                if (c3_on) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetColorU32(ImGuiCol_ButtonActive));
                }
                if (ImGui::Button("##C3", ImVec2(toolbar_btn_sz, toolbar_btn_sz))) {
                    active_cursor = c3_on ? 0 : 3;
                }
                if (c3_on) {
                    ImGui::PopStyleColor();
                }
                ImU32 c3_col = (ImGui::IsItemHovered() || c3_on)
                    ? IM_COL32(255, 150, 255, 255)
                    : IM_COL32(180, 180, 180, 255);
                draw_cursor_tag_icon(ImGui::GetWindowDrawList(),
                    ImVec2(c3_pos.x + (toolbar_btn_sz - tag_icon_sz) * 0.5f,
                           c3_pos.y + (toolbar_btn_sz - tag_icon_sz) * 0.5f),
                    tag_icon_sz, c3_col, "3", font_toolbar);
            }

            /* Cursor 4 toggle button */
            ImGui::SameLine(0, 0);
            ImGui::SetCursorPosY(toolbar_pad_y);
            {
                bool c4_on = (active_cursor == 4);
                ImVec2 c4_pos = ImGui::GetCursorScreenPos();

                if (c4_on) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetColorU32(ImGuiCol_ButtonActive));
                }
                if (ImGui::Button("##C4", ImVec2(toolbar_btn_sz, toolbar_btn_sz))) {
                    active_cursor = c4_on ? 0 : 4;
                }
                if (c4_on) {
                    ImGui::PopStyleColor();
                }
                ImU32 c4_col = (ImGui::IsItemHovered() || c4_on)
                    ? IM_COL32(150, 255, 150, 255)
                    : IM_COL32(180, 180, 180, 255);
                draw_cursor_tag_icon(ImGui::GetWindowDrawList(),
                    ImVec2(c4_pos.x + (toolbar_btn_sz - tag_icon_sz) * 0.5f,
                           c4_pos.y + (toolbar_btn_sz - tag_icon_sz) * 0.5f),
                    tag_icon_sz, c4_col, "4", font_toolbar);
            }

            /* Clear all cursors button */
            ImGui::SameLine();
            ImGui::SetCursorPosY(toolbar_pad_y);
            {
                ImVec2 cx_pos = ImGui::GetCursorScreenPos();
                if (ImGui::Button("##CX", ImVec2(toolbar_btn_sz, toolbar_btn_sz))) {
                    active_cursor = 0;
                    cursor_visible = false;  cursor_ps = -1;
                    cursor2_visible = false; cursor2_ps = -1;
                    cursor3_visible = false; cursor3_ps = -1;
                    cursor4_visible = false; cursor4_ps = -1;
                }
                ImU32 cx_col = ImGui::IsItemHovered()
                    ? IM_COL32(255, 100, 100, 255)
                    : IM_COL32(180, 180, 180, 255);
                draw_cursor_tag_icon(ImGui::GetWindowDrawList(),
                    ImVec2(cx_pos.x + (toolbar_btn_sz - tag_icon_sz) * 0.5f,
                           cx_pos.y + (toolbar_btn_sz - tag_icon_sz) * 0.5f),
                    tag_icon_sz, cx_col, "X", font_toolbar);
            }

            ImGui::SameLine();
            ImGui::SetCursorPosY(toolbar_pad_y);

            /* Draw zoom control as one seamless widget */
            {
                float ctrl_w = toolbar_btn_sz * 2 + label_w;
                ImVec2 ctrl_pos = ImGui::GetCursorScreenPos();
                ImDrawList *tdl = ImGui::GetWindowDrawList();
                float rounding = ImGui::GetStyle().FrameRounding;

                /* Background fill for entire control */
                ImU32 bg_col = ImGui::GetColorU32(ImGuiCol_Button);
                ImU32 border_col = ImGui::GetColorU32(ImGuiCol_Border);
                tdl->AddRectFilled(ImVec2(ctrl_pos.x, ctrl_pos.y),
                                   ImVec2(ctrl_pos.x + ctrl_w, ctrl_pos.y + toolbar_btn_sz),
                                   bg_col, rounding);
                tdl->AddRect(ImVec2(ctrl_pos.x, ctrl_pos.y),
                             ImVec2(ctrl_pos.x + ctrl_w, ctrl_pos.y + toolbar_btn_sz),
                             border_col, rounding, 0, 1.0f);

                /* Vertical separators between buttons and label */
                float sep1_x = ctrl_pos.x + toolbar_btn_sz;
                float sep2_x = ctrl_pos.x + toolbar_btn_sz + label_w;
                tdl->AddLine(ImVec2(sep1_x, ctrl_pos.y + 1),
                             ImVec2(sep1_x, ctrl_pos.y + toolbar_btn_sz - 1),
                             border_col, 1.0f);
                tdl->AddLine(ImVec2(sep2_x, ctrl_pos.y + 1),
                             ImVec2(sep2_x, ctrl_pos.y + toolbar_btn_sz - 1),
                             border_col, 1.0f);

                /* Zoom-out button (left) */
                ImVec2 zout_min = ctrl_pos;
                ImVec2 zout_max(ctrl_pos.x + toolbar_btn_sz, ctrl_pos.y + toolbar_btn_sz);
                ImGui::SetCursorScreenPos(zout_min);
                if (ImGui::InvisibleButton("##zout", ImVec2(toolbar_btn_sz, toolbar_btn_sz))) {
                    if (zoom_idx < num_zoom_levels - 1) zoom_idx++;
                }
                if (ImGui::IsItemHovered()) {
                    tdl->AddRectFilled(zout_min, zout_max,
                        ImGui::GetColorU32(ImGuiCol_ButtonHovered), rounding,
                        ImDrawFlags_RoundCornersLeft);
                }
                ImU32 zout_col = ImGui::IsItemHovered()
                    ? IM_COL32(255, 255, 255, 255) : IM_COL32(180, 180, 180, 255);
                draw_zoom_icon(tdl,
                    ImVec2(zout_min.x + (toolbar_btn_sz - zoom_icon_sz) * 0.5f,
                           zout_min.y + (toolbar_btn_sz - zoom_icon_sz) * 0.5f),
                    zoom_icon_sz, zout_col, false);

                /* Zoom label (center) */
                float text_actual = ImGui::CalcTextSize(zoom_label).x;
                float text_x = ctrl_pos.x + toolbar_btn_sz + (label_w - text_actual) * 0.5f;
                float text_y = ctrl_pos.y + (toolbar_btn_sz - ImGui::GetTextLineHeight()) * 0.5f;
                tdl->AddText(ImVec2(text_x, text_y),
                    ImGui::GetColorU32(ImGuiCol_Text), zoom_label);

                /* Zoom-in button (right) */
                ImVec2 zin_min(ctrl_pos.x + toolbar_btn_sz + label_w, ctrl_pos.y);
                ImVec2 zin_max(ctrl_pos.x + ctrl_w, ctrl_pos.y + toolbar_btn_sz);
                ImGui::SetCursorScreenPos(zin_min);
                if (ImGui::InvisibleButton("##zin", ImVec2(toolbar_btn_sz, toolbar_btn_sz))) {
                    if (zoom_idx > 0) zoom_idx--;
                }
                if (ImGui::IsItemHovered()) {
                    tdl->AddRectFilled(zin_min, zin_max,
                        ImGui::GetColorU32(ImGuiCol_ButtonHovered), rounding,
                        ImDrawFlags_RoundCornersRight);
                }
                ImU32 zin_col = ImGui::IsItemHovered()
                    ? IM_COL32(255, 255, 255, 255) : IM_COL32(180, 180, 180, 255);
                draw_zoom_icon(tdl,
                    ImVec2(zin_min.x + (toolbar_btn_sz - zoom_icon_sz) * 0.5f,
                           zin_min.y + (toolbar_btn_sz - zoom_icon_sz) * 0.5f),
                    zoom_icon_sz, zin_col, true);

                /* Advance cursor past the whole control */
                ImGui::SetCursorScreenPos(ImVec2(ctrl_pos.x + ctrl_w, ctrl_pos.y));
                ImGui::Dummy(ImVec2(0, toolbar_btn_sz));
            }
            ImGui::SameLine();
            ImGui::SetCursorPosY(toolbar_pad_y);
            {
                ImVec2 fit_btn_pos = ImGui::GetCursorScreenPos();

                /* Use Button with empty label for consistent styling */
                if (ImGui::Button("##Fit", ImVec2(toolbar_btn_sz, toolbar_btn_sz))) {
                    float fit_w = ws.x - sidebar_w - 20.0f;
                    if (fit_w < 1.0f) fit_w = 1.0f;
                    int64_t display_dur = jzw.sim_end_time - jzw.display_start_time;
                    double needed_ps_per_px = (double)display_dur / (double)fit_w;
                    int best = num_zoom_levels - 1;
                    for (int z = 0; z < num_zoom_levels; z++) {
                        if (zoom_levels[z].ps_per_pixel >= needed_ps_per_px) {
                            best = z;
                            break;
                        }
                    }
                    zoom_idx = best;
                    scroll_ps = jzw.display_start_time;
                }
                /* Draw the fit icon centered in the 42x42 button */
                ImU32 icon_col = ImGui::IsItemHovered()
                    ? IM_COL32(255, 255, 255, 255)
                    : IM_COL32(180, 180, 180, 255);
                float icon_sz = toolbar_btn_sz - 10.0f; /* 26px icon in 36px button */
                ImVec2 icon_pos(fit_btn_pos.x + (toolbar_btn_sz - icon_sz) * 0.5f,
                                fit_btn_pos.y + (toolbar_btn_sz - icon_sz) * 0.5f);
                draw_fit_icon(ImGui::GetWindowDrawList(), icon_pos, icon_sz, icon_col);
            }

            /* Clock info button */
            ImGui::SameLine();
            ImGui::SetCursorPosY(toolbar_pad_y);
            {
                ImVec2 clk_btn_pos = ImGui::GetCursorScreenPos();
                if (ImGui::Button("##Clocks", ImVec2(toolbar_btn_sz, toolbar_btn_sz))) {
                    show_clock_dialog = !show_clock_dialog;
                }
                ImU32 clk_col = ImGui::IsItemHovered()
                    ? IM_COL32(255, 255, 255, 255)
                    : IM_COL32(180, 180, 180, 255);
                float clk_icon_sz = toolbar_btn_sz - 10.0f;
                ImVec2 clk_icon_pos(clk_btn_pos.x + (toolbar_btn_sz - clk_icon_sz) * 0.5f,
                                    clk_btn_pos.y + (toolbar_btn_sz - clk_icon_sz) * 0.5f);
                draw_clock_icon(ImGui::GetWindowDrawList(), clk_icon_pos, clk_icon_sz, clk_col);
            }
        }

        ImGui::End();
        ImGui::PopStyleVar(); /* WindowPadding */
        ImGui::PopFont();

        /* ---- Layout: toolbar | content | gutter ---- */
        float gutter_h = row_height;
        float content_y = wp.y + toolbar_h;
        float content_h = ws.y - toolbar_h - gutter_h;

        /* ---- Sidebar (signal names) ---- */

        ImGui::SetNextWindowPos(ImVec2(wp.x, content_y));
        ImGui::SetNextWindowSize(ImVec2(sidebar_w, content_h));
        ImGui::Begin("##Sidebar", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoBringToFrontOnFocus |
                     ImGuiWindowFlags_NoScrollWithMouse);

        /* Reserve space for the ruler row to match the waveform area */
        float sidebar_start_y = ImGui::GetCursorPosY() + 20.0f; /* 20 = ruler_h */

        /* Column widths — account for scrollbar eating into right side */
        float expand_col_w = 20.0f;
        float width_col_w = 40.0f;
        float sb_scroll_w = ImGui::GetStyle().ScrollbarSize;
        float sidebar_avail_w = sidebar_w - sb_scroll_w;
        float name_col_w = sidebar_avail_w - width_col_w - expand_col_w -
                           ImGui::GetStyle().WindowPadding.x * 2 - 8.0f;
        (void)name_col_w;

        int row_idx = 0;
        for (size_t i = 0; i < jzw.signals.size(); i++) {
            Signal &sig = jzw.signals[i];
            ImGui::PushID(sig.id);

            /* Color-code by type */
            ImU32 col = signal_color(sig);
            ImVec4 col4 = ImGui::ColorConvertU32ToFloat4(col);

            /* Position at exact row to match waveform area */
            float row_y = sidebar_start_y + row_idx * row_height;
            float text_y = row_y + (row_height - ImGui::GetTextLineHeight()) * 0.5f;

            /* Drag-reorder: detect drag start on signal name */
            ImVec2 row_min(ImGui::GetStyle().WindowPadding.x,
                           row_y - scroll_y + ImGui::GetWindowPos().y - ImGui::GetWindowPos().y);

            /* Expand button column */
            ImGui::SetCursorPosY(text_y);
            ImGui::SetCursorPosX(ImGui::GetStyle().WindowPadding.x);
            if (sig.width > 1) {
                char btn_label[8];
                snprintf(btn_label, sizeof(btn_label), "%s", sig.expanded ? "v" : ">");
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_Text, col4);
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
                if (ImGui::SmallButton(btn_label)) {
                    sig.expanded = !sig.expanded;
                }
                ImGui::PopStyleVar();
                ImGui::PopStyleColor(3);
            }

            /* Signal name column */
            ImGui::SetCursorPosY(text_y);
            ImGui::SetCursorPosX(ImGui::GetStyle().WindowPadding.x + expand_col_w);
            ImGui::PushStyleColor(ImGuiCol_Text, col4);
            ImGui::Text("%s", sig.display_name.c_str());
            ImGui::PopStyleColor();

            /* Drag-reorder: invisible button over signal name for drag source */
            {
                float abs_row_y = ImGui::GetWindowPos().y - ImGui::GetScrollY() + row_y;
                ImVec2 drag_min(ImGui::GetWindowPos().x + ImGui::GetStyle().WindowPadding.x + expand_col_w,
                                abs_row_y);
                ImVec2 drag_max(ImGui::GetWindowPos().x + sidebar_avail_w - width_col_w - splitter_w,
                                abs_row_y + row_height);

                bool in_drag_zone = (io.MousePos.x >= drag_min.x && io.MousePos.x <= drag_max.x &&
                                     io.MousePos.y >= drag_min.y && io.MousePos.y <= drag_max.y);

                if (in_drag_zone && ImGui::IsMouseClicked(0) && !dragging_signal) {
                    drag_src_idx = (int)i;
                    dragging_signal = true;
                }
            }

            /* Show current value tooltip */
            if (ImGui::IsItemHovered()) {
                char time_buf[64];
                format_time(time_buf, sizeof(time_buf), jzw.sim_end_time);
                ImGui::SetTooltip("[%d] %s  width=%d  type=%s\nEnd: %s",
                                  sig.id, sig.display_name.c_str(),
                                  sig.width, sig.type.c_str(), time_buf);
            }

            /* Width column (right-aligned) */
            {
                char wbuf[16];
                snprintf(wbuf, sizeof(wbuf), "[%d]", sig.width);
                float tw = ImGui::CalcTextSize(wbuf).x;
                ImGui::SetCursorPosY(text_y);
                ImGui::SetCursorPosX(sidebar_avail_w - ImGui::GetStyle().WindowPadding.x - tw - splitter_w);
                ImGui::TextDisabled("%s", wbuf);
            }

            row_idx++;

            /* Expanded individual bits */
            if (sig.expanded && sig.width > 1) {
                for (int b = sig.width - 1; b >= 0; b--) {
                    float bit_row_y = sidebar_start_y + row_idx * row_height;
                    float bit_text_y = bit_row_y + (row_height - ImGui::GetTextLineHeight()) * 0.5f;

                    ImGui::SetCursorPosY(bit_text_y);
                    ImGui::SetCursorPosX(ImGui::GetStyle().WindowPadding.x + expand_col_w + 12.0f);
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(col4.x * 0.7f, col4.y * 0.7f, col4.z * 0.7f, 1.0f));
                    ImGui::Text("[%d]", b);
                    ImGui::PopStyleColor();

                    row_idx++;
                }
            }

            ImGui::PopID();
        }

        /* Handle drag-reorder drop */
        if (dragging_signal && drag_src_idx >= 0) {
            if (!ImGui::IsMouseDown(0)) {
                /* Determine drop target based on mouse Y */
                float mouse_rel_y = io.MousePos.y - (ImGui::GetWindowPos().y - ImGui::GetScrollY() + sidebar_start_y);
                int drop_row = (int)(mouse_rel_y / row_height);

                /* Map drop row to signal index (skip expanded bit rows) */
                int current_row = 0;
                int drop_sig_idx = -1;
                for (size_t si = 0; si < jzw.signals.size(); si++) {
                    if (current_row >= drop_row) {
                        drop_sig_idx = (int)si;
                        break;
                    }
                    current_row++;
                    if (jzw.signals[si].expanded && jzw.signals[si].width > 1) {
                        current_row += jzw.signals[si].width;
                    }
                }
                if (drop_sig_idx < 0) drop_sig_idx = (int)jzw.signals.size() - 1;

                /* Move signal from drag_src_idx to drop_sig_idx */
                if (drop_sig_idx != drag_src_idx && drop_sig_idx >= 0 &&
                    drop_sig_idx < (int)jzw.signals.size()) {
                    Signal moved = jzw.signals[drag_src_idx];
                    jzw.signals.erase(jzw.signals.begin() + drag_src_idx);
                    if (drop_sig_idx > drag_src_idx) drop_sig_idx--;
                    jzw.signals.insert(jzw.signals.begin() + drop_sig_idx, moved);
                }

                dragging_signal = false;
                drag_src_idx = -1;
            } else {
                /* Draw drag indicator */
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
            }
        }

        int total_row_count = row_idx + 1; /* +1 for empty padding row at bottom */

        /* Set content height large enough that ImGui won't clamp our scroll_y.
         * We manage scroll_y ourselves for perfect sidebar/waveform sync. */
        float max_scroll_y = total_row_count * row_height - content_h + 40.0f;
        if (max_scroll_y < 0) max_scroll_y = 0;
        ImGui::SetCursorPosY(content_h + max_scroll_y);
        ImGui::Dummy(ImVec2(1, 0));

        /* Handle mouse wheel over sidebar with same scroll logic as waveform */
        if (ImGui::IsWindowHovered() && io.MouseWheel != 0.0f) {
            scroll_y -= io.MouseWheel * row_height;
            if (scroll_y < 0) scroll_y = 0;
            if (scroll_y > max_scroll_y) scroll_y = max_scroll_y;
        }
        ImGui::SetScrollY(scroll_y);

        ImGui::End();

        /* ---- Sidebar drag handle ---- */
        {
            ImVec2 splitter_pos(wp.x + sidebar_w - splitter_w * 0.5f, content_y);
            ImVec2 splitter_size(splitter_w, content_h);
            ImGui::SetNextWindowPos(splitter_pos);
            ImGui::SetNextWindowSize(splitter_size);
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(1, 1));
            ImGui::Begin("##Splitter", nullptr,
                         ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                         ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                         ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus);

            if (ImGui::IsWindowHovered())
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

            if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0))
                dragging_sidebar = true;
            if (!ImGui::IsMouseDown(0))
                dragging_sidebar = false;
            if (dragging_sidebar) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
                sidebar_w = io.MousePos.x - wp.x;
                if (sidebar_w < sidebar_min_w) sidebar_w = sidebar_min_w;
                if (sidebar_w > sidebar_max_w) sidebar_w = sidebar_max_w;
            }

            ImGui::End();
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor();
        }

        /* ---- Waveform area ---- */
        float wave_x = wp.x + sidebar_w;
        float wave_w = ws.x - sidebar_w;

        ImGui::SetNextWindowPos(ImVec2(wave_x, content_y));
        ImGui::SetNextWindowSize(ImVec2(wave_w, content_h));
        ImGui::Begin("##Waveforms", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoBringToFrontOnFocus |
                     ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        double ps_per_px = zoom_levels[zoom_idx].ps_per_pixel;

        /* Total content width in pixels for the display range */
        int64_t display_duration = jzw.sim_end_time - jzw.display_start_time;
        float total_content_w = (float)(display_duration / ps_per_px) + wave_w;

        /* ---- Keyboard shortcuts ---- */
        /* Only process when no ImGui widget has focus (i.e. not typing in a text box) */
        if (!io.WantTextInput) {
            /* Zoom: +/= to zoom in, - to zoom out */
            if (ImGui::IsKeyPressed(ImGuiKey_Equal) || ImGui::IsKeyPressed(ImGuiKey_KeypadAdd)) {
                if (zoom_idx > 0) {
                    /* Zoom centered on cursor if visible, otherwise center of view */
                    int64_t focus_ps;
                    float focus_screen_x;
                    if (cursor_visible && cursor_ps >= 0) {
                        focus_ps = cursor_ps;
                        focus_screen_x = (float)((cursor_ps - scroll_ps) / ps_per_px);
                    } else {
                        focus_screen_x = wave_w * 0.5f;
                        focus_ps = scroll_ps + (int64_t)(focus_screen_x * ps_per_px);
                    }
                    zoom_idx--;
                    double new_ps_per_px = zoom_levels[zoom_idx].ps_per_pixel;
                    scroll_ps = focus_ps - (int64_t)(focus_screen_x * new_ps_per_px);
                    if (scroll_ps < jzw.display_start_time) scroll_ps = jzw.display_start_time;
                    if (scroll_ps > jzw.sim_end_time) scroll_ps = jzw.sim_end_time;
                }
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Minus) || ImGui::IsKeyPressed(ImGuiKey_KeypadSubtract)) {
                if (zoom_idx < num_zoom_levels - 1) {
                    int64_t focus_ps;
                    float focus_screen_x;
                    if (cursor_visible && cursor_ps >= 0) {
                        focus_ps = cursor_ps;
                        focus_screen_x = (float)((cursor_ps - scroll_ps) / ps_per_px);
                    } else {
                        focus_screen_x = wave_w * 0.5f;
                        focus_ps = scroll_ps + (int64_t)(focus_screen_x * ps_per_px);
                    }
                    zoom_idx++;
                    double new_ps_per_px = zoom_levels[zoom_idx].ps_per_pixel;
                    scroll_ps = focus_ps - (int64_t)(focus_screen_x * new_ps_per_px);
                    if (scroll_ps < jzw.display_start_time) scroll_ps = jzw.display_start_time;
                    if (scroll_ps > jzw.sim_end_time) scroll_ps = jzw.sim_end_time;
                }
            }

            /* Zoom to fit: F key */
            if (ImGui::IsKeyPressed(ImGuiKey_F)) {
                /* Calculate ps_per_pixel needed to fit display range */
                double needed_ps_per_px = (double)display_duration / (double)(wave_w - 20.0f);
                /* Find closest zoom level that fits (>= needed) */
                int best = num_zoom_levels - 1;
                for (int z = 0; z < num_zoom_levels; z++) {
                    if (zoom_levels[z].ps_per_pixel >= needed_ps_per_px) {
                        best = z;
                        break;
                    }
                }
                zoom_idx = best;
                scroll_ps = jzw.display_start_time;
            }

            /* Arrow keys: Left/Right for horizontal scroll */
            if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) || ImGui::IsKeyDown(ImGuiKey_LeftArrow)) {
                double step = ps_per_px * 40.0;
                if (io.KeyShift) step *= 5.0;
                scroll_ps -= (int64_t)step;
                if (scroll_ps < jzw.display_start_time) scroll_ps = jzw.display_start_time;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_RightArrow) || ImGui::IsKeyDown(ImGuiKey_RightArrow)) {
                double step = ps_per_px * 40.0;
                if (io.KeyShift) step *= 5.0;
                scroll_ps += (int64_t)step;
                if (scroll_ps > jzw.sim_end_time) scroll_ps = jzw.sim_end_time;
            }

            /* Arrow keys: Up/Down for vertical scroll */
            if (ImGui::IsKeyPressed(ImGuiKey_UpArrow) || ImGui::IsKeyDown(ImGuiKey_UpArrow)) {
                scroll_y -= row_height;
                if (scroll_y < 0) scroll_y = 0;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_DownArrow) || ImGui::IsKeyDown(ImGuiKey_DownArrow)) {
                float max_scroll_y = total_row_count * row_height - content_h + 40.0f;
                if (max_scroll_y < 0) max_scroll_y = 0;
                scroll_y += row_height;
                if (scroll_y > max_scroll_y) scroll_y = max_scroll_y;
            }

            /* Home/End: jump to start/end of simulation */
            if (ImGui::IsKeyPressed(ImGuiKey_Home)) {
                scroll_ps = jzw.display_start_time;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_End)) {
                /* Scroll so end of simulation is visible at right edge */
                double visible_ps = wave_w * ps_per_px;
                scroll_ps = jzw.sim_end_time - (int64_t)visible_ps;
                if (scroll_ps < jzw.display_start_time) scroll_ps = jzw.display_start_time;
            }

            /* Escape: clear cursors (last placed first) and deactivate toggle */
            if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                if (active_cursor != 0) {
                    active_cursor = 0;
                } else if (cursor4_visible) {
                    cursor4_visible = false;
                    cursor4_ps = -1;
                } else if (cursor3_visible) {
                    cursor3_visible = false;
                    cursor3_ps = -1;
                } else if (cursor2_visible) {
                    cursor2_visible = false;
                    cursor2_ps = -1;
                } else if (cursor_visible) {
                    cursor_visible = false;
                    cursor_ps = -1;
                }
            }
        }

        /* Mouse wheel over waveform */
        if (ImGui::IsWindowHovered()) {
            float wheel = io.MouseWheel;
            if (wheel != 0.0f) {
                if (io.KeyCtrl) {
                    /* Ctrl+wheel = zoom centered on mouse position */
                    float mouse_rel_x = io.MousePos.x - (wave_x + ImGui::GetStyle().WindowPadding.x);
                    int64_t mouse_ps = scroll_ps + (int64_t)(mouse_rel_x * ps_per_px);

                    if (wheel > 0 && zoom_idx > 0) zoom_idx--;
                    else if (wheel < 0 && zoom_idx < num_zoom_levels - 1) zoom_idx++;

                    double new_ps_per_px = zoom_levels[zoom_idx].ps_per_pixel;
                    scroll_ps = mouse_ps - (int64_t)(mouse_rel_x * new_ps_per_px);
                    if (scroll_ps < jzw.display_start_time) scroll_ps = jzw.display_start_time;
                    if (scroll_ps > jzw.sim_end_time) scroll_ps = jzw.sim_end_time;
                    ps_per_px = new_ps_per_px;
                } else {
                    /* Regular wheel = vertical scroll */
                    float max_scroll_y = total_row_count * row_height - content_h + 40.0f;
                    if (max_scroll_y < 0) max_scroll_y = 0;
                    scroll_y -= wheel * row_height;
                    if (scroll_y < 0) scroll_y = 0;
                    if (scroll_y > max_scroll_y) scroll_y = max_scroll_y;
                }
            }
            float wheelH = io.MouseWheelH;
            if (wheelH != 0.0f) {
                double ps_step = ps_per_px * 50.0;
                scroll_ps += (int64_t)(wheelH * ps_step);
                if (scroll_ps < jzw.display_start_time) scroll_ps = jzw.display_start_time;
                if (scroll_ps > jzw.sim_end_time) scroll_ps = jzw.sim_end_time;
            }

            /* Click in waveform area to place cursor based on active toggle */
            if (ImGui::IsMouseClicked(0) && !dragging_signal && active_cursor != 0) {
                float mouse_rel_x = io.MousePos.x - (wave_x + ImGui::GetStyle().WindowPadding.x);
                if (mouse_rel_x >= 0 && mouse_rel_x < wave_w) {
                    int64_t click_ps = scroll_ps + (int64_t)(mouse_rel_x * ps_per_px);
                    if (click_ps >= jzw.display_start_time && click_ps <= jzw.sim_end_time) {
                        if (active_cursor == 1) {
                            cursor_ps = click_ps;
                            cursor_visible = true;
                        } else if (active_cursor == 2) {
                            cursor2_ps = click_ps;
                            cursor2_visible = true;
                        } else if (active_cursor == 3) {
                            cursor3_ps = click_ps;
                            cursor3_visible = true;
                        } else if (active_cursor == 4) {
                            cursor4_ps = click_ps;
                            cursor4_visible = true;
                        }
                    }
                }
            }
        }

        ImDrawList *dl = ImGui::GetWindowDrawList();

        /* Use the fixed window position for drawing, not the scrolled cursor pos */
        ImVec2 wpos = ImGui::GetWindowPos();
        float pad_x = ImGui::GetStyle().WindowPadding.x;
        float pad_y = ImGui::GetStyle().WindowPadding.y;
        wpos.x += pad_x;
        wpos.y += pad_y;

        /* Drawable area within window padding */
        float draw_w = wave_w - pad_x * 2;
        float draw_h = content_h - pad_y * 2;

        /* Apply vertical scroll offset */
        float vert_offset = -scroll_y;

        /* Clip rect for drawing - only draw visible area */
        float clip_x0 = wpos.x;
        float clip_x1 = wpos.x + draw_w;
        float clip_y0 = wpos.y;
        float clip_y1 = wpos.y + draw_h;

        /* Time ruler (fixed at top, does not scroll vertically) */
        float ruler_h = 20.0f;
        {
            ImU32 ruler_col = IM_COL32(100, 100, 100, 255);
            ImU32 ruler_text = IM_COL32(180, 180, 180, 255);

            /* Grid interval: aim for ~100px between labels */
            double interval_ps = ps_per_px * 100.0;
            /* Round to nice number */
            double mag = pow(10.0, floor(log10(interval_ps)));
            double norm = interval_ps / mag;
            if (norm < 2.0) interval_ps = 2.0 * mag;
            else if (norm < 5.0) interval_ps = 5.0 * mag;
            else interval_ps = 10.0 * mag;

            int64_t t_first = (int64_t)(scroll_ps / interval_ps) * (int64_t)interval_ps;
            for (int64_t t = t_first; ; t += (int64_t)interval_ps) {
                float x = wpos.x + (float)((t - scroll_ps) / ps_per_px);
                if (x > clip_x1) break;
                if (x >= clip_x0) {
                    dl->AddLine(ImVec2(x, wpos.y), ImVec2(x, wpos.y + ruler_h),
                                ruler_col, 1.0f);

                    char tbuf[32];
                    format_time(tbuf, sizeof(tbuf), t);
                    dl->AddText(ImVec2(x + 3, wpos.y + 2), ruler_text, tbuf);
                }
            }

            /* Draw cursor marker on ruler */
            if (cursor_visible && cursor_ps >= 0) {
                float cx = wpos.x + (float)((cursor_ps - scroll_ps) / ps_per_px);
                if (cx >= clip_x0 && cx <= clip_x1) {
                    /* Small triangle marker on ruler */
                    ImVec2 tri[3] = {
                        {cx - 4, wpos.y + ruler_h},
                        {cx + 4, wpos.y + ruler_h},
                        {cx, wpos.y + ruler_h - 6}
                    };
                    dl->AddTriangleFilled(tri[0], tri[1], tri[2], IM_COL32(255, 255, 100, 255));
                }
            }
            if (cursor2_visible && cursor2_ps >= 0) {
                float cx = wpos.x + (float)((cursor2_ps - scroll_ps) / ps_per_px);
                if (cx >= clip_x0 && cx <= clip_x1) {
                    ImVec2 tri[3] = {
                        {cx - 4, wpos.y + ruler_h},
                        {cx + 4, wpos.y + ruler_h},
                        {cx, wpos.y + ruler_h - 6}
                    };
                    dl->AddTriangleFilled(tri[0], tri[1], tri[2], IM_COL32(100, 255, 255, 255));
                }
            }
            if (cursor3_visible && cursor3_ps >= 0) {
                float cx = wpos.x + (float)((cursor3_ps - scroll_ps) / ps_per_px);
                if (cx >= clip_x0 && cx <= clip_x1) {
                    ImVec2 tri[3] = {
                        {cx - 4, wpos.y + ruler_h},
                        {cx + 4, wpos.y + ruler_h},
                        {cx, wpos.y + ruler_h - 6}
                    };
                    dl->AddTriangleFilled(tri[0], tri[1], tri[2], IM_COL32(255, 150, 255, 255));
                }
            }
            if (cursor4_visible && cursor4_ps >= 0) {
                float cx = wpos.x + (float)((cursor4_ps - scroll_ps) / ps_per_px);
                if (cx >= clip_x0 && cx <= clip_x1) {
                    ImVec2 tri[3] = {
                        {cx - 4, wpos.y + ruler_h},
                        {cx + 4, wpos.y + ruler_h},
                        {cx, wpos.y + ruler_h - 6}
                    };
                    dl->AddTriangleFilled(tri[0], tri[1], tri[2], IM_COL32(150, 255, 150, 255));
                }
            }
        }

        /* Waveform content area starts below ruler, scrollbar at bottom */
        float sb_h_reserve = 14.0f; /* horizontal scrollbar height + padding */
        float wave_area_y = wpos.y + ruler_h;
        float wave_area_h = draw_h - ruler_h - sb_h_reserve;
        if (wave_area_h < 0) wave_area_h = 0;
        float wave_area_bottom = wave_area_y + wave_area_h;

        /* Clip waveform drawing to area below ruler, above scrollbar */
        dl->PushClipRect(ImVec2(clip_x0, wave_area_y), ImVec2(clip_x1, wave_area_bottom), true);

        /* Draw grid lines through the full waveform area */
        {
            double interval_ps = ps_per_px * 100.0;
            double mag = pow(10.0, floor(log10(interval_ps)));
            double norm = interval_ps / mag;
            if (norm < 2.0) interval_ps = 2.0 * mag;
            else if (norm < 5.0) interval_ps = 5.0 * mag;
            else interval_ps = 10.0 * mag;

            int64_t t_first = (int64_t)(scroll_ps / interval_ps) * (int64_t)interval_ps;
            for (int64_t t = t_first; ; t += (int64_t)interval_ps) {
                float x = wpos.x + (float)((t - scroll_ps) / ps_per_px);
                if (x > clip_x1) break;
                if (x >= clip_x0) {
                    dl->AddLine(ImVec2(x, wave_area_y), ImVec2(x, wave_area_bottom),
                                IM_COL32(50, 50, 50, 255), 1.0f);
                }
            }
        }

        /* Draw each signal waveform */
        int wave_row = 0;
        for (size_t i = 0; i < jzw.signals.size(); i++) {
            const Signal &sig = jzw.signals[i];
            if (!sig.visible) { continue; }

            auto it = jzw.changes.find(sig.id);

            float y = wave_area_y + vert_offset + wave_row * row_height;

            /* Skip rows that are fully off-screen */
            bool row_visible = (y + row_height > wave_area_y) && (y < wave_area_bottom);

            if (row_visible) {
                /* Row separator */
                dl->AddLine(ImVec2(clip_x0, y + row_height),
                            ImVec2(clip_x1, y + row_height),
                            IM_COL32(50, 50, 50, 255), 1.0f);

                /* Highlight row being dragged */
                if (dragging_signal && drag_src_idx == (int)i) {
                    dl->AddRectFilled(ImVec2(clip_x0, y), ImVec2(clip_x1, y + row_height),
                                      IM_COL32(80, 80, 120, 80));
                }

                if (it != jzw.changes.end()) {
                    draw_waveform(dl, sig, it->second,
                                  wpos.x, y, wave_w, row_height,
                                  ps_per_px, scroll_ps);
                }
            }

            wave_row++;

            /* Draw expanded individual bit rows */
            if (sig.expanded && sig.width > 1 && it != jzw.changes.end()) {
                const auto &vc = it->second;
                for (int b = sig.width - 1; b >= 0; b--) {
                    float bit_y = wave_area_y + vert_offset + wave_row * row_height;
                    bool bit_visible = (bit_y + row_height > wave_area_y) && (bit_y < wave_area_bottom);

                    if (bit_visible) {
                        /* Row separator */
                        dl->AddLine(ImVec2(clip_x0, bit_y + row_height),
                                    ImVec2(clip_x1, bit_y + row_height),
                                    IM_COL32(40, 40, 40, 255), 1.0f);

                        /* Create a virtual 1-bit signal for this bit */
                        Signal bit_sig = sig;
                        bit_sig.width = 1;

                        /* Build bit-extracted value changes */
                        std::vector<ValueChange> bit_vc;
                        int64_t t_start_vis = scroll_ps;
                        int64_t t_end_vis = scroll_ps + (int64_t)(wave_w * ps_per_px);

                        /* Find start index via binary search */
                        int lo = 0, hi = (int)vc.size() - 1, start = 0;
                        while (lo <= hi) {
                            int mid = (lo + hi) / 2;
                            if (vc[mid].time <= t_start_vis) { start = mid; lo = mid + 1; }
                            else hi = mid - 1;
                        }

                        /* Extract only visible range */
                        std::string prev_bit_val;
                        for (int j = start; j < (int)vc.size() && vc[j].time <= t_end_vis; j++) {
                            const std::string &val = vc[j].value;
                            int bit_pos = b; /* bit 0 = LSB = last char */
                            std::string bit_val = "0";
                            if (bit_pos < (int)val.size()) {
                                bit_val = (val[val.size() - 1 - bit_pos] == '1') ? "1" : "0";
                            }
                            if (bit_val != prev_bit_val || j == start) {
                                bit_vc.push_back({vc[j].time, bit_val});
                                prev_bit_val = bit_val;
                            }
                        }

                        if (!bit_vc.empty()) {
                            draw_waveform(dl, bit_sig, bit_vc,
                                          wpos.x, bit_y, wave_w, row_height,
                                          ps_per_px, scroll_ps);
                        }
                    }

                    wave_row++;
                }
            }
        }

        /* Draw cursor lines (on top of waveforms, clipped to waveform area) */
        {
            struct { int64_t ps; bool vis; ImU32 col; } cursors[] = {
                {cursor_ps,  cursor_visible,  IM_COL32(255, 255, 100, 180)},
                {cursor2_ps, cursor2_visible, IM_COL32(100, 255, 255, 180)},
                {cursor3_ps, cursor3_visible, IM_COL32(255, 150, 255, 180)},
                {cursor4_ps, cursor4_visible, IM_COL32(150, 255, 150, 180)},
            };
            for (int ci = 0; ci < 4; ci++) {
                if (cursors[ci].vis && cursors[ci].ps >= 0) {
                    float cx = wpos.x + (float)((cursors[ci].ps - scroll_ps) / ps_per_px);
                    if (cx >= clip_x0 && cx <= clip_x1) {
                        dl->AddLine(ImVec2(cx, wave_area_y), ImVec2(cx, wave_area_bottom),
                                    cursors[ci].col, 1.5f);
                    }
                }
            }

            /* Shaded region between C1-C2 */
            if (cursor_visible && cursor_ps >= 0 && cursor2_visible && cursor2_ps >= 0) {
                float cx1 = wpos.x + (float)((cursor_ps - scroll_ps) / ps_per_px);
                float cx2 = wpos.x + (float)((cursor2_ps - scroll_ps) / ps_per_px);
                if (cx1 > cx2) { float tmp = cx1; cx1 = cx2; cx2 = tmp; }
                float left = std::max(cx1, clip_x0);
                float right = std::min(cx2, clip_x1);
                if (left < right) {
                    dl->AddRectFilled(ImVec2(left, wave_area_y), ImVec2(right, wave_area_bottom),
                                      IM_COL32(255, 255, 100, 20));
                }
            }

            /* Shaded region between C3-C4 */
            if (cursor3_visible && cursor3_ps >= 0 && cursor4_visible && cursor4_ps >= 0) {
                float cx1 = wpos.x + (float)((cursor3_ps - scroll_ps) / ps_per_px);
                float cx2 = wpos.x + (float)((cursor4_ps - scroll_ps) / ps_per_px);
                if (cx1 > cx2) { float tmp = cx1; cx1 = cx2; cx2 = tmp; }
                float left = std::max(cx1, clip_x0);
                float right = std::min(cx2, clip_x1);
                if (left < right) {
                    dl->AddRectFilled(ImVec2(left, wave_area_y), ImVec2(right, wave_area_bottom),
                                      IM_COL32(150, 255, 150, 20));
                }
            }
        }

        dl->PopClipRect();

        /* Draw mark vertical lines in waveform area */
        dl->PushClipRect(ImVec2(clip_x0, wave_area_y), ImVec2(clip_x1, wave_area_bottom), true);
        {
            int64_t t_start = scroll_ps;
            int64_t t_end = scroll_ps + (int64_t)(draw_w * ps_per_px);

            for (const auto &ann : jzw.annotations) {
                if (ann.type != "mark") continue;
                if (ann.time > t_end) break;
                if (ann.time < t_start) continue;
                float ax = wpos.x + (float)((ann.time - scroll_ps) / ps_per_px);
                if (ax < clip_x0 || ax > clip_x1) continue;

                ImU32 col = annotation_color(ann.color);
                dl->AddLine(ImVec2(ax, wave_area_y), ImVec2(ax, wave_area_bottom),
                            col, 1.5f);
            }
        }
        dl->PopClipRect();

        /* ---- Custom horizontal scrollbar ---- */
        {
            float sb_h = 12.0f;
            float sb_y = wave_area_bottom + 1.0f;
            float sb_x = wpos.x;
            float sb_w = draw_w;
            float total_time_px = (float)(display_duration / ps_per_px);

            if (total_time_px > sb_w) {
                /* Background track */
                dl->AddRectFilled(ImVec2(sb_x, sb_y), ImVec2(sb_x + sb_w, sb_y + sb_h),
                                  IM_COL32(40, 40, 40, 255), 3.0f);

                /* Thumb */
                float visible_frac = sb_w / total_time_px;
                float thumb_w = sb_w * visible_frac;
                if (thumb_w < 20.0f) thumb_w = 20.0f;
                float scroll_frac = (float)((scroll_ps - jzw.display_start_time) / ps_per_px) / (total_time_px - sb_w);
                if (scroll_frac < 0) scroll_frac = 0;
                if (scroll_frac > 1) scroll_frac = 1;
                float thumb_x = sb_x + scroll_frac * (sb_w - thumb_w);

                bool thumb_hovered = (io.MousePos.x >= thumb_x && io.MousePos.x <= thumb_x + thumb_w &&
                                      io.MousePos.y >= sb_y && io.MousePos.y <= sb_y + sb_h);
                bool track_hovered = (io.MousePos.x >= sb_x && io.MousePos.x <= sb_x + sb_w &&
                                      io.MousePos.y >= sb_y && io.MousePos.y <= sb_y + sb_h);

                static bool dragging_scrollbar = false;
                static float drag_offset = 0.0f;

                if (track_hovered && ImGui::IsMouseClicked(0)) {
                    if (thumb_hovered) {
                        dragging_scrollbar = true;
                        drag_offset = io.MousePos.x - thumb_x;
                    } else {
                        /* Click on track: jump to position */
                        float click_frac = (io.MousePos.x - sb_x - thumb_w * 0.5f) / (sb_w - thumb_w);
                        if (click_frac < 0) click_frac = 0;
                        if (click_frac > 1) click_frac = 1;
                        scroll_ps = jzw.display_start_time + (int64_t)(click_frac * (total_time_px - sb_w) * ps_per_px);
                        dragging_scrollbar = true;
                        drag_offset = thumb_w * 0.5f;
                    }
                }
                if (!ImGui::IsMouseDown(0))
                    dragging_scrollbar = false;
                if (dragging_scrollbar) {
                    float new_thumb_x = io.MousePos.x - drag_offset;
                    float new_frac = (new_thumb_x - sb_x) / (sb_w - thumb_w);
                    if (new_frac < 0) new_frac = 0;
                    if (new_frac > 1) new_frac = 1;
                    scroll_ps = jzw.display_start_time + (int64_t)(new_frac * (total_time_px - sb_w) * ps_per_px);
                    /* Recalc for drawing */
                    scroll_frac = new_frac;
                    thumb_x = sb_x + scroll_frac * (sb_w - thumb_w);
                }

                ImU32 thumb_col = (thumb_hovered || dragging_scrollbar)
                    ? IM_COL32(140, 140, 140, 255)
                    : IM_COL32(90, 90, 90, 255);
                dl->AddRectFilled(ImVec2(thumb_x, sb_y), ImVec2(thumb_x + thumb_w, sb_y + sb_h),
                                  thumb_col, 3.0f);
            }
        }

        ImGui::End();

        /* ---- Gutter: annotation icons (full-width bottom bar) ---- */
        float gutter_y = wp.y + ws.y - gutter_h;
        ImGui::SetNextWindowPos(ImVec2(wp.x, gutter_y));
        ImGui::SetNextWindowSize(ImVec2(ws.x, gutter_h));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin("##Gutter", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                     ImGuiWindowFlags_NoBringToFrontOnFocus);
        ImGui::PopStyleVar();

        {
            ImDrawList *gdl = ImGui::GetWindowDrawList();

            /* Separator line at top of gutter */
            gdl->AddLine(ImVec2(wp.x, gutter_y), ImVec2(wp.x + ws.x, gutter_y),
                         IM_COL32(70, 70, 70, 255), 1.0f);

            /* Annotation icons are aligned to the waveform x coordinates */
            float gutter_clip_x0 = wave_x + pad_x;
            float gutter_clip_x1 = wave_x + wave_w - pad_x;
            float gutter_bottom = gutter_y + gutter_h;

            gdl->PushClipRect(ImVec2(gutter_clip_x0, gutter_y), ImVec2(gutter_clip_x1, gutter_bottom), true);

            int64_t t_start = scroll_ps;
            int64_t t_end = scroll_ps + (int64_t)(draw_w * ps_per_px);
            float icon_size = 8.0f;
            float icon_y_pos = gutter_y + (gutter_h - icon_size) * 0.5f;

            /* Binary search for first annotation at or after t_start */
            const auto &anns = jzw.annotations;
            int ann_lo = 0, ann_hi = (int)anns.size() - 1, ann_start = (int)anns.size();
            while (ann_lo <= ann_hi) {
                int mid = (ann_lo + ann_hi) / 2;
                if (anns[mid].time >= t_start) { ann_start = mid; ann_hi = mid - 1; }
                else ann_lo = mid + 1;
            }
            if (ann_start > 0) ann_start--;

            float last_alert_px = -100.0f;
            float min_alert_spacing = 2.0f;
            float hover_radius = icon_size + 2.0f;
            bool mouse_in_gutter = (io.MousePos.y >= gutter_y && io.MousePos.y <= gutter_bottom);
            std::string tooltip_text;

            for (int ai = ann_start; ai < (int)anns.size(); ai++) {
                const auto &ann = anns[ai];
                if (ann.time > t_end) break;

                float ax = wpos.x + (float)((ann.time - scroll_ps) / ps_per_px);
                if (ax < gutter_clip_x0 || ax > gutter_clip_x1) continue;

                ImU32 col = annotation_color(ann.color);

                if (ann.type == "mark") {
                    gdl->AddRectFilled(
                        ImVec2(ax - icon_size * 0.5f, icon_y_pos),
                        ImVec2(ax + icon_size * 0.5f, icon_y_pos + icon_size),
                        col);

                    if (mouse_in_gutter &&
                        io.MousePos.x >= ax - hover_radius && io.MousePos.x <= ax + hover_radius) {
                        char tbuf[64];
                        format_time(tbuf, sizeof(tbuf), ann.time);
                        if (!tooltip_text.empty()) tooltip_text += "\n---\n";
                        if (!ann.message.empty()) {
                            tooltip_text += "Mark: ";
                            tooltip_text += ann.message;
                            tooltip_text += "\nTime: ";
                            tooltip_text += tbuf;
                        } else {
                            tooltip_text += "Mark at ";
                            tooltip_text += tbuf;
                        }
                    }
                } else if (ann.type == "alert") {
                    /* Check hover even for density-skipped alerts */
                    if (mouse_in_gutter &&
                        io.MousePos.x >= ax - hover_radius && io.MousePos.x <= ax + hover_radius) {
                        char tbuf[64];
                        format_time(tbuf, sizeof(tbuf), ann.time);
                        if (!tooltip_text.empty()) tooltip_text += "\n---\n";
                        if (!ann.message.empty()) {
                            tooltip_text += "Alert: ";
                            tooltip_text += ann.message;
                            tooltip_text += "\nTime: ";
                            tooltip_text += tbuf;
                        } else {
                            tooltip_text += "Alert at ";
                            tooltip_text += tbuf;
                        }
                    }

                    if (ax - last_alert_px < min_alert_spacing) {
                        continue;
                    }
                    last_alert_px = ax;

                    float ch = icon_size;
                    float cw = icon_size * 0.6f;
                    ImVec2 tri[3] = {
                        {ax, icon_y_pos},
                        {ax - cw, icon_y_pos + ch},
                        {ax + cw, icon_y_pos + ch}
                    };
                    gdl->AddTriangleFilled(tri[0], tri[1], tri[2], col);
                }
            }

            if (!tooltip_text.empty()) {
                ImGui::SetTooltip("%s", tooltip_text.c_str());
            }

            gdl->PopClipRect();
        }

        ImGui::End();

        /* ---- Clock Info Dialog ---- */
        if (show_clock_dialog) {
            ImGui::SetNextWindowSizeConstraints(ImVec2(500, 200), ImVec2(800, 600));
            if (ImGui::Begin("Clock Information", &show_clock_dialog,
                             ImGuiWindowFlags_NoCollapse)) {
                if (jzw.clocks.empty()) {
                    ImGui::TextDisabled("No clock data available.");
                } else {
                    for (size_t ci = 0; ci < jzw.clocks.size(); ci++) {
                        const ClockInfo &c = jzw.clocks[ci];

                        if (ci > 0) ImGui::Separator();
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.8f, 1.0f, 1.0f));
                        ImGui::Text("%s", c.name.c_str());
                        ImGui::PopStyleColor();

                        ImGui::Indent(12.0f);

                        /* Period and frequency */
                        double period_ns = c.period_ps / 1e3;
                        double freq_mhz = 1e3 / period_ns;
                        char tbuf[64];
                        format_time(tbuf, sizeof(tbuf), c.period_ps);
                        ImGui::Text("Period:    %s  (%.3f MHz)", tbuf, freq_mhz);

                        /* Phase */
                        if (c.phase_ps > 0) {
                            format_time(tbuf, sizeof(tbuf), c.phase_ps);
                            double phase_deg = (double)c.phase_ps / (double)c.period_ps * 360.0;
                            ImGui::Text("Phase:     %s  (%.1f deg)", tbuf, phase_deg);
                        } else {
                            ImGui::TextDisabled("Phase:     0 (none)");
                        }

                        /* Jitter */
                        if (c.jitter_pp_ps > 0) {
                            format_time(tbuf, sizeof(tbuf), c.jitter_pp_ps);
                            ImGui::Text("Jitter:    %s peak-to-peak", tbuf);
                            char sbuf[64];
                            snprintf(sbuf, sizeof(sbuf), "%.1f ps", c.jitter_sigma_ps);
                            ImGui::Text("           sigma = %s  (clamped at +/-%lld ps)",
                                        sbuf, (long long)(c.jitter_pp_ps / 2));
                        } else {
                            ImGui::TextDisabled("Jitter:    disabled");
                        }

                        /* Drift */
                        if (c.drift_max_ppm > 0.0) {
                            ImGui::Text("Drift:     +/-%.1f ppm max", c.drift_max_ppm);
                            ImGui::Text("           actual = %.6f ppm", c.drift_actual_ppm);
                            if (c.drifted_period_ps > 0.0) {
                                double drifted_ns = c.drifted_period_ps / 1e3;
                                double drifted_mhz = 1e3 / drifted_ns;
                                format_time(tbuf, sizeof(tbuf), (int64_t)c.drifted_period_ps);
                                ImGui::Text("           drifted period = %s  (%.3f MHz)",
                                            tbuf, drifted_mhz);
                            }
                        } else {
                            ImGui::TextDisabled("Drift:     disabled");
                        }

                        ImGui::Unindent(12.0f);
                    }
                }
            }
            ImGui::End();
        }

        /* ---- Render ---- */
        ImGui::Render();
        SDL_SetRenderDrawColor(renderer, 25, 25, 25, 255);
        SDL_RenderClear(renderer);
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);
    }

    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}

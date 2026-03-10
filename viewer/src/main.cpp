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
};

struct ValueChange {
    int64_t     time;
    std::string value;
};

struct JZWFile {
    std::string                          filename;
    std::string                          module_name;
    int64_t                              sim_end_time = 0;
    int64_t                              tick_ps = 1;
    std::vector<Signal>                  signals;
    std::map<int, std::vector<ValueChange>> changes; /* signal_id -> changes */

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

    sqlite3_close(db);
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
    {"1ns",    1000.0   /  1.0},    /*    1 ps/px → 1ns across 1000px */
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

    if (sig.width == 1) {
        /* Digital waveform: step transitions */
        int start_idx = find_start();

        for (int i = start_idx; i < (int)vc.size(); i++) {
            if (vc[i].time > t_end) break;

            int64_t t0 = vc[i].time;
            int64_t t1 = (i + 1 < (int)vc.size()) ? vc[i + 1].time : t_end;
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

    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    /* State */
    int    zoom_idx   = 6;  /* start at 100ns */
    float  sidebar_w  = 200.0f;
    int64_t scroll_ps = 0;
    float  row_height = 30.0f;
    float  toolbar_h  = 32.0f;

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
        ImGui::SetNextWindowPos(wp);
        ImGui::SetNextWindowSize(ImVec2(ws.x, toolbar_h));
        ImGui::Begin("##Toolbar", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus);

        ImGui::Text("%s", display_name);
        if (!jzw.module_name.empty()) {
            ImGui::SameLine();
            ImGui::TextDisabled("(%s)", jzw.module_name.c_str());
        }

        /* Zoom control on the right side */
        {
            const char *zoom_label = zoom_levels[zoom_idx].label;
            /* Fixed-width label area: "Zoom: 100ms/div" is the widest */
            float label_w = ImGui::CalcTextSize("Zoom: 100ms/div").x;
            float btn_w = ImGui::CalcTextSize(" - ").x + ImGui::GetStyle().FramePadding.x * 2;
            float total_w = btn_w * 2 + label_w + ImGui::GetStyle().ItemSpacing.x * 4 + 8;
            ImGui::SameLine(ws.x - total_w);

            if (ImGui::Button(" - ##zout")) {
                if (zoom_idx < num_zoom_levels - 1) zoom_idx++;
            }
            ImGui::SameLine();

            /* Center zoom text in a fixed-width area */
            char zoom_text[64];
            snprintf(zoom_text, sizeof(zoom_text), "Zoom: %s/div", zoom_label);
            float text_actual = ImGui::CalcTextSize(zoom_text).x;
            float pad = (label_w - text_actual) * 0.5f;
            if (pad > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + pad);
            ImGui::Text("%s", zoom_text);
            if (pad > 0) {
                ImGui::SameLine();
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + pad);
            }

            ImGui::SameLine();
            if (ImGui::Button(" + ##zin")) {
                if (zoom_idx > 0) zoom_idx--;
            }
        }

        ImGui::End();

        /* ---- Sidebar (signal names) ---- */
        float content_y = wp.y + toolbar_h;
        float content_h = ws.y - toolbar_h;

        ImGui::SetNextWindowPos(ImVec2(wp.x, content_y));
        ImGui::SetNextWindowSize(ImVec2(sidebar_w, content_h));
        ImGui::Begin("##Sidebar", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoBringToFrontOnFocus);

        /* Reserve space for the ruler row to match the waveform area */
        float sidebar_start_y = ImGui::GetCursorPosY() + 20.0f; /* 20 = ruler_h */

        for (size_t i = 0; i < jzw.signals.size(); i++) {
            const Signal &sig = jzw.signals[i];
            ImGui::PushID(sig.id);

            /* Color-code by type */
            ImU32 col = signal_color(sig);
            ImVec4 col4 = ImGui::ColorConvertU32ToFloat4(col);
            ImGui::PushStyleColor(ImGuiCol_Text, col4);

            /* Position at exact row to match waveform area */
            float row_y = sidebar_start_y + i * row_height;
            float text_y = row_y + (row_height - ImGui::GetTextLineHeight()) * 0.5f;
            ImGui::SetCursorPosY(text_y);
            ImGui::Text("%s", sig.display_name.c_str());

            ImGui::PopStyleColor();

            /* Show current value tooltip */
            if (ImGui::IsItemHovered()) {
                char time_buf[64];
                format_time(time_buf, sizeof(time_buf), jzw.sim_end_time);
                ImGui::SetTooltip("[%d] %s  width=%d  type=%s\nEnd: %s",
                                  sig.id, sig.display_name.c_str(),
                                  sig.width, sig.type.c_str(), time_buf);
            }

            ImGui::PopID();
        }

        ImGui::End();

        /* ---- Waveform area ---- */
        float wave_x = wp.x + sidebar_w;
        float wave_w = ws.x - sidebar_w;

        ImGui::SetNextWindowPos(ImVec2(wave_x, content_y));
        ImGui::SetNextWindowSize(ImVec2(wave_w, content_h));
        ImGui::Begin("##Waveforms", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoBringToFrontOnFocus |
                     ImGuiWindowFlags_HorizontalScrollbar);

        double ps_per_px = zoom_levels[zoom_idx].ps_per_pixel;

        /* Total content width in pixels for the entire simulation */
        float total_content_w = (float)(jzw.sim_end_time / ps_per_px) + wave_w;

        /* Handle zoom via mouse wheel (before we read scroll) */
        if (ImGui::IsWindowHovered()) {
            float wheel = io.MouseWheel;
            if (wheel != 0.0f && !io.KeyShift) {
                int old_zoom = zoom_idx;
                zoom_idx -= (int)wheel;
                if (zoom_idx < 0) zoom_idx = 0;
                if (zoom_idx >= num_zoom_levels) zoom_idx = num_zoom_levels - 1;

                /* Keep mouse position stable during zoom */
                if (zoom_idx != old_zoom) {
                    double old_ps_per_px = zoom_levels[old_zoom].ps_per_pixel;
                    double new_ps_per_px = zoom_levels[zoom_idx].ps_per_pixel;
                    float mx = io.MousePos.x - wave_x;
                    int64_t mouse_time = scroll_ps + (int64_t)(mx * old_ps_per_px);
                    scroll_ps = mouse_time - (int64_t)(mx * new_ps_per_px);
                    if (scroll_ps < 0) scroll_ps = 0;
                    if (scroll_ps > jzw.sim_end_time) scroll_ps = jzw.sim_end_time;

                    /* Recompute with new zoom */
                    ps_per_px = new_ps_per_px;
                    total_content_w = (float)(jzw.sim_end_time / ps_per_px) + wave_w;

                    /* Sync ImGui scroll to our scroll_ps */
                    float target_scroll_x = (float)(scroll_ps / ps_per_px);
                    ImGui::SetScrollX(target_scroll_x);
                }
            }
        }

        /* Place an invisible dummy to define total scrollable width.
         * The dummy height is tiny; all actual drawing uses fixed screen coords. */
        ImGui::Dummy(ImVec2(total_content_w, 1));

        /* Sync scroll_ps from ImGui's horizontal scrollbar */
        float scroll_x = ImGui::GetScrollX();
        scroll_ps = (int64_t)(scroll_x * ps_per_px);
        if (scroll_ps < 0) scroll_ps = 0;
        if (scroll_ps > jzw.sim_end_time) scroll_ps = jzw.sim_end_time;

        ImDrawList *dl = ImGui::GetWindowDrawList();

        /* Use the fixed window position for drawing, not the scrolled cursor pos */
        ImVec2 wpos = ImGui::GetWindowPos();
        wpos.x += ImGui::GetStyle().WindowPadding.x;
        wpos.y += ImGui::GetStyle().WindowPadding.y;

        /* Clip rect for drawing - only draw visible area */
        float clip_x0 = wpos.x;
        float clip_x1 = wpos.x + wave_w;

        /* Time ruler */
        {
            float ruler_h = 20.0f;
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
                    dl->AddLine(ImVec2(x, wpos.y), ImVec2(x, wpos.y + content_h),
                                IM_COL32(50, 50, 50, 255), 1.0f);
                    dl->AddLine(ImVec2(x, wpos.y), ImVec2(x, wpos.y + ruler_h),
                                ruler_col, 1.0f);

                    char tbuf[32];
                    format_time(tbuf, sizeof(tbuf), t);
                    dl->AddText(ImVec2(x + 3, wpos.y + 2), ruler_text, tbuf);
                }
            }

            wpos.y += ruler_h;
        }

        /* Draw each signal waveform */
        for (size_t i = 0; i < jzw.signals.size(); i++) {
            const Signal &sig = jzw.signals[i];
            if (!sig.visible) continue;

            auto it = jzw.changes.find(sig.id);
            if (it == jzw.changes.end()) continue;

            float y = wpos.y + i * row_height;

            /* Row separator */
            dl->AddLine(ImVec2(clip_x0, y + row_height),
                        ImVec2(clip_x1, y + row_height),
                        IM_COL32(50, 50, 50, 255), 1.0f);

            draw_waveform(dl, sig, it->second,
                          wpos.x, y, wave_w, row_height,
                          ps_per_px, scroll_ps);
        }

        ImGui::End();

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

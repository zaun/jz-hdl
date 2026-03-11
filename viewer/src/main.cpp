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
    int    zoom_idx   = 9;  /* start at 1us */
    float  sidebar_w  = 220.0f;
    int64_t scroll_ps = 0;
    float  scroll_y   = 0.0f;
    float  row_height = 30.0f;
    float  toolbar_h  = 32.0f;
    bool   dragging_sidebar = false;
    float  sidebar_min_w = 120.0f;
    float  sidebar_max_w = 500.0f;
    float  splitter_w = 4.0f;

    /* Cursor state */
    int64_t cursor_ps = -1;       /* primary cursor time (-1 = not placed) */
    int64_t cursor2_ps = -1;      /* secondary cursor time (-1 = not placed) */
    bool    cursor_visible = false;
    bool    cursor2_visible = false;

    /* Drag-reorder state */
    int drag_src_idx = -1;        /* index in jzw.signals being dragged */
    bool dragging_signal = false;

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

        /* Cursor info in toolbar */
        if (cursor_visible && cursor_ps >= 0) {
            ImGui::SameLine();
            ImGui::TextDisabled("|");
            ImGui::SameLine();
            char tbuf[64];
            format_time(tbuf, sizeof(tbuf), cursor_ps);
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.4f, 1.0f), "C1: %s", tbuf);

            if (cursor2_visible && cursor2_ps >= 0) {
                ImGui::SameLine();
                format_time(tbuf, sizeof(tbuf), cursor2_ps);
                ImGui::TextColored(ImVec4(0.4f, 1.0f, 1.0f, 1.0f), "C2: %s", tbuf);

                ImGui::SameLine();
                int64_t delta = cursor2_ps - cursor_ps;
                if (delta < 0) delta = -delta;
                format_time(tbuf, sizeof(tbuf), delta);
                ImGui::TextColored(ImVec4(1.0f, 0.6f, 1.0f, 1.0f), "dt: %s", tbuf);
            }
        }

        /* Zoom control on the right side */
        {
            const char *zoom_label = zoom_levels[zoom_idx].label;
            /* Fixed-width label area: "Zoom: 100ms/div" is the widest */
            float label_w = ImGui::CalcTextSize("Zoom: 100ms/div").x;
            float btn_w = ImGui::CalcTextSize(" - ").x + ImGui::GetStyle().FramePadding.x * 2;
            float fit_btn_w = ImGui::CalcTextSize("Fit").x + ImGui::GetStyle().FramePadding.x * 2;
            float total_w = btn_w * 2 + fit_btn_w + label_w + ImGui::GetStyle().ItemSpacing.x * 5 + 8;
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
            ImGui::SameLine();
            if (ImGui::Button("Fit")) {
                float fit_w = ws.x - sidebar_w - 20.0f;
                if (fit_w < 1.0f) fit_w = 1.0f;
                double needed_ps_per_px = (double)jzw.sim_end_time / (double)fit_w;
                int best = num_zoom_levels - 1;
                for (int z = 0; z < num_zoom_levels; z++) {
                    if (zoom_levels[z].ps_per_pixel >= needed_ps_per_px) {
                        best = z;
                        break;
                    }
                }
                zoom_idx = best;
                scroll_ps = 0;
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

        /* Column widths */
        float expand_col_w = 20.0f;
        float width_col_w = 40.0f;
        float name_col_w = sidebar_w - width_col_w - expand_col_w -
                           ImGui::GetStyle().WindowPadding.x * 2 - 8.0f;
        (void)name_col_w;

        /* Cursor value column width */
        float cursor_val_w = 0.0f;
        if (cursor_visible && cursor_ps >= 0) {
            cursor_val_w = 60.0f;
        }

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
                ImVec2 drag_max(ImGui::GetWindowPos().x + sidebar_w - width_col_w - cursor_val_w - splitter_w,
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

            /* Cursor value column (between name and width) */
            if (cursor_visible && cursor_ps >= 0) {
                const char *binval = jzw.value_at(sig.id, cursor_ps);
                char vbuf[32];
                format_value(vbuf, sizeof(vbuf), binval, sig.width);
                float vw = ImGui::CalcTextSize(vbuf).x;
                ImGui::SetCursorPosY(text_y);
                ImGui::SetCursorPosX(sidebar_w - ImGui::GetStyle().WindowPadding.x - width_col_w - cursor_val_w - splitter_w + (cursor_val_w - vw) * 0.5f);
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.4f, 1.0f), "%s", vbuf);
            }

            /* Width column (right-aligned) */
            {
                char wbuf[16];
                snprintf(wbuf, sizeof(wbuf), "[%d]", sig.width);
                float tw = ImGui::CalcTextSize(wbuf).x;
                ImGui::SetCursorPosY(text_y);
                ImGui::SetCursorPosX(sidebar_w - ImGui::GetStyle().WindowPadding.x - tw - splitter_w);
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

                    /* Cursor value for expanded bit */
                    if (cursor_visible && cursor_ps >= 0) {
                        const char *binval = jzw.value_at(sig.id, cursor_ps);
                        int bit_pos = b;
                        int len = (int)strlen(binval);
                        char bit_val = '0';
                        if (bit_pos < len) {
                            bit_val = binval[len - 1 - bit_pos];
                        }
                        char vbuf[4];
                        snprintf(vbuf, sizeof(vbuf), "%c", bit_val);
                        float vw = ImGui::CalcTextSize(vbuf).x;
                        ImGui::SetCursorPosY(bit_text_y);
                        ImGui::SetCursorPosX(sidebar_w - ImGui::GetStyle().WindowPadding.x - width_col_w - cursor_val_w - splitter_w + (cursor_val_w - vw) * 0.5f);
                        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.4f, 0.7f), "%s", vbuf);
                    }

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

        /* Set total content height so scrollbar appears */
        float total_rows_h = sidebar_start_y + row_idx * row_height + ImGui::GetStyle().WindowPadding.y;
        ImGui::SetCursorPosY(total_rows_h);
        ImGui::Dummy(ImVec2(1, 0));

        /* Sync vertical scroll */
        if (ImGui::IsWindowHovered() && io.MouseWheel != 0.0f) {
            /* Sidebar is being scrolled - read its position as source of truth */
            scroll_y = ImGui::GetScrollY();
        } else {
            /* Otherwise push our scroll_y into the sidebar */
            ImGui::SetScrollY(scroll_y);
        }

        int total_row_count = row_idx; /* save for waveform area */

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

        /* Total content width in pixels for the entire simulation */
        float total_content_w = (float)(jzw.sim_end_time / ps_per_px) + wave_w;

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
                    if (scroll_ps < 0) scroll_ps = 0;
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
                    if (scroll_ps < 0) scroll_ps = 0;
                    if (scroll_ps > jzw.sim_end_time) scroll_ps = jzw.sim_end_time;
                }
            }

            /* Zoom to fit: F key */
            if (ImGui::IsKeyPressed(ImGuiKey_F)) {
                /* Calculate ps_per_pixel needed to fit entire simulation */
                double needed_ps_per_px = (double)jzw.sim_end_time / (double)(wave_w - 20.0f);
                /* Find closest zoom level that fits (>= needed) */
                int best = num_zoom_levels - 1;
                for (int z = 0; z < num_zoom_levels; z++) {
                    if (zoom_levels[z].ps_per_pixel >= needed_ps_per_px) {
                        best = z;
                        break;
                    }
                }
                zoom_idx = best;
                scroll_ps = 0;
            }

            /* Arrow keys: Left/Right for horizontal scroll */
            if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) || ImGui::IsKeyDown(ImGuiKey_LeftArrow)) {
                double step = ps_per_px * 40.0;
                if (io.KeyShift) step *= 5.0;
                scroll_ps -= (int64_t)step;
                if (scroll_ps < 0) scroll_ps = 0;
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
                scroll_ps = 0;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_End)) {
                /* Scroll so end of simulation is visible at right edge */
                double visible_ps = wave_w * ps_per_px;
                scroll_ps = jzw.sim_end_time - (int64_t)visible_ps;
                if (scroll_ps < 0) scroll_ps = 0;
            }

            /* Escape: clear cursors */
            if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                if (cursor2_visible) {
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
                    if (scroll_ps < 0) scroll_ps = 0;
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
                if (scroll_ps < 0) scroll_ps = 0;
                if (scroll_ps > jzw.sim_end_time) scroll_ps = jzw.sim_end_time;
            }

            /* Click in waveform area to place cursor */
            if (ImGui::IsMouseClicked(0) && !dragging_signal) {
                float mouse_rel_x = io.MousePos.x - (wave_x + ImGui::GetStyle().WindowPadding.x);
                if (mouse_rel_x >= 0 && mouse_rel_x < wave_w) {
                    int64_t click_ps = scroll_ps + (int64_t)(mouse_rel_x * ps_per_px);
                    if (click_ps >= 0 && click_ps <= jzw.sim_end_time) {
                        if (io.KeyShift && cursor_visible) {
                            /* Shift+click places secondary cursor */
                            cursor2_ps = click_ps;
                            cursor2_visible = true;
                        } else {
                            cursor_ps = click_ps;
                            cursor_visible = true;
                            /* Clear secondary cursor on primary click */
                            cursor2_visible = false;
                            cursor2_ps = -1;
                        }
                    }
                }
            }
        }

        ImDrawList *dl = ImGui::GetWindowDrawList();

        /* Use the fixed window position for drawing, not the scrolled cursor pos */
        ImVec2 wpos = ImGui::GetWindowPos();
        wpos.x += ImGui::GetStyle().WindowPadding.x;
        wpos.y += ImGui::GetStyle().WindowPadding.y;

        /* Apply vertical scroll offset */
        float vert_offset = -scroll_y;

        /* Clip rect for drawing - only draw visible area */
        float clip_x0 = wpos.x;
        float clip_x1 = wpos.x + wave_w;
        float clip_y0 = wpos.y;
        float clip_y1 = wpos.y + content_h;

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
        }

        /* Waveform content area starts below ruler */
        float wave_area_y = wpos.y + ruler_h;
        float wave_area_h = content_h - ruler_h;

        /* Clip waveform drawing to area below ruler */
        dl->PushClipRect(ImVec2(clip_x0, wave_area_y), ImVec2(clip_x1, clip_y1), true);

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
                    dl->AddLine(ImVec2(x, wave_area_y), ImVec2(x, clip_y1),
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
            bool row_visible = (y + row_height > wave_area_y) && (y < clip_y1);

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
                    bool bit_visible = (bit_y + row_height > wave_area_y) && (bit_y < clip_y1);

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

        /* Draw cursor lines (on top of waveforms) */
        if (cursor_visible && cursor_ps >= 0) {
            float cx = wpos.x + (float)((cursor_ps - scroll_ps) / ps_per_px);
            if (cx >= clip_x0 && cx <= clip_x1) {
                dl->AddLine(ImVec2(cx, wave_area_y), ImVec2(cx, clip_y1),
                            IM_COL32(255, 255, 100, 180), 1.5f);
            }
        }
        if (cursor2_visible && cursor2_ps >= 0) {
            float cx = wpos.x + (float)((cursor2_ps - scroll_ps) / ps_per_px);
            if (cx >= clip_x0 && cx <= clip_x1) {
                dl->AddLine(ImVec2(cx, wave_area_y), ImVec2(cx, clip_y1),
                            IM_COL32(100, 255, 255, 180), 1.5f);
            }

            /* Draw shaded region between cursors */
            if (cursor_visible && cursor_ps >= 0) {
                float cx1 = wpos.x + (float)((cursor_ps - scroll_ps) / ps_per_px);
                float cx2 = wpos.x + (float)((cursor2_ps - scroll_ps) / ps_per_px);
                if (cx1 > cx2) { float tmp = cx1; cx1 = cx2; cx2 = tmp; }
                float left = std::max(cx1, clip_x0);
                float right = std::min(cx2, clip_x1);
                if (left < right) {
                    dl->AddRectFilled(ImVec2(left, wave_area_y), ImVec2(right, clip_y1),
                                      IM_COL32(255, 255, 100, 20));
                }
            }
        }

        dl->PopClipRect();

        /* ---- Custom horizontal scrollbar ---- */
        {
            float sb_h = 12.0f;
            float sb_y = wpos.y + content_h - sb_h - ImGui::GetStyle().WindowPadding.y;
            float sb_x = wpos.x;
            float sb_w = wave_w - ImGui::GetStyle().WindowPadding.x * 2;
            float total_time_px = (float)(jzw.sim_end_time / ps_per_px);

            if (total_time_px > sb_w) {
                /* Background track */
                dl->AddRectFilled(ImVec2(sb_x, sb_y), ImVec2(sb_x + sb_w, sb_y + sb_h),
                                  IM_COL32(40, 40, 40, 255), 3.0f);

                /* Thumb */
                float visible_frac = sb_w / total_time_px;
                float thumb_w = sb_w * visible_frac;
                if (thumb_w < 20.0f) thumb_w = 20.0f;
                float scroll_frac = (float)(scroll_ps / ps_per_px) / (total_time_px - sb_w);
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
                        scroll_ps = (int64_t)(click_frac * (total_time_px - sb_w) * ps_per_px);
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
                    scroll_ps = (int64_t)(new_frac * (total_time_px - sb_w) * ps_per_px);
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

/**
 * @file sim_jzw.c
 * @brief JZW (JZ-HDL Waveform) SQLite-based waveform writer.
 *
 * Implements the JZW specification: SQLite database with meta, signals,
 * changes, and annotations tables.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sim_jzw.h"
#include "sqlite3.h"

#define JZW_MAX_SIGNALS 4096
#define JZW_BATCH_SIZE  1000

typedef struct {
    int      width;
    uint64_t last_value;
    int      has_value;   /* whether we've written at least one value */
} JZWSignal;

struct JZWWriter {
    sqlite3      *db;
    sqlite3_stmt *insert_change;
    int           num_signals;
    JZWSignal     signals[JZW_MAX_SIGNALS];
    uint64_t      current_time;
    uint64_t      end_time;
    int           defs_ended;
    int           batch_count;
    int           in_transaction;
};

/* ---- Internal helpers ---- */

static int jzw_exec(JZWWriter *w, const char *sql)
{
    char *err = NULL;
    int rc = sqlite3_exec(w->db, sql, NULL, NULL, &err);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "JZW SQL error: %s\n", err ? err : "unknown");
        sqlite3_free(err);
        return -1;
    }
    return 0;
}

static void jzw_begin(JZWWriter *w)
{
    if (!w->in_transaction) {
        jzw_exec(w, "BEGIN TRANSACTION");
        w->in_transaction = 1;
    }
}

static void jzw_commit(JZWWriter *w)
{
    if (w->in_transaction) {
        jzw_exec(w, "COMMIT");
        w->in_transaction = 0;
        w->batch_count = 0;
    }
}

static void jzw_maybe_commit(JZWWriter *w)
{
    if (w->batch_count >= JZW_BATCH_SIZE) {
        jzw_commit(w);
    }
}

/**
 * @brief Format a value as a binary string (MSB first).
 */
static void value_to_binstr(char *buf, uint64_t value, int width)
{
    for (int i = 0; i < width; i++) {
        int bit = (width - 1) - i;
        buf[i] = (value >> bit) & 1 ? '1' : '0';
    }
    buf[width] = '\0';
}

/* ---- Public API ---- */

JZWWriter *jzw_open(const char *filename, uint64_t timescale_ps)
{
    (void)timescale_ps;

    JZWWriter *w = (JZWWriter *)calloc(1, sizeof(JZWWriter));
    if (!w) return NULL;

    /* Remove existing file to start fresh */
    remove(filename);

    int rc = sqlite3_open(filename, &w->db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "JZW: failed to create database '%s': %s\n",
                filename, sqlite3_errmsg(w->db));
        sqlite3_close(w->db);
        free(w);
        return NULL;
    }

    /* Set pragmas per spec Section 2.4 / 6.3 */
    jzw_exec(w, "PRAGMA journal_mode=WAL");
    jzw_exec(w, "PRAGMA synchronous=NORMAL");
    jzw_exec(w, "PRAGMA cache_size=-8000");
    jzw_exec(w, "PRAGMA temp_store=MEMORY");

    /* Create schema per spec Section 3 */
    jzw_exec(w,
        "CREATE TABLE meta ("
        "  key   TEXT PRIMARY KEY,"
        "  value TEXT NOT NULL"
        ")");

    jzw_exec(w,
        "CREATE TABLE signals ("
        "  id    INTEGER PRIMARY KEY,"
        "  name  TEXT    NOT NULL,"
        "  scope TEXT    NOT NULL,"
        "  width INTEGER NOT NULL,"
        "  type  TEXT    NOT NULL"
        ")");

    jzw_exec(w,
        "CREATE TABLE changes ("
        "  time      INTEGER NOT NULL,"
        "  signal_id INTEGER NOT NULL,"
        "  value     TEXT    NOT NULL,"
        "  FOREIGN KEY (signal_id) REFERENCES signals(id)"
        ")");

    jzw_exec(w, "CREATE INDEX idx_changes_time ON changes(time)");
    jzw_exec(w, "CREATE INDEX idx_changes_signal ON changes(signal_id, time)");

    jzw_exec(w,
        "CREATE TABLE annotations ("
        "  id        INTEGER PRIMARY KEY,"
        "  time      INTEGER NOT NULL,"
        "  type      TEXT    NOT NULL,"
        "  signal_id INTEGER,"
        "  message   TEXT,"
        "  color     TEXT,"
        "  end_time  INTEGER,"
        "  FOREIGN KEY (signal_id) REFERENCES signals(id)"
        ")");

    jzw_exec(w, "CREATE INDEX idx_annotations_time ON annotations(time)");

    jzw_exec(w,
        "CREATE TABLE clocks ("
        "  id               INTEGER PRIMARY KEY,"
        "  name             TEXT    NOT NULL,"
        "  period_ps        INTEGER NOT NULL,"
        "  phase_ps         INTEGER NOT NULL DEFAULT 0,"
        "  jitter_pp_ps     INTEGER NOT NULL DEFAULT 0,"
        "  jitter_sigma_ps  REAL    NOT NULL DEFAULT 0.0,"
        "  drift_max_ppm    REAL    NOT NULL DEFAULT 0.0,"
        "  drift_actual_ppm REAL    NOT NULL DEFAULT 0.0,"
        "  drifted_period_ps REAL   NOT NULL DEFAULT 0.0"
        ")");

    /* Prepare insert statement */
    rc = sqlite3_prepare_v2(w->db,
        "INSERT INTO changes (time, signal_id, value) VALUES (?, ?, ?)",
        -1, &w->insert_change, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "JZW: failed to prepare statement: %s\n",
                sqlite3_errmsg(w->db));
        sqlite3_close(w->db);
        free(w);
        return NULL;
    }

    /* Write required meta: jzw_version and timescale */
    jzw_exec(w,
        "INSERT INTO meta (key, value) VALUES ('jzw_version', '1')");
    jzw_exec(w,
        "INSERT INTO meta (key, value) VALUES ('timescale', '1ps')");
    jzw_exec(w,
        "INSERT INTO meta (key, value) VALUES ('sim_start_time', '0')");

    return w;
}

void jzw_set_meta(JZWWriter *w, const char *key, const char *value)
{
    if (!w || !key || !value) return;

    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(w->db,
        "INSERT OR REPLACE INTO meta (key, value) VALUES (?, ?)",
        -1, &stmt, NULL);
    if (rc != SQLITE_OK) return;

    sqlite3_bind_text(stmt, 1, key, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, value, -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void jzw_add_clock(JZWWriter *w, const char *name, uint64_t period_ps,
                   uint64_t phase_ps, uint64_t jitter_pp_ps,
                   double jitter_sigma_ps, double drift_max_ppm,
                   double drift_actual_ppm, double drifted_period_ps)
{
    if (!w || !name) return;

    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(w->db,
        "INSERT INTO clocks (name, period_ps, phase_ps, jitter_pp_ps, "
        "jitter_sigma_ps, drift_max_ppm, drift_actual_ppm, drifted_period_ps) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?)",
        -1, &stmt, NULL);
    if (rc != SQLITE_OK) return;

    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, (sqlite3_int64)period_ps);
    sqlite3_bind_int64(stmt, 3, (sqlite3_int64)phase_ps);
    sqlite3_bind_int64(stmt, 4, (sqlite3_int64)jitter_pp_ps);
    sqlite3_bind_double(stmt, 5, jitter_sigma_ps);
    sqlite3_bind_double(stmt, 6, drift_max_ppm);
    sqlite3_bind_double(stmt, 7, drift_actual_ppm);
    sqlite3_bind_double(stmt, 8, drifted_period_ps);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

int jzw_add_signal(JZWWriter *w, const char *scope, const char *name,
                   int width, const char *type)
{
    if (!w || w->defs_ended || w->num_signals >= JZW_MAX_SIGNALS) return -1;

    int idx = w->num_signals;
    int db_id = idx + 1; /* 1-based in the database */

    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(w->db,
        "INSERT INTO signals (id, name, scope, width, type) "
        "VALUES (?, ?, ?, ?, ?)",
        -1, &stmt, NULL);
    if (rc != SQLITE_OK) return -1;

    sqlite3_bind_int(stmt, 1, db_id);
    sqlite3_bind_text(stmt, 2, name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, scope ? scope : "top", -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, width);
    sqlite3_bind_text(stmt, 5, type ? type : "wire", -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    w->signals[idx].width = width;
    w->signals[idx].last_value = 0;
    w->signals[idx].has_value = 0;

    w->num_signals++;
    return idx;
}

void jzw_end_definitions(JZWWriter *w)
{
    if (!w || w->defs_ended) return;
    w->defs_ended = 1;
}

void jzw_set_time(JZWWriter *w, uint64_t time_ps)
{
    if (!w || !w->defs_ended) return;

    /* Commit previous batch if switching time */
    if (time_ps != w->current_time && w->in_transaction) {
        jzw_maybe_commit(w);
    }

    w->current_time = time_ps;
    if (time_ps > w->end_time) {
        w->end_time = time_ps;
    }
}

void jzw_dump_value(JZWWriter *w, int sig_id, uint64_t value, int width)
{
    if (!w || !w->defs_ended || sig_id < 0 || sig_id >= w->num_signals) return;

    JZWSignal *sig = &w->signals[sig_id];

    /* Change-only storage: skip if value unchanged (except at time 0) */
    if (sig->has_value && value == sig->last_value && w->current_time != 0) {
        return;
    }

    sig->last_value = value;
    sig->has_value = 1;

    /* Format value as binary string */
    char buf[65];
    value_to_binstr(buf, value, width);

    if (!w->in_transaction) {
        jzw_begin(w);
    }

    sqlite3_reset(w->insert_change);
    sqlite3_bind_int64(w->insert_change, 1, (sqlite3_int64)w->current_time);
    sqlite3_bind_int(w->insert_change, 2, sig_id + 1); /* 1-based */
    sqlite3_bind_text(w->insert_change, 3, buf, -1, SQLITE_TRANSIENT);
    sqlite3_step(w->insert_change);

    w->batch_count++;
    jzw_maybe_commit(w);
}

void jzw_add_annotation(JZWWriter *w, uint64_t time_ps, const char *type,
                         int signal_id, const char *message,
                         const char *color, uint64_t end_time)
{
    if (!w || !w->defs_ended || !type) return;

    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(w->db,
        "INSERT INTO annotations (time, type, signal_id, message, color, end_time) "
        "VALUES (?, ?, ?, ?, ?, ?)",
        -1, &stmt, NULL);
    if (rc != SQLITE_OK) return;

    if (!w->in_transaction) {
        jzw_begin(w);
    }

    sqlite3_bind_int64(stmt, 1, (sqlite3_int64)time_ps);
    sqlite3_bind_text(stmt, 2, type, -1, SQLITE_TRANSIENT);
    if (signal_id >= 0)
        sqlite3_bind_int(stmt, 3, signal_id + 1); /* 1-based */
    else
        sqlite3_bind_null(stmt, 3);
    if (message)
        sqlite3_bind_text(stmt, 4, message, -1, SQLITE_TRANSIENT);
    else
        sqlite3_bind_null(stmt, 4);
    if (color)
        sqlite3_bind_text(stmt, 5, color, -1, SQLITE_TRANSIENT);
    else
        sqlite3_bind_null(stmt, 5);
    if (end_time > 0)
        sqlite3_bind_int64(stmt, 6, (sqlite3_int64)end_time);
    else
        sqlite3_bind_null(stmt, 6);

    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    w->batch_count++;
    jzw_maybe_commit(w);
}

void jzw_close(JZWWriter *w)
{
    if (!w) return;

    /* Flush any pending transaction */
    if (w->in_transaction) {
        jzw_commit(w);
    }

    /* Write sim_end_time */
    if (w->db) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%llu", (unsigned long long)w->end_time);
        jzw_set_meta(w, "sim_end_time", buf);
    }

    if (w->insert_change) {
        sqlite3_finalize(w->insert_change);
    }

    if (w->db) {
        sqlite3_close(w->db);
    }

    free(w);
}

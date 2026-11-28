/* Copyright (c) 2026 Todd Carnes
 * SPDX-License-Identifier: MIT
 * See LICENSE in the project root for the full license text.
 */
#include "jpl_private.h"

#include <math.h>
#include <string.h>

jpl_status jpl_schedule_endpoint(const double start[2], double step, size_t index,
                                 double out[2]) {
    double coverage[2];
    jpl_status status;
    if (!start || !out) return JPL_ERROR_FORMAT;
    if (!isfinite(start[0]) || !isfinite(start[1]) || !isfinite(step) || step <= 0.0)
        return JPL_ERROR_FORMAT;
    status = jpl_date_normalize(start, coverage);
    if (status != JPL_OK) return JPL_ERROR_FORMAT;
    status = jpl_date_add_scaled(coverage, step, index, out);
    return status == JPL_OK ? JPL_OK : JPL_ERROR_FORMAT;
}

jpl_status jpl_schedule_record(const jpl_header *header, size_t covered_count,
                               const jpl_verified_epoch *epoch,
                               jpl_record_span *out) {
    double cov_start[2], norm[2], diff[2], boundary[2], next[2];
    double quotient_d;
    size_t candidate = 0, lo, hi;
    jpl_status status;
    if (!header || !epoch || !out) return JPL_ERROR_ARGUMENT;
    if (covered_count == 0) return JPL_ERROR_FORMAT;
    status = jpl_date_normalize(header->start_jd, cov_start);
    if (status != JPL_OK) return JPL_ERROR_FORMAT;
    if (!isfinite(header->step_days) || header->step_days <= 0.0) return JPL_ERROR_FORMAT;
    memcpy(norm, epoch->epoch, sizeof norm);
    status = jpl_date_normalize(header->end_jd, boundary);
    if (status != JPL_OK) return JPL_ERROR_FORMAT;
    if (jpl_date_compare(norm, cov_start) < 0 || jpl_date_compare(norm, boundary) > 0)
        return JPL_ERROR_RANGE;
    if (jpl_date_compare(norm, boundary) == 0) {
        candidate = covered_count - 1;
        status = jpl_date_add_scaled(cov_start, header->step_days, candidate, out->start);
        if (status != JPL_OK) return JPL_ERROR_FORMAT;
        out->index = candidate;
        memcpy(out->end, boundary, sizeof out->end);
        return JPL_OK;
    }
    status = jpl_date_subtract(norm, cov_start, diff);
    if (status != JPL_OK) return JPL_ERROR_ARGUMENT;
    quotient_d = (diff[0] + diff[1]) / header->step_days;
    if (!isfinite(quotient_d) || quotient_d < 0.0) return JPL_ERROR_RANGE;
    candidate = quotient_d >= (double)covered_count ? covered_count - 1 : (size_t)quotient_d;
    for (;;) {
        int below = 0, above = 0;
        status = jpl_date_add_scaled(cov_start, header->step_days, candidate, boundary);
        if (status != JPL_OK) return JPL_ERROR_FORMAT;
        if (candidate + 1 < covered_count) {
            status = jpl_date_add_scaled(cov_start, header->step_days, candidate + 1, next);
            if (status != JPL_OK) return JPL_ERROR_FORMAT;
        } else {
            status = jpl_date_normalize(header->end_jd, next);
            if (status != JPL_OK) return JPL_ERROR_FORMAT;
        }
        below = jpl_date_compare(norm, boundary) < 0;
        above = jpl_date_compare(norm, next) >= 0 && candidate + 1 < covered_count;
        if (!below && !above) {
            out->index = candidate;
            memcpy(out->start, boundary, sizeof out->start);
            memcpy(out->end, next, sizeof out->end);
            return JPL_OK;
        }
        if (below && candidate > 0) {
            size_t probe = candidate - 1;
            status = jpl_date_add_scaled(cov_start, header->step_days, probe, boundary);
            if (status != JPL_OK) return JPL_ERROR_FORMAT;
            status = jpl_date_add_scaled(cov_start, header->step_days, candidate, next);
            if (status != JPL_OK) return JPL_ERROR_FORMAT;
            if (jpl_date_compare(norm, boundary) >= 0 && jpl_date_compare(norm, next) < 0) {
                candidate = probe;
                continue;
            }
            break;
        }
        if (above && candidate + 1 < covered_count) {
            size_t probe = candidate + 1;
            status = jpl_date_add_scaled(cov_start, header->step_days, probe, boundary);
            if (status != JPL_OK) return JPL_ERROR_FORMAT;
            if (candidate + 2 < covered_count) {
                status = jpl_date_add_scaled(cov_start, header->step_days, probe + 1, next);
                if (status != JPL_OK) return JPL_ERROR_FORMAT;
            } else {
                status = jpl_date_normalize(header->end_jd, next);
                if (status != JPL_OK) return JPL_ERROR_FORMAT;
            }
            if (jpl_date_compare(norm, boundary) >= 0 &&
                (jpl_date_compare(norm, next) < 0 || probe + 1 >= covered_count)) {
                candidate = probe;
                continue;
            }
            break;
        }
        break;
    }
    lo = 0;
    hi = covered_count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        status = jpl_date_add_scaled(cov_start, header->step_days, mid, boundary);
        if (status != JPL_OK) return JPL_ERROR_FORMAT;
        if (jpl_date_compare(norm, boundary) < 0) {
            hi = mid;
        } else {
            if (mid + 1 < covered_count) {
                status = jpl_date_add_scaled(cov_start, header->step_days, mid + 1, next);
                if (status != JPL_OK) return JPL_ERROR_FORMAT;
            } else {
                status = jpl_date_normalize(header->end_jd, next);
                if (status != JPL_OK) return JPL_ERROR_FORMAT;
            }
            if (jpl_date_compare(norm, next) < 0 || mid + 1 >= covered_count) {
                out->index = mid;
                status = jpl_date_add_scaled(cov_start, header->step_days, mid, out->start);
                if (status != JPL_OK) return JPL_ERROR_FORMAT;
                memcpy(out->end, next, sizeof out->end);
                return JPL_OK;
            }
            lo = mid + 1;
        }
    }
    return JPL_ERROR_RANGE;
}

jpl_status jpl_schedule_verify_record(size_t index, const double record_dates[2],
                                      const jpl_verified_epoch *epoch,
                                      jpl_record_span *out) {
    double start[2], end[2];
    jpl_status status;
    if (!record_dates || !epoch || !out) return JPL_ERROR_ARGUMENT;
    start[0] = record_dates[0]; start[1] = 0.0;
    end[0] = record_dates[1]; end[1] = 0.0;
    status = jpl_date_normalize(start, start);
    if (status != JPL_OK) return JPL_ERROR_FORMAT;
    status = jpl_date_normalize(end, end);
    if (status != JPL_OK) return JPL_ERROR_FORMAT;
    if (jpl_date_compare(end, start) <= 0) return JPL_ERROR_FORMAT;
    if (jpl_date_compare(epoch->epoch, start) < 0 ||
        jpl_date_compare(epoch->epoch, end) > 0)
        return JPL_ERROR_FORMAT;
    out->index = index;
    memcpy(out->start, start, sizeof out->start);
    memcpy(out->end, end, sizeof out->end);
    return JPL_OK;
}

jpl_status jpl_schedule_subinterval(const jpl_record_span *span, size_t nsub,
                                    const jpl_verified_epoch *epoch,
                                    jpl_subinterval_span *out) {
    double span_days, rec_span[2], from_start[2], local[2];
    double qd, lo[2], hi[2];
    int below, above;
    jpl_status status;
    size_t subinterval;
    if (!span || !epoch || !out || nsub == 0) return JPL_ERROR_ARGUMENT;
    status = jpl_date_subtract(span->end, span->start, rec_span);
    if (status != JPL_OK) return JPL_ERROR_FORMAT;
    span_days = rec_span[0] + rec_span[1];
    if (!isfinite(span_days) || span_days <= 0.0) return JPL_ERROR_FORMAT;
    out->substep = span_days / (double)nsub;
    if (!isfinite(out->substep) || out->substep <= 0.0) return JPL_ERROR_FORMAT;
    status = jpl_date_subtract(epoch->epoch, span->start, from_start);
    if (status != JPL_OK) return JPL_ERROR_FORMAT;
    qd = (from_start[0] + from_start[1]) / out->substep;
    if (!isfinite(qd) || qd < 0.0) return JPL_ERROR_FORMAT;
    subinterval = qd >= (double)nsub ? nsub - 1 : (size_t)qd;
    for (;;) {
        below = 0; above = 0;
        status = jpl_date_partition(span->start, span->end, nsub, subinterval, lo);
        if (status != JPL_OK) return JPL_ERROR_FORMAT;
        status = jpl_date_partition(span->start, span->end, nsub, subinterval + 1, hi);
        if (status != JPL_OK) return JPL_ERROR_FORMAT;
        below = jpl_date_compare(epoch->epoch, lo) < 0;
        above = subinterval + 1 < nsub ? jpl_date_compare(epoch->epoch, hi) >= 0 : 0;
        if (!below && !above) break;
        if (below && subinterval > 0) { --subinterval; continue; }
        if (above && subinterval + 1 < nsub) { ++subinterval; continue; }
        return JPL_ERROR_FORMAT;
    }
    out->index = subinterval;
    status = jpl_date_partition(span->start, span->end, nsub, subinterval, out->origin);
    if (status != JPL_OK) return JPL_ERROR_FORMAT;
    status = jpl_date_subtract(epoch->epoch, out->origin, local);
    if (status != JPL_OK) return JPL_ERROR_FORMAT;
    out->argument = 2.0 * ((local[0] + local[1]) / out->substep) - 1.0;
    if (!isfinite(out->argument)) return JPL_ERROR_FORMAT;
    return JPL_OK;
}

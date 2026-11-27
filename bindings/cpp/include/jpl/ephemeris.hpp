/* Copyright (c) 2026 Todd Carnes
 * SPDX-License-Identifier: MIT
 * See LICENSE in the project root for the full license text.
 */
#pragma once
#include <jpl.h>
#include <jpl_frames.h>
#include <jpl_convenience.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <array>
#include <optional>

namespace jpl {

class JplError : public std::runtime_error {
public:
    explicit JplError(jpl_status status)
        : std::runtime_error(::jpl_status_string(status)), status_(status) {}
    [[nodiscard]] jpl_status status() const noexcept { return status_; }
private:
    jpl_status status_;
};

inline void throw_if_error(jpl_status status) {
    if (status != JPL_OK) { throw JplError(status); }
}

class Ephemeris {
public:
    explicit Ephemeris(std::string_view path) {
        jpl_ephemeris* raw = nullptr;
        throw_if_error(::jpl_open(std::string(path).c_str(), &raw));
        handle_ = raw;
    }

    static Ephemeris open_mmap(std::string_view path) {
        jpl_ephemeris* raw = nullptr;
        throw_if_error(::jpl_open_mmap(std::string(path).c_str(), &raw));
        Ephemeris e;
        e.handle_ = raw;
        return e;
    }

    ~Ephemeris() { if (handle_ != nullptr) { ::jpl_close(handle_); } }

    Ephemeris(const Ephemeris&) = delete;
    Ephemeris& operator=(const Ephemeris&) = delete;

    Ephemeris(Ephemeris&& other) noexcept : handle_(other.handle_) { other.handle_ = nullptr; }
    Ephemeris& operator=(Ephemeris&& other) noexcept {
        if (this != &other) {
            if (handle_ != nullptr) { ::jpl_close(handle_); }
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }

    [[nodiscard]] jpl_ephemeris* native_handle() const noexcept { return handle_; }

    struct Header {
        std::array<std::string, 3> titles;
        std::vector<std::string> constant_names;
        std::array<double, 2> start_jd;
        std::array<double, 2> end_jd;
        double step_days;
        int constant_count;
        double astronomical_unit_km;
        double earth_moon_mass_ratio;
        struct Pointer {
            int start_index;
            int coefficients_per_component;
            int subintervals;
        };
        std::array<Pointer, 12> pointers;
        int ephemeris_number;
        jpl_reference_frame reference_frame;
        Pointer libration_pointer;
        Pointer tt_tdb_pointer;
        int has_tt_tdb;
        size_t record_double_count;
        jpl_byte_order byte_order;
    };

    [[nodiscard]] Header header() const {
        const jpl_header* c_hdr = ::jpl_get_header(handle_);
        if (c_hdr == nullptr) { throw JplError(JPL_ERROR_ARGUMENT); }
        Header h;
        for (int i = 0; i < 3; ++i) h.titles[i] = c_hdr->titles[i];
        h.constant_names.reserve(static_cast<size_t>(c_hdr->constant_count));
        for (int i = 0; i < c_hdr->constant_count; ++i) {
            if (i < JPL_MAX_CONSTANT_NAMES) {
                h.constant_names.emplace_back(c_hdr->constant_names[i]);
            } else {
                char name[JPL_CONSTANT_NAME_LENGTH + 1];
                throw_if_error(::jpl_get_constant_name(handle_, static_cast<size_t>(i), name));
                h.constant_names.emplace_back(name);
            }
        }
        h.start_jd = {c_hdr->start_jd[0], c_hdr->start_jd[1]};
        h.end_jd = {c_hdr->end_jd[0], c_hdr->end_jd[1]};
        h.step_days = c_hdr->step_days;
        h.constant_count = c_hdr->constant_count;
        h.astronomical_unit_km = c_hdr->astronomical_unit_km;
        h.earth_moon_mass_ratio = c_hdr->earth_moon_mass_ratio;
        for (int i = 0; i < 12; ++i) {
            h.pointers[i] = {c_hdr->pointers[i].start_index,
                             c_hdr->pointers[i].coefficients_per_component,
                             c_hdr->pointers[i].subintervals};
        }
        h.ephemeris_number = c_hdr->ephemeris_number;
        h.reference_frame = c_hdr->reference_frame;
        h.libration_pointer = {c_hdr->libration_pointer.start_index,
                               c_hdr->libration_pointer.coefficients_per_component,
                               c_hdr->libration_pointer.subintervals};
        h.tt_tdb_pointer = {c_hdr->tt_tdb_pointer.start_index,
                            c_hdr->tt_tdb_pointer.coefficients_per_component,
                            c_hdr->tt_tdb_pointer.subintervals};
        h.has_tt_tdb = c_hdr->has_tt_tdb;
        h.record_double_count = c_hdr->record_double_count;
        h.byte_order = c_hdr->byte_order;
        return h;
    }

    [[nodiscard]] std::vector<double> constants() const {
        const jpl_header* c_hdr = ::jpl_get_header(handle_);
        if (c_hdr == nullptr) { throw JplError(JPL_ERROR_ARGUMENT); }
        std::vector<double> vals(static_cast<size_t>(c_hdr->constant_count));
        throw_if_error(::jpl_read_constants(handle_, vals.data(), vals.size()));
        return vals;
    }

    [[nodiscard]] std::string constant_name(size_t index) const {
        char name[JPL_CONSTANT_NAME_LENGTH + 1];
        throw_if_error(::jpl_get_constant_name(handle_, index, name));
        return std::string(name);
    }

    [[nodiscard]] std::vector<double> read_record(size_t record_index) const {
        const jpl_header* c_hdr = ::jpl_get_header(handle_);
        if (c_hdr == nullptr) { throw JplError(JPL_ERROR_ARGUMENT); }
        std::vector<double> vals(c_hdr->record_double_count);
        throw_if_error(::jpl_read_record(handle_, record_index, vals.data(), vals.size()));
        return vals;
    }

    [[nodiscard]] size_t record_count() const {
        if (handle_ == nullptr) { throw JplError(JPL_ERROR_ARGUMENT); }
        return ::jpl_record_count(handle_);
    }

    struct TT_TDB_Result {
        double seconds;
        double rate_per_day;
    };

    [[nodiscard]] TT_TDB_Result tt_minus_tdb(const std::array<double, 2>& jd_tdb) const {
        jpl_date jd{jd_tdb[0], jd_tdb[1]};
        double sec, rate;
        throw_if_error(::jpl_get_tt_minus_tdb(handle_, &jd, &sec, &rate));
        return {sec, rate};
    }

    [[nodiscard]] bool has_tt_tdb() const {
        if (handle_ == nullptr) { throw JplError(JPL_ERROR_ARGUMENT); }
        return ::jpl_has_tt_tdb(handle_) != 0;
    }

    [[nodiscard]] std::array<double, 6> get_state(const std::array<double, 2>& jd_tdb,
                                                  int target, int center) const {
        jpl_date jd{jd_tdb[0], jd_tdb[1]};
        jpl_state state;
        throw_if_error(::jpl_get_state(handle_, &jd, target, center, &state));
        return {state.x, state.y, state.z, state.vx, state.vy, state.vz};
    }

    struct LightTimeResult {
        std::array<double, 6> state;
        double light_time_days;
    };

    [[nodiscard]] LightTimeResult get_state_lighttime(const std::array<double, 2>& jd_tdb,
                                                      int target, int center) const {
        jpl_date jd{jd_tdb[0], jd_tdb[1]};
        jpl_state state;
        double lt;
        throw_if_error(::jpl_get_state_lighttime(handle_, &jd, target, center, &state, &lt));
        return {{state.x, state.y, state.z, state.vx, state.vy, state.vz}, lt};
    }

    [[nodiscard]] static std::array<double, 6> apply_aberration(const std::array<double, 6>& state,
                                                                 const std::array<double, 3>& observer_vel) {
        jpl_state in{state[0], state[1], state[2], state[3], state[4], state[5]};
        jpl_state out;
        /* Sun distance 0 omits the solar term. */
        throw_if_error(::jpl_apply_aberration(&in, observer_vel.data(), 0.0, &out));
        return {out.x, out.y, out.z, out.vx, out.vy, out.vz};
    }

    [[nodiscard]] static std::array<double, 6> apply_aberration_full(const std::array<double, 6>& state,
                                                                      const std::array<double, 3>& observer_vel,
                                                                      double sun_distance_au) {
        jpl_state in{state[0], state[1], state[2], state[3], state[4], state[5]};
        jpl_state out;
        throw_if_error(::jpl_apply_aberration(&in, observer_vel.data(), sun_distance_au, &out));
        return {out.x, out.y, out.z, out.vx, out.vy, out.vz};
    }

    Ephemeris clone() const {
        jpl_ephemeris* raw = nullptr;
        throw_if_error(::jpl_ephemeris_clone(handle_, &raw));
        Ephemeris e;
        e.handle_ = raw;
        return e;
    }

    // Frame conversions (via the cached frame context)
    [[nodiscard]] static std::array<double, 3> rotate_vector(const std::array<double, 3>& vec,
                                                             const std::array<double, 2>& jd_tt) {
        jpl_date jd{jd_tt[0], jd_tt[1]};
        jpl_frame_context* ctx = nullptr;
        throw_if_error(::jpl_frame_context_create(&jd, &ctx));
        double out[3];
        jpl_status status = ::jpl_frame_context_rotate_vector(ctx, vec.data(), out);
        ::jpl_frame_context_destroy(ctx);
        throw_if_error(status);
        return {out[0], out[1], out[2]};
    }

    [[nodiscard]] static std::array<double, 6> ecliptic_to_icrs(const std::array<double, 6>& state,
                                                                 const std::array<double, 2>& jd_tt) {
        jpl_date jd{jd_tt[0], jd_tt[1]};
        jpl_state in{state[0], state[1], state[2], state[3], state[4], state[5]};
        jpl_state out;
        jpl_frame_context* ctx = nullptr;
        throw_if_error(::jpl_frame_context_create(&jd, &ctx));
        jpl_status status = ::jpl_frame_context_rotate_state(ctx, &in, &out);
        ::jpl_frame_context_destroy(ctx);
        throw_if_error(status);
        return {out.x, out.y, out.z, out.vx, out.vy, out.vz};
    }

    [[nodiscard]] static std::array<double, 6> icrs_to_ecliptic(const std::array<double, 6>& state,
                                                                 const std::array<double, 2>& jd_tt) {
        jpl_state in{state[0], state[1], state[2], state[3], state[4], state[5]};
        jpl_date jd{jd_tt[0], jd_tt[1]};
        jpl_state out;
        throw_if_error(::jpl_icrs_to_ecliptic(&in, &jd, &out));
        return {out.x, out.y, out.z, out.vx, out.vy, out.vz};
    }

    [[nodiscard]] static std::array<std::array<double, 3>, 3> frame_rotation_matrix(const std::array<double, 2>& jd_tt) {
        jpl_date jd{jd_tt[0], jd_tt[1]};
        double R[3][3];
        throw_if_error(::jpl_frame_rotation_matrix(&jd, R));
        std::array<std::array<double, 3>, 3> result;
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                result[i][j] = R[i][j];
        return result;
    }

    [[nodiscard]] static std::array<double, 2> tt_to_tdb(const std::array<double, 2>& jd_tt) {
        jpl_date in{jd_tt[0], jd_tt[1]};
        jpl_date jd_tdb;
        throw_if_error(::jpl_tt_to_tdb(&in, &jd_tdb));
        return {jd_tdb.hi, jd_tdb.lo};
    }

    [[nodiscard]] static std::array<double, 2> tdb_to_tt(const std::array<double, 2>& jd_tdb) {
        jpl_date in{jd_tdb[0], jd_tdb[1]};
        jpl_date jd_tt;
        throw_if_error(::jpl_tdb_to_tt(&in, &jd_tt));
        return {jd_tt.hi, jd_tt.lo};
    }

    // Convenience functions
    [[nodiscard]] std::array<double, 6> planet_position(const std::array<double, 2>& jd_tt,
                                                        int planet, int center) const {
        jpl_date jd{jd_tt[0], jd_tt[1]};
        jpl_state state;
        throw_if_error(::jpl_planet_position(handle_, &jd, planet, center, &state));
        return {state.x, state.y, state.z, state.vx, state.vy, state.vz};
    }

    [[nodiscard]] std::array<double, 6> moon_position(const std::array<double, 2>& jd_tt) const {
        jpl_date jd{jd_tt[0], jd_tt[1]};
        jpl_state state;
        throw_if_error(::jpl_moon_position(handle_, &jd, &state));
        return {state.x, state.y, state.z, state.vx, state.vy, state.vz};
    }

    struct ApparentEclipticResult {
        double longitude_deg;
        double latitude_deg;
        double range_au;
        double longitude_rate_deg_per_day;
        double range_rate_km_per_s;
    };

    [[nodiscard]] ApparentEclipticResult apparent_ecliptic_position(const std::array<double, 2>& jd_tt, int target) const {
        jpl_date jd{jd_tt[0], jd_tt[1]};
        jpl_apparent_ecliptic_result result;
        throw_if_error(::jpl_apparent_ecliptic_position(handle_, &jd, target, &result));
        return {result.longitude_deg, result.latitude_deg, result.range_au,
                result.longitude_rate_deg_per_day, result.range_rate_km_per_s};
    }

    [[nodiscard]] static std::array<double, 3> au_per_day_to_km_per_s(const std::array<double, 3>& vel_au_day) {
        double out[3];
        throw_if_error(::jpl_au_per_day_to_km_per_s(vel_au_day.data(), out));
        return {out[0], out[1], out[2]};
    }

    [[nodiscard]] static double km_to_au(double km) {
        double au;
        throw_if_error(::jpl_km_to_au(km, &au));
        return au;
    }

private:
    Ephemeris() = default;
    explicit Ephemeris(jpl_ephemeris* h) : handle_(h) {}

    jpl_ephemeris* handle_ = nullptr;

    friend Ephemeris open(std::string_view path);
    friend Ephemeris open_mmap(std::string_view path);
};

inline Ephemeris open(std::string_view path) {
    return Ephemeris(path);
}

inline Ephemeris open_mmap(std::string_view path) {
    return Ephemeris::open_mmap(path);
}

[[nodiscard]] inline std::string status_string(jpl_status status) {
    return ::jpl_status_string(status);
}

}  // namespace jpl

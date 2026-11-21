# Copyright (c) 2026 Todd Carnes
# SPDX-License-Identifier: MIT
# See LICENSE in the project root for the full license text.

"""Raw cffi bindings over the libjpl shared library. Do not import directly; use libjpl.ephemeris."""
from cffi import FFI

ffi = FFI()
ffi.cdef("""
    typedef enum { JPL_OK=0, JPL_ERROR_ARGUMENT, JPL_ERROR_IO, JPL_ERROR_FORMAT,
        JPL_ERROR_RANGE, JPL_ERROR_MEMORY, JPL_ERROR_UNSUPPORTED,
        JPL_ERROR_CONVERGENCE, JPL_ERROR_NUMERIC } jpl_status;

    typedef enum { JPL_BYTE_ORDER_LITTLE_ENDIAN, JPL_BYTE_ORDER_BIG_ENDIAN } jpl_byte_order;

    typedef enum { JPL_MERCURY=1, JPL_VENUS=2, JPL_EARTH=3, JPL_MARS=4,
        JPL_JUPITER=5, JPL_SATURN=6, JPL_URANUS=7, JPL_NEPTUNE=8,
        JPL_PLUTO=9, JPL_MOON=10, JPL_SUN=11, JPL_SOLAR_SYSTEM_BARYCENTER=12,
        JPL_EARTH_MOON_BARYCENTER=13, JPL_NUTATIONS=14, JPL_LIBRATIONS=15,
        JPL_TT_TDB=17, JPL_SSB=0 } jpl_target;

    typedef enum { JPL_FRAME_UNKNOWN=0, JPL_FRAME_DYNAMICAL_1950=1,
        JPL_FRAME_DYNAMICAL_2000=2, JPL_FRAME_ICRF=3 } jpl_reference_frame;

    typedef struct { int start_index; int coefficients_per_component; int subintervals; } jpl_pointer;

    typedef struct {
        char titles[3][85];
        char constant_names[400][7];
        double start_jd[2];
        double end_jd[2];
        double step_days;
        int constant_count;
        double astronomical_unit_km;
        double earth_moon_mass_ratio;
        jpl_pointer pointers[12];
        int ephemeris_number;
        jpl_reference_frame reference_frame;
        jpl_pointer libration_pointer;
        jpl_pointer tt_tdb_pointer;
        int has_tt_tdb;
        size_t record_double_count;
        jpl_byte_order byte_order;
    } jpl_header;

    typedef struct jpl_ephemeris jpl_ephemeris;

    typedef struct {
        double hi;
        double lo;
    } jpl_date;

    typedef struct {
        double x;
        double y;
        double z;
        double vx;
        double vy;
        double vz;
    } jpl_state;


    typedef struct {
        double longitude_deg;
        double latitude_deg;
        double range_au;
        double longitude_rate_deg_per_day;
        double range_rate_km_per_s;
    } jpl_apparent_ecliptic_result;

    /* jpl.h - core API */
    const char *jpl_status_string(jpl_status status);
    jpl_status jpl_open(const char *path, jpl_ephemeris **out_ephemeris);
    void jpl_close(jpl_ephemeris *ephemeris);
    const jpl_header *jpl_get_header(const jpl_ephemeris *ephemeris);
    jpl_status jpl_read_constants(const jpl_ephemeris *ephemeris, double *values, size_t value_count);
    jpl_status jpl_get_constant_name(const jpl_ephemeris *ephemeris, size_t index,
                                     char name[7]);
    jpl_status jpl_read_record(const jpl_ephemeris *ephemeris, size_t record_index,
                               double *values, size_t value_count);
    size_t jpl_record_count(const jpl_ephemeris *ephemeris);
    jpl_status jpl_get_tt_minus_tdb(const jpl_ephemeris *ephemeris, const jpl_date *jd_tdb,
                                    double *seconds, double *rate_per_day);
    int jpl_has_tt_tdb(const jpl_ephemeris *ephemeris);
    jpl_status jpl_get_state(const jpl_ephemeris *ephemeris, const jpl_date *jd_tdb,
                             int target, int center, jpl_state *state);
    jpl_status jpl_get_state_lighttime(const jpl_ephemeris *ephemeris, const jpl_date *jd_tdb,
                                       int target, int center, jpl_state *state, double *out_light_time);
    jpl_status jpl_apply_aberration(const jpl_state *state, const double observer_vel[3],
                                    double sun_distance_au, jpl_state *out_state);
    jpl_status jpl_open_mmap(const char *path, jpl_ephemeris **out_ephemeris);
    jpl_status jpl_ephemeris_clone(const jpl_ephemeris *ephem, jpl_ephemeris **out_clone);

    /* jpl_frames.h - frame conversions and time conversions.
     * ecliptic_to_icrs intentionally absent: callers use the cached
     * frame-context API below (same transform, no duplicate entry). */
    jpl_status jpl_icrs_to_ecliptic(const jpl_state *state, const jpl_date *jd_tt, jpl_state *out_state);
    jpl_status jpl_frame_rotation_matrix(const jpl_date *jd_tt, double R[3][3]);
    jpl_status jpl_tt_to_tdb(const jpl_date *jd_tt, jpl_date *jd_tdb);
    jpl_status jpl_tdb_to_tt(const jpl_date *jd_tdb, jpl_date *jd_tt);
    typedef struct jpl_frame_context jpl_frame_context;
    jpl_status jpl_frame_context_create(const jpl_date *jd_tt, jpl_frame_context **out_context);
    void jpl_frame_context_destroy(jpl_frame_context *context);
    jpl_status jpl_frame_context_rotate_state(const jpl_frame_context *context, const jpl_state *state, jpl_state *out_state);

    /* jpl_convenience.h - planet/Moon helpers and unit conversions */
    jpl_status jpl_planet_position(const jpl_ephemeris *ephem, const jpl_date *jd_tt, int planet, int center, jpl_state *state);
    jpl_status jpl_moon_position(const jpl_ephemeris *ephem, const jpl_date *jd_tt, jpl_state *state);
    jpl_status jpl_apparent_ecliptic_position(const jpl_ephemeris *ephem,
                                              const jpl_date *jd_tt, int target,
                                              jpl_apparent_ecliptic_result *out);
    jpl_status jpl_au_per_day_to_km_per_s(const double vel_au_day[3], double vel_km_s[3]);
    jpl_status jpl_km_to_au(double km, double *au);
""")

_lib = None
_load_error = None
try:
    _lib = ffi.dlopen("jpl")
except OSError as exc:
    _load_error = exc

if _lib is None:
    raise ImportError(
        "could not load the libjpl shared library; build with "
        "-DLIBJPL_BUILD_SHARED=ON and put jpl.dll / libjpl.so on the "
        "library search path") from _load_error

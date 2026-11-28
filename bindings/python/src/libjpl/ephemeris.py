# Copyright (c) 2026 Todd Carnes
# SPDX-License-Identifier: MIT
# See LICENSE in the project root for the full license text.

"""Pythonic wrapper over the raw cffi bindings."""
from . import _libjpl

_ffi = _libjpl.ffi
_lib = _libjpl._lib


class JplError(Exception):
    """libjpl failure carrying the numeric jpl_status value."""

    def __init__(self, status):
        super().__init__(_ffi.string(_lib.jpl_status_string(status)).decode())
        self.status = int(status)


def _check(status):
    if status != _lib.JPL_OK:
        raise JplError(status)


def _jpl_date(jd):
    """Build a jpl_date cdata from a [hi, lo] pair."""
    return _ffi.new("jpl_date *", {'hi': float(jd[0]), 'lo': float(jd[1])})


def _jpl_state(state):
    """Build a jpl_state cdata from a [x, y, z, vx, vy, vz] sequence."""
    return _ffi.new("jpl_state *", {'x': float(state[0]), 'y': float(state[1]),
                                    'z': float(state[2]), 'vx': float(state[3]),
                                    'vy': float(state[4]), 'vz': float(state[5])})


def _state_list(state):
    """Read a jpl_state cdata back into a Python list."""
    return [float(state.x), float(state.y), float(state.z),
            float(state.vx), float(state.vy), float(state.vz)]


# Type constants for convenience
class JplStatus:
    OK = 0
    ERROR_ARGUMENT = 1
    ERROR_IO = 2
    ERROR_FORMAT = 3
    ERROR_RANGE = 4
    ERROR_MEMORY = 5
    ERROR_UNSUPPORTED = 6
    ERROR_CONVERGENCE = 7
    ERROR_NUMERIC = 8


class JplTarget:
    MERCURY = 1
    VENUS = 2
    EARTH = 3
    MARS = 4
    JUPITER = 5
    SATURN = 6
    URANUS = 7
    NEPTUNE = 8
    PLUTO = 9
    MOON = 10
    SUN = 11
    SOLAR_SYSTEM_BARYCENTER = 12
    EARTH_MOON_BARYCENTER = 13
    NUTATIONS = 14
    LIBRATIONS = 15
    TT_TDB = 17
    SSB = 0


class JplByteOrder:
    LITTLE_ENDIAN = 0
    BIG_ENDIAN = 1


class JplReferenceFrame:
    UNKNOWN = 0
    DYNAMICAL_1950 = 1
    DYNAMICAL_2000 = 2
    ICRF = 3


class JplPointer:
    """Chebyshev coefficient layout for a target."""
    def __init__(self, start_index, coefficients_per_component, subintervals):
        self.start_index = start_index
        self.coefficients_per_component = coefficients_per_component
        self.subintervals = subintervals


class JplHeader:
    """Parsed DE header snapshot built from the native accessors (safe after close)."""
    def __init__(self, handle):
        titles = _ffi.new("char[3][85]")
        _check(_lib.jpl_header_titles(handle, titles))
        self._titles = [_ffi.string(titles[i]).decode().rstrip('\x00') for i in range(3)]

        start = _ffi.new("jpl_date *")
        end = _ffi.new("jpl_date *")
        step = _ffi.new("double *")
        _check(_lib.jpl_header_coverage(handle, start, end, step))
        self._start_jd = [float(start.hi), float(start.lo)]
        self._end_jd = [float(end.hi), float(end.lo)]
        self._step_days = float(step[0])

        constant_count = _ffi.new("int *")
        record_double_count = _ffi.new("size_t *")
        byte_order = _ffi.new("jpl_byte_order *")
        _check(_lib.jpl_header_shape(handle, constant_count, record_double_count, byte_order))
        self._constant_count = int(constant_count[0])
        self._record_double_count = int(record_double_count[0])
        self._byte_order = int(byte_order[0])

        ephemeris_number = _ffi.new("int *")
        reference_frame = _ffi.new("jpl_reference_frame *")
        _check(_lib.jpl_header_identity(handle, ephemeris_number, reference_frame))
        self._ephemeris_number = int(ephemeris_number[0])
        self._reference_frame = int(reference_frame[0])

        au = _ffi.new("double *")
        emrat = _ffi.new("double *")
        has_tt_tdb = _ffi.new("int *")
        _check(_lib.jpl_header_scalars(handle, au, emrat, has_tt_tdb))
        self._astronomical_unit_km = float(au[0])
        self._earth_moon_mass_ratio = float(emrat[0])
        self._has_tt_tdb = bool(has_tt_tdb[0])

        self._pointers = []
        for i in range(12):
            p = _ffi.new("jpl_pointer *")
            _check(_lib.jpl_header_pointer(handle, i, p))
            self._pointers.append(JplPointer(p.start_index,
                                             p.coefficients_per_component,
                                             p.subintervals))
        p = _ffi.new("jpl_pointer *")
        _check(_lib.jpl_header_libration_pointer(handle, p))
        self._libration_pointer = JplPointer(p.start_index, p.coefficients_per_component,
                                             p.subintervals)
        p = _ffi.new("jpl_pointer *")
        _check(_lib.jpl_header_tt_tdb_pointer(handle, p))
        self._tt_tdb_pointer = JplPointer(p.start_index, p.coefficients_per_component,
                                          p.subintervals)

        names = []
        name_buf = _ffi.new("char[7]")
        for i in range(self._constant_count):
            _check(_lib.jpl_get_constant_name(handle, i, name_buf))
            names.append(_ffi.string(name_buf).decode())
        self._constant_names = names

    @property
    def titles(self):
        return list(self._titles)

    @property
    def constant_names(self):
        return list(self._constant_names)

    @property
    def start_jd(self):
        return list(self._start_jd)

    @property
    def end_jd(self):
        return list(self._end_jd)

    @property
    def step_days(self):
        return self._step_days

    @property
    def constant_count(self):
        return self._constant_count

    @property
    def astronomical_unit_km(self):
        return self._astronomical_unit_km

    @property
    def earth_moon_mass_ratio(self):
        return self._earth_moon_mass_ratio

    @property
    def pointers(self):
        return list(self._pointers)

    @property
    def ephemeris_number(self):
        return self._ephemeris_number

    @property
    def reference_frame(self):
        return self._reference_frame

    @property
    def libration_pointer(self):
        return self._libration_pointer

    @property
    def tt_tdb_pointer(self):
        return self._tt_tdb_pointer

    @property
    def has_tt_tdb(self):
        return self._has_tt_tdb

    @property
    def record_double_count(self):
        return self._record_double_count

    @property
    def byte_order(self):
        return self._byte_order


class Ephemeris:
    """An open DE file. Use as a context manager; closes on exit."""

    def __init__(self, path):
        self._handle = _ffi.NULL
        handle = _ffi.new("jpl_ephemeris **")
        _check(_lib.jpl_open(path.encode("utf-8"), handle))
        self._handle = handle[0]

    def close(self):
        handle = getattr(self, "_handle", _ffi.NULL)
        if handle is not None and handle != _ffi.NULL:
            _lib.jpl_close(handle)
            self._handle = _ffi.NULL

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.close()

    def __del__(self):
        self.close()

    @property
    def header(self):
        return JplHeader(self._handle)

    def constants(self):
        """Read all constants."""
        constant_count = _ffi.new("int *")
        record_double_count = _ffi.new("size_t *")
        byte_order = _ffi.new("jpl_byte_order *")
        _check(_lib.jpl_header_shape(self._handle, constant_count,
                                     record_double_count, byte_order))
        count = int(constant_count[0])
        vals = _ffi.new(f"double[{count}]")
        _check(_lib.jpl_read_constants(self._handle, vals, count))
        return [float(vals[i]) for i in range(count)]

    def constant_name(self, index):
        """Get constant name by index."""
        name_buf = _ffi.new("char[7]")
        _check(_lib.jpl_get_constant_name(self._handle, index, name_buf))
        return _ffi.string(name_buf).decode()

    def read_record(self, record_index):
        """Read a complete coefficient record."""
        constant_count = _ffi.new("int *")
        record_double_count = _ffi.new("size_t *")
        byte_order = _ffi.new("jpl_byte_order *")
        _check(_lib.jpl_header_shape(self._handle, constant_count,
                                     record_double_count, byte_order))
        count = int(record_double_count[0])
        vals = _ffi.new(f"double[{count}]")
        _check(_lib.jpl_read_record(self._handle, record_index, vals, count))
        return [float(vals[i]) for i in range(count)]

    def record_count(self):
        if self._handle == _ffi.NULL:
            raise JplError(JplStatus.ERROR_ARGUMENT)
        return _lib.jpl_record_count(self._handle)

    def tt_minus_tdb(self, jd_tdb):
        """Get TT-TDB at TDB Julian date. Returns (seconds, rate_per_day)."""
        jd = _jpl_date(jd_tdb)
        sec = _ffi.new("double *")
        rate = _ffi.new("double *")
        _check(_lib.jpl_get_tt_minus_tdb(self._handle, jd, sec, rate))
        return (float(sec[0]), float(rate[0]))

    def has_tt_tdb(self):
        if self._handle == _ffi.NULL:
            raise JplError(JplStatus.ERROR_ARGUMENT)
        return bool(_lib.jpl_has_tt_tdb(self._handle))

    def get_state(self, jd_tdb, target, center):
        """Get state vector for target at jd_tdb relative to center."""
        jd = _jpl_date(jd_tdb)
        out = _ffi.new("jpl_state *")
        _check(_lib.jpl_get_state(self._handle, jd, int(target), int(center), out))
        return _state_list(out)

    def get_state_lighttime(self, jd_tdb, target, center):
        """Get light-time-corrected state. Returns (state, light_time_days)."""
        jd = _jpl_date(jd_tdb)
        out = _ffi.new("jpl_state *")
        lt = _ffi.new("double *")
        _check(_lib.jpl_get_state_lighttime(self._handle, jd, int(target), int(center), out, lt))
        return (_state_list(out), float(lt[0]))

    def lighttime_solution(self, jd_tdb, target, center):
        """Get the atomic light-time solution.

        Returns a dict with rel_state, observer_state, light_time_days,
        and emission ([hi, lo] TDB epoch used for rel_state).
        """
        jd = _jpl_date(jd_tdb)
        out = _ffi.new("jpl_lighttime_solution *")
        _check(_lib.jpl_get_lighttime_solution(self._handle, jd, int(target), int(center), out))
        return {
            'rel_state': _state_list(out.rel_state),
            'observer_state': _state_list(out.observer_state),
            'light_time_days': float(out.light_time_days),
            'emission': [float(out.emission.hi), float(out.emission.lo)],
        }

    @staticmethod
    def apply_aberration(state, observer_vel):
        """Apply stellar aberration (without solar term). Returns aberrated state."""
        in_state = _jpl_state(state)
        in_vel = _ffi.new("double[3]", [float(x) for x in observer_vel])
        out = _ffi.new("jpl_state *")
        _check(_lib.jpl_apply_aberration(in_state, in_vel, 0.0, out))
        return _state_list(out)

    @staticmethod
    def apply_aberration_full(state, observer_vel, sun_distance_au):
        """Apply full stellar aberration with solar term. Returns aberrated state."""
        in_state = _jpl_state(state)
        in_vel = _ffi.new("double[3]", [float(x) for x in observer_vel])
        out = _ffi.new("jpl_state *")
        _check(_lib.jpl_apply_aberration(in_state, in_vel, float(sun_distance_au), out))
        return _state_list(out)

    @staticmethod
    def open_mmap(path):
        """Open a DE file via memory mapping. Returns Ephemeris instance."""
        handle = _ffi.new("jpl_ephemeris **")
        _check(_lib.jpl_open_mmap(path.encode("utf-8"), handle))
        eph = Ephemeris.__new__(Ephemeris)
        eph._handle = handle[0]
        return eph

    def clone(self):
        """Clone this handle for concurrent read access."""
        handle = _ffi.new("jpl_ephemeris **")
        _check(_lib.jpl_ephemeris_clone(self._handle, handle))
        eph = Ephemeris.__new__(Ephemeris)
        eph._handle = handle[0]
        return eph

    # Frame conversions
    @staticmethod
    def rotate_vector(vec, jd_tt):
        """Rotate a mean-ecliptic-of-date vector to ICRS (via a cached frame context)."""
        in_vec = _ffi.new("double[3]", [float(v) for v in vec])
        jd = _jpl_date(jd_tt)
        out = _ffi.new("double[3]")
        ctx = _ffi.new("jpl_frame_context **")
        _check(_lib.jpl_frame_context_create(jd, ctx))
        try:
            _check(_lib.jpl_frame_context_rotate_vector(ctx[0], in_vec, out))
        finally:
            _lib.jpl_frame_context_destroy(ctx[0])
        return [float(out[i]) for i in range(3)]

    @staticmethod
    def ecliptic_to_icrs(state, jd_tt):
        """Rotate ecliptic-of-date state to ICRS (via a cached frame context)."""
        in_state = _jpl_state(state)
        jd = _jpl_date(jd_tt)
        out = _ffi.new("jpl_state *")
        ctx = _ffi.new("jpl_frame_context **")
        _check(_lib.jpl_frame_context_create(jd, ctx))
        try:
            _check(_lib.jpl_frame_context_rotate_state(ctx[0], in_state, out))
        finally:
            _lib.jpl_frame_context_destroy(ctx[0])
        return _state_list(out)

    @staticmethod
    def icrs_to_ecliptic(state, jd_tt):
        """Rotate ICRS state to ecliptic of date."""
        in_state = _jpl_state(state)
        jd = _jpl_date(jd_tt)
        out = _ffi.new("jpl_state *")
        _check(_lib.jpl_icrs_to_ecliptic(in_state, jd, out))
        return _state_list(out)

    @staticmethod
    def frame_rotation_matrix(jd_tt):
        """Get ecliptic-of-date to ICRS rotation matrix."""
        jd = _jpl_date(jd_tt)
        R = _ffi.new("double[3][3]")
        _check(_lib.jpl_frame_rotation_matrix(jd, R))
        return [[float(R[i][j]) for j in range(3)] for i in range(3)]

    @staticmethod
    def tt_to_tdb(jd_tt):
        """Convert TT Julian date to TDB."""
        jd_tt_in = _jpl_date(jd_tt)
        jd_tdb = _ffi.new("jpl_date *")
        _check(_lib.jpl_tt_to_tdb(jd_tt_in, jd_tdb))
        return [float(jd_tdb.hi), float(jd_tdb.lo)]

    @staticmethod
    def tdb_to_tt(jd_tdb):
        """Convert TDB Julian date to TT."""
        jd_tdb_in = _jpl_date(jd_tdb)
        jd_tt = _ffi.new("jpl_date *")
        _check(_lib.jpl_tdb_to_tt(jd_tdb_in, jd_tt))
        return [float(jd_tt.hi), float(jd_tt.lo)]

    # Convenience functions
    def planet_position(self, jd_tt, planet, center):
        """Apparent planet position (light-time + aberration, ICRS)."""
        jd = _jpl_date(jd_tt)
        out = _ffi.new("jpl_state *")
        _check(_lib.jpl_planet_position(self._handle, jd, int(planet), int(center), out))
        return _state_list(out)

    def moon_position(self, jd_tt):
        """Apparent Moon position, Earth-centered ICRS."""
        jd = _jpl_date(jd_tt)
        out = _ffi.new("jpl_state *")
        _check(_lib.jpl_moon_position(self._handle, jd, out))
        return _state_list(out)

    def apparent_ecliptic_position(self, jd_tt, target):
        """Apparent geocentric true-ecliptic position and motion."""
        jd = _jpl_date(jd_tt)
        result = _ffi.new("jpl_apparent_ecliptic_result *")
        _check(_lib.jpl_apparent_ecliptic_position(self._handle, jd, int(target),
                                                   result))
        return {
            'longitude_deg': float(result.longitude_deg),
            'latitude_deg': float(result.latitude_deg),
            'range_au': float(result.range_au),
            'longitude_rate_deg_per_day': float(result.longitude_rate_deg_per_day),
            'range_rate_km_per_s': float(result.range_rate_km_per_s),
        }

    @staticmethod
    def au_per_day_to_km_per_s(vel_au_day):
        """Convert velocity from AU/day to km/s."""
        in_vel = _ffi.new("double[3]", [float(x) for x in vel_au_day])
        out = _ffi.new("double[3]")
        _check(_lib.jpl_au_per_day_to_km_per_s(in_vel, out))
        return [float(out[i]) for i in range(3)]

    @staticmethod
    def km_to_au(km):
        """Convert distance from km to AU."""
        au = _ffi.new("double *")
        _check(_lib.jpl_km_to_au(float(km), au))
        return float(au[0])

    def __repr__(self):
        return f"Ephemeris(handle={self._handle})"


def status_string(status):
    """Return human-readable status string."""
    return _ffi.string(_lib.jpl_status_string(int(status))).decode()


def open(path):
    """Open a DE file. Returns Ephemeris instance."""
    return Ephemeris(path)


def open_mmap(path):
    """Open a DE file via memory mapping. Returns Ephemeris instance."""
    return Ephemeris.open_mmap(path)


__all__ = [
    'JplError', 'JplStatus', 'JplTarget', 'JplByteOrder', 'JplReferenceFrame',
    'JplPointer', 'JplHeader', 'Ephemeris',
    'status_string', 'open', 'open_mmap',
]
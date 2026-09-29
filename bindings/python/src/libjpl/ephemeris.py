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
    """Parsed DE header."""
    def __init__(self, c_header):
        self._c = c_header

    @property
    def titles(self):
        return [_ffi.string(self._c.titles[i]).decode().rstrip('\x00') for i in range(3)]

    @property
    def constant_names(self):
        return [_ffi.string(self._c.constant_names[i]).decode().rstrip('\x00') for i in range(self._c.constant_count)]

    @property
    def start_jd(self):
        return [self._c.start_jd[0], self._c.start_jd[1]]

    @property
    def end_jd(self):
        return [self._c.end_jd[0], self._c.end_jd[1]]

    @property
    def step_days(self):
        return self._c.step_days

    @property
    def constant_count(self):
        return self._c.constant_count

    @property
    def astronomical_unit_km(self):
        return self._c.astronomical_unit_km

    @property
    def earth_moon_mass_ratio(self):
        return self._c.earth_moon_mass_ratio

    @property
    def pointers(self):
        return [JplPointer(self._c.pointers[i].start_index,
                          self._c.pointers[i].coefficients_per_component,
                          self._c.pointers[i].subintervals) for i in range(12)]

    @property
    def ephemeris_number(self):
        return self._c.ephemeris_number

    @property
    def reference_frame(self):
        return self._c.reference_frame

    @property
    def libration_pointer(self):
        lp = self._c.libration_pointer
        return JplPointer(lp.start_index, lp.coefficients_per_component, lp.subintervals)

    @property
    def tt_tdb_pointer(self):
        tp = self._c.tt_tdb_pointer
        return JplPointer(tp.start_index, tp.coefficients_per_component, tp.subintervals)

    @property
    def has_tt_tdb(self):
        return bool(self._c.has_tt_tdb)

    @property
    def record_double_count(self):
        return self._c.record_double_count

    @property
    def byte_order(self):
        return self._c.byte_order


class Ephemeris:
    """An open DE file. Use as a context manager; closes on exit."""

    def __init__(self, path):
        handle = _ffi.new("jpl_ephemeris **")
        _check(_lib.jpl_open(path.encode("utf-8"), handle))
        self._handle = handle[0]

    def close(self):
        if self._handle is not None:
            _lib.jpl_close(self._handle)
            self._handle = None

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.close()

    def __del__(self):
        self.close()

    @property
    def header(self):
        c_header = _lib.jpl_get_header(self._handle)
        if c_header == _ffi.NULL:
            raise JplError(JplStatus.ERROR_ARGUMENT)
        return JplHeader(c_header)

    def constants(self):
        """Read all constants."""
        hdr = self.header
        vals = _ffi.new(f"double[{hdr.constant_count}]")
        _check(_lib.jpl_read_constants(self._handle, vals, hdr.constant_count))
        return [float(vals[i]) for i in range(hdr.constant_count)]

    def constant_name(self, index):
        """Get constant name by index."""
        name_buf = _ffi.new("char[7]")
        _check(_lib.jpl_get_constant_name(self._handle, index, name_buf))
        return _ffi.string(name_buf).decode()

    def read_record(self, record_index):
        """Read a complete coefficient record."""
        hdr = self.header
        vals = _ffi.new(f"double[{hdr.record_double_count}]")
        _check(_lib.jpl_read_record(self._handle, record_index, vals, hdr.record_double_count))
        return [float(vals[i]) for i in range(hdr.record_double_count)]

    def record_count(self):
        return _lib.jpl_record_count(self._handle)

    def tt_minus_tdb(self, jd_tdb):
        """Get TT-TDB at TDB Julian date. Returns (seconds, rate_per_day)."""
        jd = _ffi.new("double[2]", [float(jd_tdb[0]), float(jd_tdb[1])])
        sec = _ffi.new("double *")
        rate = _ffi.new("double *")
        _check(_lib.jpl_get_tt_minus_tdb(self._handle, jd, sec, rate))
        return (float(sec[0]), float(rate[0]))

    def has_tt_tdb(self):
        return bool(_lib.jpl_has_tt_tdb(self._handle))

    def get_state(self, jd_tdb, target, center):
        """Get state vector for target at jd_tdb relative to center."""
        jd = _ffi.new("double[2]", [float(jd_tdb[0]), float(jd_tdb[1])])
        out = _ffi.new("double[6]")
        _check(_lib.jpl_get_state(self._handle, jd, int(target), int(center), out))
        return [float(out[i]) for i in range(6)]

    def get_state_lighttime(self, jd_tdb, target, center):
        """Get light-time-corrected state. Returns (state, light_time_days)."""
        jd = _ffi.new("double[2]", [float(jd_tdb[0]), float(jd_tdb[1])])
        out = _ffi.new("double[6]")
        lt = _ffi.new("double *")
        _check(_lib.jpl_get_state_lighttime(self._handle, jd, int(target), int(center), out, lt))
        return ([float(out[i]) for i in range(6)], float(lt[0]))

    def apply_aberration(self, state, observer_vel):
        """Apply stellar aberration (without solar term). Returns aberrated state."""
        in_state = _ffi.new("double[6]", [float(x) for x in state])
        in_vel = _ffi.new("double[3]", [float(x) for x in observer_vel])
        out = _ffi.new("double[6]")
        _check(_lib.jpl_apply_aberration(in_state, in_vel, out))
        return [float(out[i]) for i in range(6)]

    def apply_aberration_full(self, state, observer_vel, sun_distance_au):
        """Apply full stellar aberration with solar term. Returns aberrated state."""
        in_state = _ffi.new("double[6]", [float(x) for x in state])
        in_vel = _ffi.new("double[3]", [float(x) for x in observer_vel])
        out = _ffi.new("double[6]")
        _check(_lib.jpl_apply_aberration_full(in_state, in_vel, float(sun_distance_au), out))
        return [float(out[i]) for i in range(6)]

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
    def ecliptic_to_icrs(state, jd_tt):
        """Rotate ecliptic-of-date state to ICRS."""
        in_state = _ffi.new("double[6]", [float(x) for x in state])
        jd = _ffi.new("double[2]", [float(jd_tt[0]), float(jd_tt[1])])
        out = _ffi.new("double[6]")
        _check(_lib.jpl_ecliptic_to_icrs(in_state, jd, out))
        return [float(out[i]) for i in range(6)]

    @staticmethod
    def icrs_to_ecliptic(state, jd_tt):
        """Rotate ICRS state to ecliptic of date."""
        in_state = _ffi.new("double[6]", [float(x) for x in state])
        jd = _ffi.new("double[2]", [float(jd_tt[0]), float(jd_tt[1])])
        out = _ffi.new("double[6]")
        _check(_lib.jpl_icrs_to_ecliptic(in_state, jd, out))
        return [float(out[i]) for i in range(6)]

    @staticmethod
    def frame_rotation_matrix(jd_tt):
        """Get ecliptic-of-date to ICRS rotation matrix."""
        jd = _ffi.new("double[2]", [float(jd_tt[0]), float(jd_tt[1])])
        R = _ffi.new("double[3][3]")
        _check(_lib.jpl_frame_rotation_matrix(jd, R))
        return [[float(R[i][j]) for j in range(3)] for i in range(3)]

    @staticmethod
    def tt_to_tdb(jd_tt):
        """Convert TT Julian date to TDB."""
        jd_tt_arr = _ffi.new("double[2]", [float(jd_tt[0]), float(jd_tt[1])])
        jd_tdb = _ffi.new("double[2]")
        _check(_lib.jpl_tt_to_tdb(jd_tt_arr, jd_tdb))
        return [float(jd_tdb[0]), float(jd_tdb[1])]

    @staticmethod
    def tdb_to_tt(jd_tdb):
        """Convert TDB Julian date to TT."""
        jd_tdb_arr = _ffi.new("double[2]", [float(jd_tdb[0]), float(jd_tdb[1])])
        jd_tt = _ffi.new("double[2]")
        _check(_lib.jpl_tdb_to_tt(jd_tdb_arr, jd_tt))
        return [float(jd_tt[0]), float(jd_tt[1])]

    # Convenience functions
    def planet_position(self, jd_tt, planet, center):
        """Apparent planet position (light-time + aberration, ICRS)."""
        jd = _ffi.new("double[2]", [float(jd_tt[0]), float(jd_tt[1])])
        out = _ffi.new("double[6]")
        _check(_lib.jpl_planet_position(self._handle, jd, int(planet), int(center), out))
        return [float(out[i]) for i in range(6)]

    def moon_position(self, jd_tt):
        """Apparent Moon position, Earth-centered ICRS."""
        jd = _ffi.new("double[2]", [float(jd_tt[0]), float(jd_tt[1])])
        out = _ffi.new("double[6]")
        _check(_lib.jpl_moon_position(self._handle, jd, out))
        return [float(out[i]) for i in range(6)]

    def apparent_ecliptic_position(self, jd_tt, target):
        """Apparent geocentric true-ecliptic position and motion."""
        jd = _ffi.new("double[2]", [float(jd_tt[0]), float(jd_tt[1])])
        lon = _ffi.new("double *")
        lat = _ffi.new("double *")
        rng = _ffi.new("double *")
        lon_rate = _ffi.new("double *")
        rng_rate = _ffi.new("double *")
        _check(_lib.jpl_apparent_ecliptic_position(self._handle, jd, int(target),
                                                  lon, lat, rng, lon_rate, rng_rate))
        return {
            'longitude_deg': float(lon[0]),
            'latitude_deg': float(lat[0]),
            'range_au': float(rng[0]),
            'longitude_rate_deg_per_day': float(lon_rate[0]),
            'range_rate_km_per_s': float(rng_rate[0]),
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
    'JplPointer', 'JplHeader', 'Ephemeris', 'JplError',
    'status_string', 'open', 'open_mmap',
]
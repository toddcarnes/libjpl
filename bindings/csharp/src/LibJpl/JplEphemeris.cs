/* Copyright (c) 2026 Todd Carnes
 * SPDX-License-Identifier: MIT
 * See LICENSE in the project root for the full license text.
 */
using System;
using System.Runtime.InteropServices;

namespace LibJpl;

public sealed class JplEphemeris : SafeHandle
{
    public JplEphemeris() : base(IntPtr.Zero, true) { }

    public override bool IsInvalid => handle == IntPtr.Zero;

    protected override bool ReleaseHandle()
    {
        if (!IsInvalid)
        {
            Native.jpl_close(handle);
            handle = IntPtr.Zero;
        }
        return true;
    }

    public static JplEphemeris Open(string path)
    {
        if (path == null)
            throw new ArgumentNullException(nameof(path));

        JplStatus status = Native.jpl_open(path, out IntPtr handle);
        if (status != JplStatus.Ok)
        {
            throw new JplException(status);
        }
        return new JplEphemeris(handle);
    }

    public static JplEphemeris OpenMmap(string path)
    {
        if (path == null)
            throw new ArgumentNullException(nameof(path));

        JplStatus status = Native.jpl_open_mmap(path, out IntPtr handle);
        if (status != JplStatus.Ok)
        {
            throw new JplException(status);
        }
        return new JplEphemeris(handle);
    }

    internal JplEphemeris(IntPtr handle) : base(IntPtr.Zero, true)
    {
        SetHandle(handle);
    }

    public void Clone(out JplEphemeris clone)
    {
        if (IsInvalid)
            throw new ObjectDisposedException(nameof(JplEphemeris));

        JplStatus status = Native.jpl_ephemeris_clone(handle, out IntPtr cloneHandle);
        if (status != JplStatus.Ok)
        {
            throw new JplException(status);
        }
        clone = new JplEphemeris(cloneHandle);
    }

    public JplHeader GetHeader()
    {
        if (IsInvalid)
            throw new ObjectDisposedException(nameof(JplEphemeris));

        IntPtr headerPtr = Native.jpl_get_header(handle);
        if (headerPtr == IntPtr.Zero)
            throw new JplException(JplStatus.ErrorArgument);

        return JplHeader.FromNative(headerPtr);
    }

    public double[] ReadConstants()
    {
        if (IsInvalid)
            throw new ObjectDisposedException(nameof(JplEphemeris));

        var header = GetHeader();
        double[] values = new double[header.ConstantCount];
        JplStatus status = Native.jpl_read_constants(handle, values, values.Length);
        if (status != JplStatus.Ok)
        {
            throw new JplException(status);
        }
        return values;
    }

    public string GetConstantName(long index)
    {
        if (IsInvalid)
            throw new ObjectDisposedException(nameof(JplEphemeris));

        byte[] name = new byte[7];
        JplStatus status = Native.jpl_get_constant_name(handle, index, name);
        if (status != JplStatus.Ok)
        {
            throw new JplException(status);
        }
        return System.Text.Encoding.ASCII.GetString(name).TrimEnd('\0');
    }

    public double[] ReadRecord(long recordIndex)
    {
        if (IsInvalid)
            throw new ObjectDisposedException(nameof(JplEphemeris));

        var header = GetHeader();
        double[] values = new double[header.RecordDoubleCount];
        JplStatus status = Native.jpl_read_record(handle, recordIndex, values, values.Length);
        if (status != JplStatus.Ok)
        {
            throw new JplException(status);
        }
        return values;
    }

    public long RecordCount => IsInvalid ? 0 : Native.jpl_record_count(handle);

    public (double Seconds, double RatePerDay) GetTtMinusTdb(double[] jdTdb)
    {
        if (IsInvalid)
            throw new ObjectDisposedException(nameof(JplEphemeris));

        JplStatus status = Native.jpl_get_tt_minus_tdb(handle, jdTdb, out double seconds, out double rate);
        if (status != JplStatus.Ok)
        {
            throw new JplException(status);
        }
        return (seconds, rate);
    }

    public bool HasTtTdb => !IsInvalid && Native.jpl_has_tt_tdb(handle) != 0;

    public double[] GetState(double[] jdTdb, JplTarget target, JplTarget center)
    {
        if (IsInvalid)
            throw new ObjectDisposedException(nameof(JplEphemeris));

        double[] state = new double[6];
        JplStatus status = Native.jpl_get_state(handle, jdTdb, (int)target, (int)center, state);
        if (status != JplStatus.Ok)
        {
            throw new JplException(status);
        }
        return state;
    }

    public (double[] State, double LightTimeDays) GetStateLighttime(double[] jdTdb, JplTarget target, JplTarget center)
    {
        if (IsInvalid)
            throw new ObjectDisposedException(nameof(JplEphemeris));

        double[] state = new double[6];
        JplStatus status = Native.jpl_get_state_lighttime(handle, jdTdb, (int)target, (int)center, state, out double lt);
        if (status != JplStatus.Ok)
        {
            throw new JplException(status);
        }
        return (state, lt);
    }



    // Frame conversions
    public static double[] EclipticToIcrs(double[] state, double[] jdTt)
    {
        double[] outState = new double[6];
        JplStatus status = Native.jpl_ecliptic_to_icrs(state, jdTt, outState);
        if (status != JplStatus.Ok)
        {
            throw new JplException(status);
        }
        return outState;
    }

    public static double[] IcrsToEcliptic(double[] state, double[] jdTt)
    {
        double[] outState = new double[6];
        JplStatus status = Native.jpl_icrs_to_ecliptic(state, jdTt, outState);
        if (status != JplStatus.Ok)
        {
            throw new JplException(status);
        }
        return outState;
    }

    public static double[,] FrameRotationMatrix(double[] jdTt)
    {
        double[,] R = new double[3, 3];
        JplStatus status = Native.jpl_frame_rotation_matrix(jdTt, R);
        if (status != JplStatus.Ok)
        {
            throw new JplException(status);
        }
        return R;
    }

    public static double[] TtToTdb(double[] jdTt)
    {
        double[] jdTdb = new double[2];
        JplStatus status = Native.jpl_tt_to_tdb(jdTt, jdTdb);
        if (status != JplStatus.Ok)
        {
            throw new JplException(status);
        }
        return jdTdb;
    }

    public static double[] TdbToTt(double[] jdTdb)
    {
        double[] jdTt = new double[2];
        JplStatus status = Native.jpl_tdb_to_tt(jdTdb, jdTt);
        if (status != JplStatus.Ok)
        {
            throw new JplException(status);
        }
        return jdTt;
    }

    public static double[] ApplyAberration(double[] geomState, double[] obsVel)
    {
        double[] outState = new double[6];
        JplStatus status = Native.jpl_apply_aberration(geomState, obsVel, outState);
        if (status != JplStatus.Ok)
        {
            throw new JplException(status);
        }
        return outState;
    }

    // Convenience: Clone returning JplEphemeris
    public JplEphemeris Clone()
    {
        if (IsInvalid)
            throw new ObjectDisposedException(nameof(JplEphemeris));

        JplStatus status = Native.jpl_ephemeris_clone(handle, out IntPtr cloneHandle);
        if (status != JplStatus.Ok)
        {
            throw new JplException(status);
        }
        return new JplEphemeris(cloneHandle);
    }

    // Convenience functions
    public double[] PlanetPosition(double[] jdTt, JplTarget planet, JplTarget center)
    {
        if (IsInvalid)
            throw new ObjectDisposedException(nameof(JplEphemeris));

        double[] state = new double[6];
        JplStatus status = Native.jpl_planet_position(handle, jdTt, (int)planet, (int)center, state);
        if (status != JplStatus.Ok)
        {
            throw new JplException(status);
        }
        return state;
    }

    public double[] MoonPosition(double[] jdTt)
    {
        if (IsInvalid)
            throw new ObjectDisposedException(nameof(JplEphemeris));

        double[] state = new double[6];
        JplStatus status = Native.jpl_moon_position(handle, jdTt, state);
        if (status != JplStatus.Ok)
        {
            throw new JplException(status);
        }
        return state;
    }

    public ApparentEclipticResult ApparentEclipticPosition(double[] jdTt, JplTarget target)
    {
        if (IsInvalid)
            throw new ObjectDisposedException(nameof(JplEphemeris));

        JplStatus status = Native.jpl_apparent_ecliptic_position(
            handle, jdTt, (int)target, out JplApparentEclipticResultNative result);
        if (status != JplStatus.Ok)
        {
            throw new JplException(status);
        }

        return new ApparentEclipticResult
        {
            LongitudeDeg = result.LongitudeDeg,
            LatitudeDeg = result.LatitudeDeg,
            RangeAu = result.RangeAu,
            LongitudeRateDegPerDay = result.LongitudeRateDegPerDay,
            RangeRateKmPerS = result.RangeRateKmPerS
        };
    }

    public static double[] AuPerDayToKmPerS(double[] velAuDay)
    {
        double[] outVel = new double[3];
        JplStatus status = Native.jpl_au_per_day_to_km_per_s(velAuDay, outVel);
        if (status != JplStatus.Ok)
        {
            throw new JplException(status);
        }
        return outVel;
    }

    public static double KmToAu(double km)
    {
        JplStatus status = Native.jpl_km_to_au(km, out double au);
        if (status != JplStatus.Ok)
        {
            throw new JplException(status);
        }
        return au;
    }
}
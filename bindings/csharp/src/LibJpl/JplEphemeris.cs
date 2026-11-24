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

    private static void ValidateLength(double[] arr, int minLength, string paramName)
    {
        if (arr == null)
            throw new ArgumentNullException(paramName);
        if (arr.Length < minLength)
            throw new ArgumentException($"Expected at least {minLength} elements.", paramName);
    }

    private T WithHandle<T>(Func<IntPtr, T> action)
    {
        if (IsInvalid)
            throw new ObjectDisposedException(nameof(JplEphemeris));

        bool addedRef = false;
        try
        {
            DangerousAddRef(ref addedRef);
            return action(DangerousGetHandle());
        }
        finally
        {
            if (addedRef)
                DangerousRelease();
        }
    }

    public JplHeader GetHeader()
    {
        return WithHandle(h =>
        {
            IntPtr headerPtr = Native.jpl_get_header(h);
            if (headerPtr == IntPtr.Zero)
                throw new JplException(JplStatus.ErrorArgument);

            return JplHeader.FromNative(headerPtr, idx => GetConstantNameFromHandle(h, idx));
        });
    }

    private static string GetConstantNameFromHandle(IntPtr h, long index)
    {
        byte[] name = new byte[7];
        JplStatus status = Native.jpl_get_constant_name(h, index, name);
        if (status != JplStatus.Ok)
        {
            throw new JplException(status);
        }
        return System.Text.Encoding.ASCII.GetString(name).TrimEnd('\0');
    }

    public double[] ReadConstants()
    {
        return WithHandle(h =>
        {
            IntPtr headerPtr = Native.jpl_get_header(h);
            if (headerPtr == IntPtr.Zero)
                throw new JplException(JplStatus.ErrorArgument);

            JplHeaderNative native = Marshal.PtrToStructure<JplHeaderNative>(headerPtr);
            double[] values = new double[native.ConstantCount];
            JplStatus status = Native.jpl_read_constants(h, values, values.Length);
            if (status != JplStatus.Ok)
            {
                throw new JplException(status);
            }
            return values;
        });
    }

    public string GetConstantName(long index)
    {
        return WithHandle(h => GetConstantNameFromHandle(h, index));
    }

    public double[] ReadRecord(long recordIndex)
    {
        return WithHandle(h =>
        {
            IntPtr headerPtr = Native.jpl_get_header(h);
            if (headerPtr == IntPtr.Zero)
                throw new JplException(JplStatus.ErrorArgument);

            JplHeaderNative native = Marshal.PtrToStructure<JplHeaderNative>(headerPtr);
            double[] values = new double[(long)native.RecordDoubleCount];
            JplStatus status = Native.jpl_read_record(h, recordIndex, values, values.Length);
            if (status != JplStatus.Ok)
            {
                throw new JplException(status);
            }
            return values;
        });
    }

    public long RecordCount => IsInvalid ? 0 : WithHandle(h => Native.jpl_record_count(h));

    public (double Seconds, double RatePerDay) GetTtMinusTdb(double[] jdTdb)
    {
        ValidateLength(jdTdb, 2, nameof(jdTdb));
        var jd = new JplDate { Hi = jdTdb[0], Lo = jdTdb[1] };
        return WithHandle(h =>
        {
            JplStatus status = Native.jpl_get_tt_minus_tdb(h, jd, out double seconds, out double rate);
            if (status != JplStatus.Ok)
            {
                throw new JplException(status);
            }
            return (seconds, rate);
        });
    }

    public bool HasTtTdb => !IsInvalid && WithHandle(h => Native.jpl_has_tt_tdb(h) != 0);

    public double[] GetState(double[] jdTdb, JplTarget target, JplTarget center)
    {
        ValidateLength(jdTdb, 2, nameof(jdTdb));
        var jd = new JplDate { Hi = jdTdb[0], Lo = jdTdb[1] };
        return WithHandle(h =>
        {
            JplStatus status = Native.jpl_get_state(h, jd, (int)target, (int)center, out JplState state);
            if (status != JplStatus.Ok)
            {
                throw new JplException(status);
            }
            return new double[] { state.X, state.Y, state.Z, state.Vx, state.Vy, state.Vz };
        });
    }

    public (double[] State, double LightTimeDays) GetStateLighttime(double[] jdTdb, JplTarget target, JplTarget center)
    {
        ValidateLength(jdTdb, 2, nameof(jdTdb));
        var jd = new JplDate { Hi = jdTdb[0], Lo = jdTdb[1] };
        return WithHandle(h =>
        {
            JplStatus status = Native.jpl_get_state_lighttime(h, jd, (int)target, (int)center, out JplState state, out double lt);
            if (status != JplStatus.Ok)
            {
                throw new JplException(status);
            }
            return (new double[] { state.X, state.Y, state.Z, state.Vx, state.Vy, state.Vz }, lt);
        });
    }



    // Frame conversions (ecliptic-to-ICRS via a cached frame context)
    public static double[] EclipticToIcrs(double[] state, double[] jdTt)
    {
        ValidateLength(state, 6, nameof(state));
        ValidateLength(jdTt, 2, nameof(jdTt));
        var st = new JplState { X = state[0], Y = state[1], Z = state[2], Vx = state[3], Vy = state[4], Vz = state[5] };
        var jd = new JplDate { Hi = jdTt[0], Lo = jdTt[1] };
        JplStatus status = Native.jpl_frame_context_create(jd, out IntPtr ctx);
        if (status != JplStatus.Ok)
        {
            throw new JplException(status);
        }
        try
        {
            status = Native.jpl_frame_context_rotate_state(ctx, st, out JplState outState);
            if (status != JplStatus.Ok)
            {
                throw new JplException(status);
            }
            return new double[] { outState.X, outState.Y, outState.Z, outState.Vx, outState.Vy, outState.Vz };
        }
        finally
        {
            Native.jpl_frame_context_destroy(ctx);
        }
    }

    public static double[] IcrsToEcliptic(double[] state, double[] jdTt)
    {
        ValidateLength(state, 6, nameof(state));
        ValidateLength(jdTt, 2, nameof(jdTt));
        var st = new JplState { X = state[0], Y = state[1], Z = state[2], Vx = state[3], Vy = state[4], Vz = state[5] };
        var jd = new JplDate { Hi = jdTt[0], Lo = jdTt[1] };
        JplStatus status = Native.jpl_icrs_to_ecliptic(st, jd, out JplState outState);
        if (status != JplStatus.Ok)
        {
            throw new JplException(status);
        }
        return new double[] { outState.X, outState.Y, outState.Z, outState.Vx, outState.Vy, outState.Vz };
    }

    public static double[,] FrameRotationMatrix(double[] jdTt)
    {
        ValidateLength(jdTt, 2, nameof(jdTt));
        var jd = new JplDate { Hi = jdTt[0], Lo = jdTt[1] };
        double[,] R = new double[3, 3];
        JplStatus status = Native.jpl_frame_rotation_matrix(jd, R);
        if (status != JplStatus.Ok)
        {
            throw new JplException(status);
        }
        return R;
    }

    public static double[] TtToTdb(double[] jdTt)
    {
        ValidateLength(jdTt, 2, nameof(jdTt));
        var jd = new JplDate { Hi = jdTt[0], Lo = jdTt[1] };
        JplStatus status = Native.jpl_tt_to_tdb(jd, out JplDate jdTdb);
        if (status != JplStatus.Ok)
        {
            throw new JplException(status);
        }
        return new double[] { jdTdb.Hi, jdTdb.Lo };
    }

    public static double[] TdbToTt(double[] jdTdb)
    {
        ValidateLength(jdTdb, 2, nameof(jdTdb));
        var jd = new JplDate { Hi = jdTdb[0], Lo = jdTdb[1] };
        JplStatus status = Native.jpl_tdb_to_tt(jd, out JplDate jdTt);
        if (status != JplStatus.Ok)
        {
            throw new JplException(status);
        }
        return new double[] { jdTt.Hi, jdTt.Lo };
    }

    public static double[] ApplyAberration(double[] geomState, double[] obsVel)
    {
        return ApplyAberrationFull(geomState, obsVel, 0.0);
    }

    public static double[] ApplyAberrationFull(double[] geomState, double[] obsVel, double sunDistanceAu)
    {
        ValidateLength(geomState, 6, nameof(geomState));
        ValidateLength(obsVel, 3, nameof(obsVel));
        var st = new JplState { X = geomState[0], Y = geomState[1], Z = geomState[2], Vx = geomState[3], Vy = geomState[4], Vz = geomState[5] };
        JplStatus status = Native.jpl_apply_aberration(st, obsVel, sunDistanceAu, out JplState outState);
        if (status != JplStatus.Ok)
        {
            throw new JplException(status);
        }
        return new double[] { outState.X, outState.Y, outState.Z, outState.Vx, outState.Vy, outState.Vz };
    }

    // Convenience: Clone returning JplEphemeris
    public JplEphemeris Clone()
    {
        return WithHandle(h =>
        {
            JplStatus status = Native.jpl_ephemeris_clone(h, out IntPtr cloneHandle);
            if (status != JplStatus.Ok)
            {
                throw new JplException(status);
            }
            return new JplEphemeris(cloneHandle);
        });
    }

    // Convenience functions
    public double[] PlanetPosition(double[] jdTt, JplTarget planet, JplTarget center)
    {
        ValidateLength(jdTt, 2, nameof(jdTt));
        var jd = new JplDate { Hi = jdTt[0], Lo = jdTt[1] };
        return WithHandle(h =>
        {
            JplStatus status = Native.jpl_planet_position(h, jd, (int)planet, (int)center, out JplState state);
            if (status != JplStatus.Ok)
            {
                throw new JplException(status);
            }
            return new double[] { state.X, state.Y, state.Z, state.Vx, state.Vy, state.Vz };
        });
    }

    public double[] MoonPosition(double[] jdTt)
    {
        ValidateLength(jdTt, 2, nameof(jdTt));
        var jd = new JplDate { Hi = jdTt[0], Lo = jdTt[1] };
        return WithHandle(h =>
        {
            JplStatus status = Native.jpl_moon_position(h, jd, out JplState state);
            if (status != JplStatus.Ok)
            {
                throw new JplException(status);
            }
            return new double[] { state.X, state.Y, state.Z, state.Vx, state.Vy, state.Vz };
        });
    }

    public ApparentEclipticResult ApparentEclipticPosition(double[] jdTt, JplTarget target)
    {
        ValidateLength(jdTt, 2, nameof(jdTt));
        var jd = new JplDate { Hi = jdTt[0], Lo = jdTt[1] };
        return WithHandle(h =>
        {
            JplStatus status = Native.jpl_apparent_ecliptic_position(
                h, jd, (int)target, out JplApparentEclipticResultNative result);
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
        });
    }

    public static double[] AuPerDayToKmPerS(double[] velAuDay)
    {
        ValidateLength(velAuDay, 3, nameof(velAuDay));
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
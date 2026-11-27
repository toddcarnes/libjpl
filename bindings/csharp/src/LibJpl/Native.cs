/* Copyright (c) 2026 Todd Carnes
 * SPDX-License-Identifier: MIT
 * See LICENSE in the project root for the full license text.
 */
using System;
using System.Runtime.InteropServices;

namespace LibJpl;

[StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi, Pack = 8)]
internal struct JplPointerNative
{
    public int StartIndex;
    public int CoefficientsPerComponent;
    public int Subintervals;
}

[StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi, Pack = 8)]
internal struct JplHeaderNative
{
    // titles[3][85]
    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 3 * 85)]
    public byte[] TitlesFlat;

    // constant_names[400][7]
    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 400 * 7)]
    public byte[] ConstantNamesFlat;

    // start_jd[2]
    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 2)]
    public double[] StartJd;

    // end_jd[2]
    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 2)]
    public double[] EndJd;

    public double StepDays;
    public int ConstantCount;
    public double AstronomicalUnitKm;
    public double EarthMoonMassRatio;

    // pointers[12]
    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 12)]
    public JplPointerNative[] Pointers;

    public int EphemerisNumber;
    public int ReferenceFrame;
    public JplPointerNative LibrationPointer;
    public JplPointerNative TtTdbPointer;
    public int HasTtTdb;
    public ulong RecordDoubleCount;
    public int ByteOrder;
}

[StructLayout(LayoutKind.Sequential, Pack = 8)]
public struct JplDate
{
    public double Hi;
    public double Lo;
}

[StructLayout(LayoutKind.Sequential, Pack = 8)]
public struct JplState
{
    public double X;
    public double Y;
    public double Z;
    public double Vx;
    public double Vy;
    public double Vz;
}

[StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi, Pack = 8)]
internal struct JplApparentEclipticResultNative
{
    public double LongitudeDeg;
    public double LatitudeDeg;
    public double RangeAu;
    public double LongitudeRateDegPerDay;
    public double RangeRateKmPerS;
}

public static partial class Native
{
    // Must match JPL_MAX_CONSTANT_NAMES in include/jpl.h.
    public const int JplMaxConstantNames = 400;
    private const string DllName = "jpl";

    // jpl.h - core API
    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern JplStatus jpl_open(
        [MarshalAs(UnmanagedType.LPUTF8Str)] string path,
        out IntPtr ephemeris);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void jpl_close(IntPtr ephemeris);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr jpl_get_header(IntPtr ephemeris);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern JplStatus jpl_read_constants(
        IntPtr ephemeris,
        [Out] double[] values,
        long value_count);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern JplStatus jpl_get_constant_name(
        IntPtr ephemeris,
        long index,
        [Out] byte[] name);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern JplStatus jpl_read_record(
        IntPtr ephemeris,
        long record_index,
        [Out] double[] values,
        long value_count);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern long jpl_record_count(IntPtr ephemeris);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern JplStatus jpl_get_tt_minus_tdb(
        IntPtr ephemeris,
        in JplDate jd_tdb,
        out double seconds,
        out double rate_per_day);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int jpl_has_tt_tdb(IntPtr ephemeris);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern JplStatus jpl_get_state(
        IntPtr ephemeris,
        in JplDate jd_tdb,
        int target,
        int center,
        out JplState state);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern JplStatus jpl_get_state_lighttime(
        IntPtr ephemeris,
        in JplDate jd_tdb,
        int target,
        int center,
        out JplState state,
        out double out_light_time);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern JplStatus jpl_apply_aberration(
        in JplState state,
        double[] observer_vel,
        double sun_distance_au,
        out JplState out_state);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern JplStatus jpl_open_mmap(
        [MarshalAs(UnmanagedType.LPUTF8Str)] string path,
        out IntPtr ephemeris);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern JplStatus jpl_ephemeris_clone(
        IntPtr ephem,
        out IntPtr out_clone);

    // jpl_frames.h - frame conversions and time conversions.
    // (No ecliptic_to_icrs import: callers use the cached frame-context
    // API below for the same transform.)
    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern JplStatus jpl_icrs_to_ecliptic(
        in JplState state,
        in JplDate jd_tt,
        out JplState out_state);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern JplStatus jpl_frame_rotation_matrix(
        in JplDate jd_tt,
        [Out, MarshalAs(UnmanagedType.LPArray, SizeConst = 9)] double[,] R);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern JplStatus jpl_frame_context_create(
        in JplDate jd_tt,
        out IntPtr out_context);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void jpl_frame_context_destroy(IntPtr context);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern JplStatus jpl_frame_context_rotate_vector(
        IntPtr context,
        double[] vec,
        [Out] double[] out_vec);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern JplStatus jpl_frame_context_rotate_state(
        IntPtr context,
        in JplState state,
        out JplState out_state);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern JplStatus jpl_tt_to_tdb(
        in JplDate jd_tt,
        out JplDate jd_tdb);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern JplStatus jpl_tdb_to_tt(
        in JplDate jd_tdb,
        out JplDate jd_tt);

    // jpl_convenience.h - planet/Moon helpers and unit conversions
    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern JplStatus jpl_planet_position(
        IntPtr ephem,
        in JplDate jd_tt,
        int planet,
        int center,
        out JplState state);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern JplStatus jpl_moon_position(
        IntPtr ephem,
        in JplDate jd_tt,
        out JplState state);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern JplStatus jpl_apparent_ecliptic_position(
        IntPtr ephem,
        in JplDate jd_tt,
        int target,
        out JplApparentEclipticResultNative result);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern JplStatus jpl_au_per_day_to_km_per_s(
        double[] vel_au_day,
        [Out] double[] vel_km_s);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern JplStatus jpl_km_to_au(
        double km,
        out double au);

    // Returns a pointer to a static C string literal; never marshal directly as
    // string (which would attempt to free the static pointer).
    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr jpl_status_string(JplStatus status);
}
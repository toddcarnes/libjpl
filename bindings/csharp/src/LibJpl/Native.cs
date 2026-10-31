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
        double[] jd_tdb,
        out double seconds,
        out double rate_per_day);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int jpl_has_tt_tdb(IntPtr ephemeris);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern JplStatus jpl_get_state(
        IntPtr ephemeris,
        double[] jd_tdb,
        int target,
        int center,
        [Out] double[] state);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern JplStatus jpl_get_state_lighttime(
        IntPtr ephemeris,
        double[] jd_tdb,
        int target,
        int center,
        [Out] double[] state,
        out double out_light_time);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern JplStatus jpl_apply_aberration(
        double[] state,
        double[] observer_vel,
        [Out] double[] out_state);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern JplStatus jpl_apply_aberration_full(
        double[] state,
        double[] observer_vel,
        double sun_distance_au,
        [Out] double[] out_state);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern JplStatus jpl_open_mmap(
        [MarshalAs(UnmanagedType.LPUTF8Str)] string path,
        out IntPtr ephemeris);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern JplStatus jpl_ephemeris_clone(
        IntPtr ephem,
        out IntPtr out_clone);

    // jpl_frames.h - frame conversions and time conversions
    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern JplStatus jpl_ecliptic_to_icrs(
        [In, MarshalAs(UnmanagedType.LPArray, SizeConst = 6)] double[] state,
        [In, MarshalAs(UnmanagedType.LPArray, SizeConst = 2)] double[] jd_tt,
        [Out, MarshalAs(UnmanagedType.LPArray, SizeConst = 6)] double[] out_state);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern JplStatus jpl_icrs_to_ecliptic(
        [In, MarshalAs(UnmanagedType.LPArray, SizeConst = 6)] double[] state,
        [In, MarshalAs(UnmanagedType.LPArray, SizeConst = 2)] double[] jd_tt,
        [Out, MarshalAs(UnmanagedType.LPArray, SizeConst = 6)] double[] out_state);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern JplStatus jpl_frame_rotation_matrix(
        [In, MarshalAs(UnmanagedType.LPArray, SizeConst = 2)] double[] jd_tt,
        [Out, MarshalAs(UnmanagedType.LPArray, SizeConst = 9)] double[,] R);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern JplStatus jpl_tt_to_tdb(
        [In, MarshalAs(UnmanagedType.LPArray, SizeConst = 2)] double[] jd_tt,
        [Out, MarshalAs(UnmanagedType.LPArray, SizeConst = 2)] double[] jd_tdb);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern JplStatus jpl_tdb_to_tt(
        [In, MarshalAs(UnmanagedType.LPArray, SizeConst = 2)] double[] jd_tdb,
        [Out, MarshalAs(UnmanagedType.LPArray, SizeConst = 2)] double[] jd_tt);

    // jpl_convenience.h - planet/Moon helpers and unit conversions
    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern JplStatus jpl_planet_position(
        IntPtr ephem,
        double[] jd_tt,
        int planet,
        int center,
        [Out] double[] state);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern JplStatus jpl_moon_position(
        IntPtr ephem,
        double[] jd_tt,
        [Out] double[] state);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern JplStatus jpl_apparent_ecliptic_position(
        IntPtr ephem,
        double[] jd_tt,
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

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.LPUTF8Str)]
    public static extern string jpl_status_string(JplStatus status);
}
/* Copyright (c) 2026 Todd Carnes
 * SPDX-License-Identifier: MIT
 * See LICENSE in the project root for the full license text.
 */
using System;
using System.Runtime.InteropServices;

namespace LibJpl;

public sealed class JplException : Exception
{
    public JplStatus Status { get; }

    public JplException(JplStatus status)
        : base($"libjpl error: {GetStatusString(status)}")
    {
        Status = status;
    }

    private static string GetStatusString(JplStatus status)
    {
        return status switch
        {
            JplStatus.Ok => "success",
            JplStatus.ErrorArgument => "invalid argument",
            JplStatus.ErrorIo => "I/O error",
            JplStatus.ErrorFormat => "invalid format",
            JplStatus.ErrorRange => "index or date out of range",
            JplStatus.ErrorMemory => "out of memory",
            JplStatus.ErrorUnsupported => "unsupported",
            JplStatus.ErrorConvergence => "convergence failure",
            JplStatus.ErrorNumeric => "numeric error",
            _ => $"unknown error (status={status})"
        };
    }
}

public struct TtMinusTdbResult
{
    public double Seconds;
    public double RatePerDay;
}

public struct LightTimeResult
{
    public double[] State;
    public double LightTimeDays;
}

public struct ApparentEclipticResult
{
    public double LongitudeDeg;
    public double LatitudeDeg;
    public double RangeAu;
    public double LongitudeRateDegPerDay;
    public double RangeRateKmPerS;
}

public struct JplPointer
{
    public int StartIndex;
    public int CoefficientsPerComponent;
    public int Subintervals;
}

public class JplHeader
{
    public string[] Titles { get; internal set; } = Array.Empty<string>();
    public string[] ConstantNames { get; internal set; } = Array.Empty<string>();
    public double[] StartJd { get; internal set; } = Array.Empty<double>();
    public double[] EndJd { get; internal set; } = Array.Empty<double>();
    public double StepDays { get; internal set; }
    public int ConstantCount { get; internal set; }
    public double AstronomicalUnitKm { get; internal set; }
    public double EarthMoonMassRatio { get; internal set; }
    public JplPointer[] Pointers { get; internal set; } = Array.Empty<JplPointer>();
    public int EphemerisNumber { get; internal set; }
    public JplReferenceFrame ReferenceFrame { get; internal set; }
    public JplPointer LibrationPointer { get; internal set; }
    public JplPointer TtTdbPointer { get; internal set; }
    public int HasTtTdb { get; internal set; }
    public long RecordDoubleCount { get; internal set; }
    public JplByteOrder ByteOrder { get; internal set; }

internal static JplHeader FromNative(IntPtr headerPtr)
{
    if (headerPtr == IntPtr.Zero)
        throw new ArgumentException("Invalid header pointer");

    // Read ephemeris_number directly using pointer arithmetic to verify struct layout
    // Offset calculation:
    // titles[3][85] = 255 bytes
    // constant_names[400][7] = 2800 bytes (total 3055)
    // start_jd[2] = 16 bytes, aligned to 8 -> offset 3056 (total 3072)
    // end_jd[2] = 16 bytes (total 3088)
    // step_days = 8 bytes (total 3096)
    // constant_count = 4 bytes + 4 padding = 8 bytes (total 3104)
    // astronomical_unit_km = 8 bytes (total 3112)
    // earth_moon_mass_ratio = 8 bytes (total 3120)
    // pointers[12] = 12 * 12 = 144 bytes (total 3264)
    // ephemeris_number = 4 bytes at offset 3264
    int ephemerisNumber = Marshal.ReadInt32(headerPtr, 3264);
    
    var h = new JplHeader
    {
        EphemerisNumber = ephemerisNumber,
        Titles = Array.Empty<string>(),
        ConstantNames = Array.Empty<string>(),
        StartJd = Array.Empty<double>(),
        EndJd = Array.Empty<double>(),
        Pointers = Array.Empty<JplPointer>()
    };
    
    return h;
}
}

public static class LibJplApi
{
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

    public static string StatusString(JplStatus status)
    {
        return Native.jpl_status_string(status);
    }
}
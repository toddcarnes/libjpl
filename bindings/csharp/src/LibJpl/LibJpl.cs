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
        : base($"libjpl error: {LibJplApi.StatusString(status)}")
    {
        Status = status;
    }
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

public sealed class JplLightTimeSolution
{
    public double[] RelState { get; init; } = Array.Empty<double>();
    public double[] ObserverState { get; init; } = Array.Empty<double>();
    public double LightTimeDays { get; init; }
    public double[] Emission { get; init; } = Array.Empty<double>();
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

internal static JplHeader FromHandle(IntPtr handle, Func<long, string> getName)
{
    static void Check(JplStatus status)
    {
        if (status != JplStatus.Ok)
            throw new JplException(status);
    }

    static string FixedString(byte[] bytes, int offset, int length)
    {
        int end = Array.IndexOf(bytes, (byte)0, offset, length);
        if (end < 0)
            end = offset + length;
        return System.Text.Encoding.UTF8.GetString(bytes, offset, end - offset);
    }

    var titleBytes = new byte[3 * 85];
    Check(Native.jpl_header_titles(handle, titleBytes));
    var titles = new string[3];
    for (int i = 0; i < titles.Length; ++i)
        titles[i] = FixedString(titleBytes, i * 85, 85);

    Check(Native.jpl_header_coverage(handle, out JplDate start, out JplDate end, out double step));
    Check(Native.jpl_header_shape(handle, out int constantCount, out ulong recordDoubleCount, out int byteOrder));
    Check(Native.jpl_header_identity(handle, out int ephemerisNumber, out int frame));
    Check(Native.jpl_header_scalars(handle, out double au, out double emrat, out int hasTtTdb));

    var pointers = new JplPointer[12];
    for (int i = 0; i < pointers.Length; ++i)
    {
        Check(Native.jpl_header_pointer(handle, i, out JplPointerNative pointer));
        pointers[i] = ToPointer(pointer);
    }
    Check(Native.jpl_header_libration_pointer(handle, out JplPointerNative libration));
    Check(Native.jpl_header_tt_tdb_pointer(handle, out JplPointerNative ttTdb));

    var names = new string[Math.Max(constantCount, 0)];
    for (int i = 0; i < names.Length; ++i)
        names[i] = getName(i);

    return new JplHeader
    {
        Titles = titles,
        ConstantNames = names,
        StartJd = new[] { start.Hi, start.Lo },
        EndJd = new[] { end.Hi, end.Lo },
        StepDays = step,
        AstronomicalUnitKm = au,
        ConstantCount = constantCount,
        EarthMoonMassRatio = emrat,
        Pointers = pointers,
        EphemerisNumber = ephemerisNumber,
        ReferenceFrame = (JplReferenceFrame)frame,
        LibrationPointer = ToPointer(libration),
        TtTdbPointer = ToPointer(ttTdb),
        HasTtTdb = hasTtTdb,
        RecordDoubleCount = (long)recordDoubleCount,
        ByteOrder = (JplByteOrder)byteOrder
    };
}

private static JplPointer ToPointer(JplPointerNative p)
{
    return new JplPointer
    {
        StartIndex = p.StartIndex,
        CoefficientsPerComponent = p.CoefficientsPerComponent,
        Subintervals = p.Subintervals
    };
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
        return Marshal.PtrToStringUTF8(Native.jpl_status_string(status)) ?? string.Empty;
    }
}
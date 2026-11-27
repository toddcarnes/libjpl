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

internal static JplHeader FromNative(IntPtr headerPtr, Func<long, string>? getOverflowName = null)
{
    if (headerPtr == IntPtr.Zero)
        throw new ArgumentException("Invalid header pointer");

    JplHeaderNative native = Marshal.PtrToStructure<JplHeaderNative>(headerPtr);

    static string FixedString(byte[] bytes, int offset, int length)
    {
        int end = Array.IndexOf(bytes, (byte)0, offset, length);
        if (end < 0)
            end = offset + length;
        return System.Text.Encoding.UTF8.GetString(bytes, offset, end - offset);
    }

    var titles = new string[3];
    for (int i = 0; i < titles.Length; ++i)
    {
        titles[i] = FixedString(native.TitlesFlat, i * 85, 85);
    }

    int totalNames = Math.Max(native.ConstantCount, 0);
    int inlineCount = Math.Min(totalNames, Native.JplMaxConstantNames);
    var constantNames = new string[getOverflowName != null ? totalNames : inlineCount];
    for (int i = 0; i < inlineCount; ++i)
    {
        constantNames[i] = FixedString(native.ConstantNamesFlat, i * 7, 7);
    }
    if (getOverflowName != null)
    {
        for (int i = inlineCount; i < totalNames; ++i)
        {
            constantNames[i] = getOverflowName(i);
        }
    }

    var pointers = new JplPointer[12];
    for (int i = 0; i < pointers.Length; ++i)
    {
        pointers[i] = ToPointer(native.Pointers[i]);
    }

    return new JplHeader
    {
        Titles = titles,
        ConstantNames = constantNames,
        StartJd = new[] { native.StartJd[0], native.StartJd[1] },
        EndJd = new[] { native.EndJd[0], native.EndJd[1] },
        StepDays = native.StepDays,
        AstronomicalUnitKm = native.AstronomicalUnitKm,
        ConstantCount = native.ConstantCount,
        EarthMoonMassRatio = native.EarthMoonMassRatio,
        Pointers = pointers,
        EphemerisNumber = native.EphemerisNumber,
        ReferenceFrame = (JplReferenceFrame)native.ReferenceFrame,
        LibrationPointer = ToPointer(native.LibrationPointer),
        TtTdbPointer = ToPointer(native.TtTdbPointer),
        HasTtTdb = native.HasTtTdb,
        RecordDoubleCount = (long)native.RecordDoubleCount,
        ByteOrder = (JplByteOrder)native.ByteOrder
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
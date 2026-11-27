' Copyright (c) 2026 Todd Carnes
' SPDX-License-Identifier: MIT
' See LICENSE in the project root for the full license text.

Imports System
Imports System.Runtime.InteropServices

Namespace LibJpl

<StructLayout(LayoutKind.Sequential, CharSet:=CharSet.Ansi, Pack:=8)>
Friend Structure JplPointerNative
    Public StartIndex As Integer
    Public CoefficientsPerComponent As Integer
    Public Subintervals As Integer
End Structure

<StructLayout(LayoutKind.Sequential, CharSet:=CharSet.Ansi, Pack:=8)>
Friend Structure JplHeaderNative
    <MarshalAs(UnmanagedType.ByValArray, SizeConst:=3 * 85)>
    Public TitlesFlat As Byte()

    <MarshalAs(UnmanagedType.ByValArray, SizeConst:=400 * 7)>
    Public ConstantNamesFlat As Byte()

    <MarshalAs(UnmanagedType.ByValArray, SizeConst:=2)>
    Public StartJd As Double()

    <MarshalAs(UnmanagedType.ByValArray, SizeConst:=2)>
    Public EndJd As Double()

    Public StepDays As Double
    Public ConstantCount As Integer
    Public AstronomicalUnitKm As Double
    Public EarthMoonMassRatio As Double

    <MarshalAs(UnmanagedType.ByValArray, SizeConst:=12)>
    Public Pointers As JplPointerNative()

    Public EphemerisNumber As Integer
    Public ReferenceFrame As Integer
    Public LibrationPointer As JplPointerNative
    Public TtTdbPointer As JplPointerNative
    Public HasTtTdb As Integer
    Public RecordDoubleCount As ULong
    Public ByteOrder As Integer
End Structure

<StructLayout(LayoutKind.Sequential, Pack:=8)>
Friend Structure JplDateNative
    Public Hi As Double
    Public Lo As Double
End Structure

<StructLayout(LayoutKind.Sequential, Pack:=8)>
Friend Structure JplStateNative
    Public X As Double
    Public Y As Double
    Public Z As Double
    Public Vx As Double
    Public Vy As Double
    Public Vz As Double
End Structure

<StructLayout(LayoutKind.Sequential, Pack:=8)>
Friend Structure JplApparentEclipticResultNative
    Public LongitudeDeg As Double
    Public LatitudeDeg As Double
    Public RangeAu As Double
    Public LongitudeRateDegPerDay As Double
    Public RangeRateKmPerS As Double
End Structure

Friend Module Native
    ' Must match JPL_MAX_CONSTANT_NAMES in include/jpl.h.
    Public Const JplMaxConstantNames As Integer = 400
    Private Const DllName As String = "jpl"

    ' jpl.h - core API
    <DllImport(DllName, CallingConvention:=CallingConvention.Cdecl)>
    Public Function jpl_open(
        <MarshalAs(UnmanagedType.LPUTF8Str)> path As String,
        ByRef ephemeris As IntPtr) As JplStatus
    End Function

    <DllImport(DllName, CallingConvention:=CallingConvention.Cdecl)>
    Public Sub jpl_close(ephemeris As IntPtr)
    End Sub

    <DllImport(DllName, CallingConvention:=CallingConvention.Cdecl)>
    Public Function jpl_get_header(ephemeris As IntPtr) As IntPtr
    End Function

    <DllImport(DllName, CallingConvention:=CallingConvention.Cdecl)>
    Public Function jpl_read_constants(
        ephemeris As IntPtr,
        <Out, MarshalAs(UnmanagedType.LPArray, SizeParamIndex:=2)> values As Double(),
        value_count As Long) As JplStatus
    End Function

    <DllImport(DllName, CallingConvention:=CallingConvention.Cdecl)>
    Public Function jpl_get_constant_name(
        ephemeris As IntPtr,
        index As Long,
        <Out, MarshalAs(UnmanagedType.LPArray, SizeConst:=7)> name As Byte()) As JplStatus
    End Function

    <DllImport(DllName, CallingConvention:=CallingConvention.Cdecl)>
    Public Function jpl_read_record(
        ephemeris As IntPtr,
        record_index As Long,
        <Out, MarshalAs(UnmanagedType.LPArray, SizeParamIndex:=3)> values As Double(),
        value_count As Long) As JplStatus
    End Function

    <DllImport(DllName, CallingConvention:=CallingConvention.Cdecl)>
    Public Function jpl_record_count(ephemeris As IntPtr) As Long
    End Function

    <DllImport(DllName, CallingConvention:=CallingConvention.Cdecl)>
    Public Function jpl_get_tt_minus_tdb(
        ephemeris As IntPtr,
        ByRef jd_tdb As JplDateNative,
        <Out()> ByRef seconds As Double,
        <Out()> ByRef rate_per_day As Double) As JplStatus
    End Function

    <DllImport(DllName, CallingConvention:=CallingConvention.Cdecl)>
    Public Function jpl_has_tt_tdb(ephemeris As IntPtr) As Integer
    End Function

    <DllImport(DllName, CallingConvention:=CallingConvention.Cdecl)>
    Public Function jpl_get_state(
        ephemeris As IntPtr,
        ByRef jd_tdb As JplDateNative,
        target As Integer,
        center As Integer,
        <Out()> ByRef state As JplStateNative) As JplStatus
    End Function

    <DllImport(DllName, CallingConvention:=CallingConvention.Cdecl)>
    Public Function jpl_get_state_lighttime(
        ephemeris As IntPtr,
        ByRef jd_tdb As JplDateNative,
        target As Integer,
        center As Integer,
        <Out()> ByRef state As JplStateNative,
        <Out()> ByRef out_light_time As Double) As JplStatus
    End Function

    <DllImport(DllName, CallingConvention:=CallingConvention.Cdecl)>
    Public Function jpl_apply_aberration(
        ByRef state As JplStateNative,
        <MarshalAs(UnmanagedType.LPArray, SizeConst:=3)> observer_vel As Double(),
        sun_distance_au As Double,
        <Out()> ByRef out_state As JplStateNative) As JplStatus
    End Function

    <DllImport(DllName, CallingConvention:=CallingConvention.Cdecl)>
    Public Function jpl_open_mmap(
        <MarshalAs(UnmanagedType.LPUTF8Str)> path As String,
        ByRef ephemeris As IntPtr) As JplStatus
    End Function

    <DllImport(DllName, CallingConvention:=CallingConvention.Cdecl)>
    Public Function jpl_ephemeris_clone(
        ephem As IntPtr,
        ByRef out_clone As IntPtr) As JplStatus
    End Function

    ' jpl_frames.h - frame conversions and time conversions.
    ' (No ecliptic_to_icrs import: callers use the cached frame-context
    ' API below for the same transform.)
    <DllImport(DllName, CallingConvention:=CallingConvention.Cdecl)>
    Public Function jpl_icrs_to_ecliptic(
        ByRef state As JplStateNative,
        ByRef jd_tt As JplDateNative,
        <Out()> ByRef out_state As JplStateNative) As JplStatus
    End Function

    <DllImport(DllName, CallingConvention:=CallingConvention.Cdecl)>
    Public Function jpl_frame_rotation_matrix(
        ByRef jd_tt As JplDateNative,
        <Out(), MarshalAs(UnmanagedType.LPArray)> R As Double(,)) As JplStatus
    End Function

    <DllImport(DllName, CallingConvention:=CallingConvention.Cdecl)>
    Public Function jpl_frame_context_create(
        ByRef jd_tt As JplDateNative,
        ByRef out_context As IntPtr) As JplStatus
    End Function

    <DllImport(DllName, CallingConvention:=CallingConvention.Cdecl)>
    Public Sub jpl_frame_context_destroy(context As IntPtr)
    End Sub

    <DllImport(DllName, CallingConvention:=CallingConvention.Cdecl)>
    Public Function jpl_frame_context_rotate_vector(
        context As IntPtr,
        vec As Double(),
        <Out()> out_vec As Double()) As JplStatus
    End Function

    <DllImport(DllName, CallingConvention:=CallingConvention.Cdecl)>
    Public Function jpl_frame_context_rotate_state(
        context As IntPtr,
        ByRef state As JplStateNative,
        ByRef out_state As JplStateNative) As JplStatus
    End Function

    <DllImport(DllName, CallingConvention:=CallingConvention.Cdecl)>
    Public Function jpl_tt_to_tdb(
        ByRef jd_tt As JplDateNative,
        <Out()> ByRef jd_tdb As JplDateNative) As JplStatus
    End Function

    <DllImport(DllName, CallingConvention:=CallingConvention.Cdecl)>
    Public Function jpl_tdb_to_tt(
        ByRef jd_tdb As JplDateNative,
        <Out()> ByRef jd_tt As JplDateNative) As JplStatus
    End Function

    ' jpl_convenience.h - planet/Moon helpers and unit conversions
    <DllImport(DllName, CallingConvention:=CallingConvention.Cdecl)>
    Public Function jpl_planet_position(
        ephem As IntPtr,
        ByRef jd_tt As JplDateNative,
        planet As Integer,
        center As Integer,
        <Out()> ByRef state As JplStateNative) As JplStatus
    End Function

    <DllImport(DllName, CallingConvention:=CallingConvention.Cdecl)>
    Public Function jpl_moon_position(
        ephem As IntPtr,
        ByRef jd_tt As JplDateNative,
        <Out()> ByRef state As JplStateNative) As JplStatus
    End Function

    <DllImport(DllName, CallingConvention:=CallingConvention.Cdecl)>
    Public Function jpl_apparent_ecliptic_position(
        ephem As IntPtr,
        ByRef jd_tt As JplDateNative,
        target As Integer,
        ByRef result As JplApparentEclipticResultNative) As JplStatus
    End Function

    <DllImport(DllName, CallingConvention:=CallingConvention.Cdecl)>
    Public Function jpl_au_per_day_to_km_per_s(
        <MarshalAs(UnmanagedType.LPArray, SizeConst:=3)> vel_au_day As Double(),
        <Out, MarshalAs(UnmanagedType.LPArray, SizeConst:=3)> vel_km_s As Double()) As JplStatus
    End Function

    <DllImport(DllName, CallingConvention:=CallingConvention.Cdecl)>
    Public Function jpl_km_to_au(
        km As Double,
        <Out()> ByRef au As Double) As JplStatus
    End Function

    ' Returns a pointer to a static C string literal; never marshal directly as
    ' String (which would attempt to free the static pointer).
    <DllImport(DllName, CallingConvention:=CallingConvention.Cdecl)>
    Public Function jpl_status_string(status As JplStatus) As IntPtr
    End Function

End Module

End Namespace
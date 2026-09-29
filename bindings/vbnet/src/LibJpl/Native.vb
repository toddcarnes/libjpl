' Copyright (c) 2026 Todd Carnes
' SPDX-License-Identifier: MIT
' See LICENSE in the project root for the full license text.

Imports System
Imports System.Runtime.InteropServices

Namespace LibJpl

Friend Module Native
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
        <MarshalAs(UnmanagedType.LPArray, SizeConst:=2)> jd_tdb As Double(),
        <Out()> ByRef seconds As Double,
        <Out()> ByRef rate_per_day As Double) As JplStatus
    End Function

    <DllImport(DllName, CallingConvention:=CallingConvention.Cdecl)>
    Public Function jpl_has_tt_tdb(ephemeris As IntPtr) As Integer
    End Function

    <DllImport(DllName, CallingConvention:=CallingConvention.Cdecl)>
    Public Function jpl_get_state(
        ephemeris As IntPtr,
        <MarshalAs(UnmanagedType.LPArray, SizeConst:=2)> jd_tdb As Double(),
        target As Integer,
        center As Integer,
        <Out, MarshalAs(UnmanagedType.LPArray, SizeConst:=6)> state As Double()) As JplStatus
    End Function

    <DllImport(DllName, CallingConvention:=CallingConvention.Cdecl)>
    Public Function jpl_get_state_lighttime(
        ephemeris As IntPtr,
        <MarshalAs(UnmanagedType.LPArray, SizeConst:=2)> jd_tdb As Double(),
        target As Integer,
        center As Integer,
        <Out, MarshalAs(UnmanagedType.LPArray, SizeConst:=6)> state As Double(),
        <Out()> ByRef out_light_time As Double) As JplStatus
    End Function

    <DllImport(DllName, CallingConvention:=CallingConvention.Cdecl)>
    Public Function jpl_apply_aberration(
        <MarshalAs(UnmanagedType.LPArray, SizeConst:=6)> state As Double(),
        <MarshalAs(UnmanagedType.LPArray, SizeConst:=3)> observer_vel As Double(),
        <Out, MarshalAs(UnmanagedType.LPArray, SizeConst:=6)> out_state As Double()) As JplStatus
    End Function

    <DllImport(DllName, CallingConvention:=CallingConvention.Cdecl)>
    Public Function jpl_apply_aberration_full(
        <MarshalAs(UnmanagedType.LPArray, SizeConst:=6)> state As Double(),
        <MarshalAs(UnmanagedType.LPArray, SizeConst:=3)> observer_vel As Double(),
        sun_distance_au As Double,
        <Out, MarshalAs(UnmanagedType.LPArray, SizeConst:=6)> out_state As Double()) As JplStatus
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

    ' jpl_frames.h - frame conversions and time conversions
    <DllImport(DllName, CallingConvention:=CallingConvention.Cdecl)>
    Public Function jpl_ecliptic_to_icrs(
        <MarshalAs(UnmanagedType.LPArray, SizeConst:=6)> state As Double(),
        <MarshalAs(UnmanagedType.LPArray, SizeConst:=2)> jd_tt As Double(),
        <Out, MarshalAs(UnmanagedType.LPArray, SizeConst:=6)> out_state As Double()) As JplStatus
    End Function

    <DllImport(DllName, CallingConvention:=CallingConvention.Cdecl)>
    Public Function jpl_icrs_to_ecliptic(
        <MarshalAs(UnmanagedType.LPArray, SizeConst:=6)> state As Double(),
        <MarshalAs(UnmanagedType.LPArray, SizeConst:=2)> jd_tt As Double(),
        <Out, MarshalAs(UnmanagedType.LPArray, SizeConst:=6)> out_state As Double()) As JplStatus
    End Function

    <DllImport(DllName, CallingConvention:=CallingConvention.Cdecl)>
    Public Function jpl_frame_rotation_matrix(
        <MarshalAs(UnmanagedType.LPArray, SizeConst:=2)> jd_tt As Double(),
        <Out, MarshalAs(UnmanagedType.LPArray)> R As Double(,)) As JplStatus
    End Function

    <DllImport(DllName, CallingConvention:=CallingConvention.Cdecl)>
    Public Function jpl_tt_to_tdb(
        <MarshalAs(UnmanagedType.LPArray, SizeConst:=2)> jd_tt As Double(),
        <Out, MarshalAs(UnmanagedType.LPArray, SizeConst:=2)> jd_tdb As Double()) As JplStatus
    End Function

    <DllImport(DllName, CallingConvention:=CallingConvention.Cdecl)>
    Public Function jpl_tdb_to_tt(
        <MarshalAs(UnmanagedType.LPArray, SizeConst:=2)> jd_tdb As Double(),
        <Out, MarshalAs(UnmanagedType.LPArray, SizeConst:=2)> jd_tt As Double()) As JplStatus
    End Function

    ' jpl_convenience.h - planet/Moon helpers and unit conversions
    <DllImport(DllName, CallingConvention:=CallingConvention.Cdecl)>
    Public Function jpl_planet_position(
        ephem As IntPtr,
        <MarshalAs(UnmanagedType.LPArray, SizeConst:=2)> jd_tt As Double(),
        planet As Integer,
        center As Integer,
        <Out, MarshalAs(UnmanagedType.LPArray, SizeConst:=6)> state As Double()) As JplStatus
    End Function

    <DllImport(DllName, CallingConvention:=CallingConvention.Cdecl)>
    Public Function jpl_moon_position(
        ephem As IntPtr,
        <MarshalAs(UnmanagedType.LPArray, SizeConst:=2)> jd_tt As Double(),
        <Out, MarshalAs(UnmanagedType.LPArray, SizeConst:=6)> state As Double()) As JplStatus
    End Function

    <DllImport(DllName, CallingConvention:=CallingConvention.Cdecl)>
    Public Function jpl_apparent_ecliptic_position(
        ephem As IntPtr,
        <MarshalAs(UnmanagedType.LPArray, SizeConst:=2)> jd_tt As Double(),
        target As Integer,
        <Out()> ByRef longitude_deg As Double,
        <Out()> ByRef latitude_deg As Double,
        <Out()> ByRef range_au As Double,
        <Out()> ByRef longitude_rate_deg_per_day As Double,
        <Out()> ByRef range_rate_km_per_s As Double) As JplStatus
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

    <DllImport(DllName, CallingConvention:=CallingConvention.Cdecl)>
    Public Function jpl_status_string(status As JplStatus) As String
    End Function

End Module

End Namespace
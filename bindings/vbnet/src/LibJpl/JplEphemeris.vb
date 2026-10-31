' Copyright (c) 2026 Todd Carnes
' SPDX-License-Identifier: MIT
' See LICENSE in the project root for the full license text.

Imports System
Imports System.Runtime.InteropServices

Namespace LibJpl

Public NotInheritable Class JplEphemeris
    Inherits SafeHandle

    Public Sub New()
        MyBase.New(IntPtr.Zero, True)
    End Sub

    Public Overrides ReadOnly Property IsInvalid As Boolean
        Get
            Return handle = IntPtr.Zero
        End Get
    End Property

    Protected Overrides Function ReleaseHandle() As Boolean
        If Not IsInvalid Then
            Native.jpl_close(handle)
            handle = IntPtr.Zero
        End If
        Return True
    End Function

    Public Shared Function Open(path As String) As JplEphemeris
        If path Is Nothing Then
            Throw New ArgumentNullException(NameOf(path))
        End If

        Dim ephemerisHandle As IntPtr = IntPtr.Zero
        Dim status As JplStatus = Native.jpl_open(path, ephemerisHandle)
        If status <> JplStatus.Ok Then
            Throw New JplException(status)
        End If
        Return New JplEphemeris(ephemerisHandle)
    End Function

    Public Shared Function OpenMmap(path As String) As JplEphemeris
        If path Is Nothing Then
            Throw New ArgumentNullException(NameOf(path))
        End If

        Dim ephemerisHandle As IntPtr = IntPtr.Zero
        Dim status As JplStatus = Native.jpl_open_mmap(path, ephemerisHandle)
        If status <> JplStatus.Ok Then
            Throw New JplException(status)
        End If
        Return New JplEphemeris(ephemerisHandle)
    End Function

    Friend Sub New(handle As IntPtr)
        MyBase.New(IntPtr.Zero, True)
        SetHandle(handle)
    End Sub

    Public Sub Clone(ByRef clone As JplEphemeris)
        If IsInvalid Then
            Throw New ObjectDisposedException(NameOf(JplEphemeris))
        End If

        Dim cloneHandle As IntPtr = IntPtr.Zero
        Dim status As JplStatus = Native.jpl_ephemeris_clone(handle, cloneHandle)
        If status <> JplStatus.Ok Then
            Throw New JplException(status)
        End If
        clone = New JplEphemeris(cloneHandle)
    End Sub

    Public Function GetHeader() As JplHeader
        If IsInvalid Then
            Throw New ObjectDisposedException(NameOf(JplEphemeris))
        End If

        Dim headerPtr As IntPtr = Native.jpl_get_header(handle)
        If headerPtr = IntPtr.Zero Then
            Throw New JplException(JplStatus.ErrorArgument)
        End If

        Return JplHeader.FromNative(headerPtr)
    End Function

    Public Function ReadConstants() As Double()
        If IsInvalid Then
            Throw New ObjectDisposedException(NameOf(JplEphemeris))
        End If

        Dim header As JplHeader = GetHeader()
        Dim values(header.ConstantCount - 1) As Double
        Dim status As JplStatus = Native.jpl_read_constants(handle, values, values.Length)
        If status <> JplStatus.Ok Then
            Throw New JplException(status)
        End If
        Return values
    End Function

    Public Function GetConstantName(index As Long) As String
        If IsInvalid Then
            Throw New ObjectDisposedException(NameOf(JplEphemeris))
        End If

        Dim name(6) As Byte
        Dim status As JplStatus = Native.jpl_get_constant_name(handle, index, name)
        If status <> JplStatus.Ok Then
            Throw New JplException(status)
        End If
        Return System.Text.Encoding.ASCII.GetString(name).TrimEnd(ControlChars.NullChar)
    End Function

    Public Function ReadRecord(recordIndex As Long) As Double()
        If IsInvalid Then
            Throw New ObjectDisposedException(NameOf(JplEphemeris))
        End If

        Dim header As JplHeader = GetHeader()
        Dim values(header.RecordDoubleCount - 1) As Double
        Dim status As JplStatus = Native.jpl_read_record(handle, recordIndex, values, values.Length)
        If status <> JplStatus.Ok Then
            Throw New JplException(status)
        End If
        Return values
    End Function

    Public ReadOnly Property RecordCount As Long
        Get
            If IsInvalid Then Return 0
            Return Native.jpl_record_count(handle)
        End Get
    End Property

    Public Function GetTtMinusTdb(jdTdb As Double()) As (Seconds As Double, RatePerDay As Double)
        If IsInvalid Then
            Throw New ObjectDisposedException(NameOf(JplEphemeris))
        End If

        Dim seconds As Double
        Dim rate As Double
        Dim status As JplStatus = Native.jpl_get_tt_minus_tdb(handle, jdTdb, seconds, rate)
        If status <> JplStatus.Ok Then
            Throw New JplException(status)
        End If
        Return (seconds, rate)
    End Function

    Public ReadOnly Property HasTtTdb As Boolean
        Get
            Return Not IsInvalid AndAlso Native.jpl_has_tt_tdb(handle) <> 0
        End Get
    End Property

    Public Function GetState(jdTdb As Double(), target As JplTarget, center As JplTarget) As Double()
        If IsInvalid Then
            Throw New ObjectDisposedException(NameOf(JplEphemeris))
        End If

        Dim state(5) As Double
        Dim status As JplStatus = Native.jpl_get_state(handle, jdTdb, CInt(target), CInt(center), state)
        If status <> JplStatus.Ok Then
            Throw New JplException(status)
        End If
        Return state
    End Function

    Public Function GetStateLighttime(jdTdb As Double(), target As JplTarget, center As JplTarget) As (State As Double(), LightTimeDays As Double)
        If IsInvalid Then
            Throw New ObjectDisposedException(NameOf(JplEphemeris))
        End If

        Dim state(5) As Double
        Dim lt As Double
        Dim status As JplStatus = Native.jpl_get_state_lighttime(handle, jdTdb, CInt(target), CInt(center), state, lt)
        If status <> JplStatus.Ok Then
            Throw New JplException(status)
        End If
        Return (state, lt)
    End Function

    Public Function GetTtMinusTdbResult(jdTdb As Double()) As TtMinusTdbResult
        If IsInvalid Then
            Throw New ObjectDisposedException(NameOf(JplEphemeris))
        End If

        Dim seconds As Double
        Dim rate As Double
        Dim status As JplStatus = Native.jpl_get_tt_minus_tdb(handle, jdTdb, seconds, rate)
        If status <> JplStatus.Ok Then
            Throw New JplException(status)
        End If
        Return New TtMinusTdbResult With {.Seconds = seconds, .RatePerDay = rate}
    End Function

    ' Frame conversions
    Public Shared Function EclipticToIcrs(state As Double(), jdTt As Double()) As Double()
        Dim outState(5) As Double
        Dim status As JplStatus = Native.jpl_ecliptic_to_icrs(state, jdTt, outState)
        If status <> JplStatus.Ok Then
            Throw New JplException(status)
        End If
        Return outState
    End Function

    Public Shared Function IcrsToEcliptic(state As Double(), jdTt As Double()) As Double()
        Dim outState(5) As Double
        Dim status As JplStatus = Native.jpl_icrs_to_ecliptic(state, jdTt, outState)
        If status <> JplStatus.Ok Then
            Throw New JplException(status)
        End If
        Return outState
    End Function

    Public Shared Function FrameRotationMatrix(jdTt As Double()) As Double(,)
        Dim R(2, 2) As Double
        Dim status As JplStatus = Native.jpl_frame_rotation_matrix(jdTt, R)
        If status <> JplStatus.Ok Then
            Throw New JplException(status)
        End If
        Return R
    End Function

    Public Shared Function TtToTdb(jdTt As Double()) As Double()
        Dim jdTdb(1) As Double
        Dim status As JplStatus = Native.jpl_tt_to_tdb(jdTt, jdTdb)
        If status <> JplStatus.Ok Then
            Throw New JplException(status)
        End If
        Return jdTdb
    End Function

    Public Shared Function TdbToTt(jdTdb As Double()) As Double()
        Dim jdTt(1) As Double
        Dim status As JplStatus = Native.jpl_tdb_to_tt(jdTdb, jdTt)
        If status <> JplStatus.Ok Then
            Throw New JplException(status)
        End If
        Return jdTt
    End Function

    Public Shared Function ApplyAberration(geomState As Double(), obsVel As Double()) As Double()
        Dim outState(5) As Double
        Dim status As JplStatus = Native.jpl_apply_aberration(geomState, obsVel, outState)
        If status <> JplStatus.Ok Then
            Throw New JplException(status)
        End If
        Return outState
    End Function

    ' Convenience: Clone returning JplEphemeris
    Public Function Clone() As JplEphemeris
        If IsInvalid Then
            Throw New ObjectDisposedException(NameOf(JplEphemeris))
        End If

        Dim cloneHandle As IntPtr = IntPtr.Zero
        Dim status As JplStatus = Native.jpl_ephemeris_clone(handle, cloneHandle)
        If status <> JplStatus.Ok Then
            Throw New JplException(status)
        End If
        Return New JplEphemeris(cloneHandle)
    End Function

    ' Convenience functions
    Public Function PlanetPosition(jdTt As Double(), planet As JplTarget, center As JplTarget) As Double()
        If IsInvalid Then
            Throw New ObjectDisposedException(NameOf(JplEphemeris))
        End If

        Dim state(5) As Double
        Dim status As JplStatus = Native.jpl_planet_position(handle, jdTt, CInt(planet), CInt(center), state)
        If status <> JplStatus.Ok Then
            Throw New JplException(status)
        End If
        Return state
    End Function

    Public Function MoonPosition(jdTt As Double()) As Double()
        If IsInvalid Then
            Throw New ObjectDisposedException(NameOf(JplEphemeris))
        End If

        Dim state(5) As Double
        Dim status As JplStatus = Native.jpl_moon_position(handle, jdTt, state)
        If status <> JplStatus.Ok Then
            Throw New JplException(status)
        End If
        Return state
    End Function

    Public Function ApparentEclipticPosition(jdTt As Double(), target As JplTarget) As ApparentEclipticResult
        If IsInvalid Then
            Throw New ObjectDisposedException(NameOf(JplEphemeris))
        End If

        Dim result As New JplApparentEclipticResultNative()
        Dim status As JplStatus = Native.jpl_apparent_ecliptic_position(
            handle, jdTt, CInt(target), result)
        If status <> JplStatus.Ok Then
            Throw New JplException(status)
        End If
        Return New ApparentEclipticResult With {
            .LongitudeDeg = result.LongitudeDeg,
            .LatitudeDeg = result.LatitudeDeg,
            .RangeAu = result.RangeAu,
            .LongitudeRateDegPerDay = result.LongitudeRateDegPerDay,
            .RangeRateKmPerS = result.RangeRateKmPerS
        }
    End Function

    Public Shared Function AuPerDayToKmPerS(velAuDay As Double()) As Double()
        Dim outVel(2) As Double
        Dim status As JplStatus = Native.jpl_au_per_day_to_km_per_s(velAuDay, outVel)
        If status <> JplStatus.Ok Then
            Throw New JplException(status)
        End If
        Return outVel
    End Function

    Public Shared Function KmToAu(km As Double) As Double
        Dim au As Double
        Dim status As JplStatus = Native.jpl_km_to_au(km, au)
        If status <> JplStatus.Ok Then
            Throw New JplException(status)
        End If
        Return au
    End Function

End Class

End Namespace
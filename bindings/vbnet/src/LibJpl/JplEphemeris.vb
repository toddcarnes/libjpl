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

    Private Shared Sub ValidateLength(arr As Double(), minLength As Integer, paramName As String)
        If arr Is Nothing Then
            Throw New ArgumentNullException(paramName)
        End If
        If arr.Length < minLength Then
            Throw New ArgumentException($"Expected at least {minLength} elements.", paramName)
        End If
    End Sub

    Private Function WithHandle(Of T)(action As Func(Of IntPtr, T)) As T
        If IsInvalid Then
            Throw New ObjectDisposedException(NameOf(JplEphemeris))
        End If

        Dim addedRef As Boolean = False
        Try
            DangerousAddRef(addedRef)
            Return action(DangerousGetHandle())
        Finally
            If addedRef Then
                DangerousRelease()
            End If
        End Try
    End Function

    Private Shared Function GetConstantNameFromHandle(h As IntPtr, index As Long) As String
        Dim name(6) As Byte
        Dim status As JplStatus = Native.jpl_get_constant_name(h, index, name)
        If status <> JplStatus.Ok Then
            Throw New JplException(status)
        End If
        Return System.Text.Encoding.ASCII.GetString(name).TrimEnd(ControlChars.NullChar)
    End Function

    Public Function GetHeader() As JplHeader
        Return WithHandle(Function(h)
            Dim headerPtr As IntPtr = Native.jpl_get_header(h)
            If headerPtr = IntPtr.Zero Then
                Throw New JplException(JplStatus.ErrorArgument)
            End If
            Return JplHeader.FromNative(headerPtr, Function(idx) GetConstantNameFromHandle(h, idx))
        End Function)
    End Function

    Public Function ReadConstants() As Double()
        Return WithHandle(Function(h)
            Dim headerPtr As IntPtr = Native.jpl_get_header(h)
            If headerPtr = IntPtr.Zero Then
                Throw New JplException(JplStatus.ErrorArgument)
            End If
            Dim nativeHdr As JplHeaderNative = Marshal.PtrToStructure(Of JplHeaderNative)(headerPtr)
            Dim values(nativeHdr.ConstantCount - 1) As Double
            Dim status As JplStatus = Native.jpl_read_constants(h, values, values.Length)
            If status <> JplStatus.Ok Then
                Throw New JplException(status)
            End If
            Return values
        End Function)
    End Function

    Public Function GetConstantName(index As Long) As String
        Return WithHandle(Function(h) GetConstantNameFromHandle(h, index))
    End Function

    Public Function ReadRecord(recordIndex As Long) As Double()
        Return WithHandle(Function(h)
            Dim headerPtr As IntPtr = Native.jpl_get_header(h)
            If headerPtr = IntPtr.Zero Then
                Throw New JplException(JplStatus.ErrorArgument)
            End If
            Dim nativeHdr As JplHeaderNative = Marshal.PtrToStructure(Of JplHeaderNative)(headerPtr)
            Dim values(CInt(nativeHdr.RecordDoubleCount) - 1) As Double
            Dim status As JplStatus = Native.jpl_read_record(h, recordIndex, values, values.Length)
            If status <> JplStatus.Ok Then
                Throw New JplException(status)
            End If
            Return values
        End Function)
    End Function

    Public ReadOnly Property RecordCount As Long
        Get
            Return WithHandle(Function(h) Native.jpl_record_count(h))
        End Get
    End Property

    Public Function GetTtMinusTdb(jdTdb As Double()) As (Seconds As Double, RatePerDay As Double)
        ValidateLength(jdTdb, 2, NameOf(jdTdb))
        Dim jd As New JplDateNative With {.Hi = jdTdb(0), .Lo = jdTdb(1)}
        Return WithHandle(Function(h)
            Dim seconds As Double
            Dim rate As Double
            Dim status As JplStatus = Native.jpl_get_tt_minus_tdb(h, jd, seconds, rate)
            If status <> JplStatus.Ok Then
                Throw New JplException(status)
            End If
            Return (seconds, rate)
        End Function)
    End Function

    Public ReadOnly Property HasTtTdb As Boolean
        Get
            Return WithHandle(Function(h) Native.jpl_has_tt_tdb(h) <> 0)
        End Get
    End Property

    Public Function GetState(jdTdb As Double(), target As JplTarget, center As JplTarget) As Double()
        ValidateLength(jdTdb, 2, NameOf(jdTdb))
        Dim jd As New JplDateNative With {.Hi = jdTdb(0), .Lo = jdTdb(1)}
        Return WithHandle(Function(h)
            Dim stNative As JplStateNative
            Dim status As JplStatus = Native.jpl_get_state(h, jd, CInt(target), CInt(center), stNative)
            If status <> JplStatus.Ok Then
                Throw New JplException(status)
            End If
            Return {stNative.X, stNative.Y, stNative.Z, stNative.Vx, stNative.Vy, stNative.Vz}
        End Function)
    End Function

    Public Function GetStateLighttime(jdTdb As Double(), target As JplTarget, center As JplTarget) As (State As Double(), LightTimeDays As Double)
        ValidateLength(jdTdb, 2, NameOf(jdTdb))
        Dim jd As New JplDateNative With {.Hi = jdTdb(0), .Lo = jdTdb(1)}
        Return WithHandle(Function(h)
            Dim stNative As JplStateNative
            Dim lt As Double
            Dim status As JplStatus = Native.jpl_get_state_lighttime(h, jd, CInt(target), CInt(center), stNative, lt)
            If status <> JplStatus.Ok Then
                Throw New JplException(status)
            End If
            Return ({stNative.X, stNative.Y, stNative.Z, stNative.Vx, stNative.Vy, stNative.Vz}, lt)
        End Function)
    End Function

    ' Frame conversions (vector and state via a cached frame context)
    Public Shared Function RotateVector(vec As Double(), jdTt As Double()) As Double()
        ValidateLength(vec, 3, NameOf(vec))
        ValidateLength(jdTt, 2, NameOf(jdTt))
        Dim jd As New JplDateNative With {.Hi = jdTt(0), .Lo = jdTt(1)}
        Dim result(2) As Double
        Dim ctx As IntPtr = IntPtr.Zero
        Dim status As JplStatus = Native.jpl_frame_context_create(jd, ctx)
        If status <> JplStatus.Ok Then
            Throw New JplException(status)
        End If
        Try
            status = Native.jpl_frame_context_rotate_vector(ctx, vec, result)
            If status <> JplStatus.Ok Then
                Throw New JplException(status)
            End If
            Return result
        Finally
            Native.jpl_frame_context_destroy(ctx)
        End Try
    End Function

    Public Shared Function EclipticToIcrs(state As Double(), jdTt As Double()) As Double()
        ValidateLength(state, 6, NameOf(state))
        ValidateLength(jdTt, 2, NameOf(jdTt))
        Dim nativeIn As New JplStateNative With {.X = state(0), .Y = state(1), .Z = state(2),
                                                .Vx = state(3), .Vy = state(4), .Vz = state(5)}
        Dim jd As New JplDateNative With {.Hi = jdTt(0), .Lo = jdTt(1)}
        Dim nativeOut As JplStateNative
        Dim ctx As IntPtr = IntPtr.Zero
        Dim status As JplStatus = Native.jpl_frame_context_create(jd, ctx)
        If status <> JplStatus.Ok Then
            Throw New JplException(status)
        End If
        Try
            status = Native.jpl_frame_context_rotate_state(ctx, nativeIn, nativeOut)
            If status <> JplStatus.Ok Then
                Throw New JplException(status)
            End If
            Return {nativeOut.X, nativeOut.Y, nativeOut.Z, nativeOut.Vx, nativeOut.Vy, nativeOut.Vz}
        Finally
            Native.jpl_frame_context_destroy(ctx)
        End Try
    End Function

    Public Shared Function IcrsToEcliptic(state As Double(), jdTt As Double()) As Double()
        ValidateLength(state, 6, NameOf(state))
        ValidateLength(jdTt, 2, NameOf(jdTt))
        Dim nativeIn As New JplStateNative With {.X = state(0), .Y = state(1), .Z = state(2),
                                                .Vx = state(3), .Vy = state(4), .Vz = state(5)}
        Dim jd As New JplDateNative With {.Hi = jdTt(0), .Lo = jdTt(1)}
        Dim nativeOut As JplStateNative
        Dim status As JplStatus = Native.jpl_icrs_to_ecliptic(nativeIn, jd, nativeOut)
        If status <> JplStatus.Ok Then
            Throw New JplException(status)
        End If
        Return {nativeOut.X, nativeOut.Y, nativeOut.Z, nativeOut.Vx, nativeOut.Vy, nativeOut.Vz}
    End Function

    Public Shared Function FrameRotationMatrix(jdTt As Double()) As Double(,)
        ValidateLength(jdTt, 2, NameOf(jdTt))
        Dim jd As New JplDateNative With {.Hi = jdTt(0), .Lo = jdTt(1)}
        Dim R(2, 2) As Double
        Dim status As JplStatus = Native.jpl_frame_rotation_matrix(jd, R)
        If status <> JplStatus.Ok Then
            Throw New JplException(status)
        End If
        Return R
    End Function

    Public Shared Function TtToTdb(jdTt As Double()) As Double()
        ValidateLength(jdTt, 2, NameOf(jdTt))
        Dim jdIn As New JplDateNative With {.Hi = jdTt(0), .Lo = jdTt(1)}
        Dim jdOut As JplDateNative
        Dim status As JplStatus = Native.jpl_tt_to_tdb(jdIn, jdOut)
        If status <> JplStatus.Ok Then
            Throw New JplException(status)
        End If
        Return {jdOut.Hi, jdOut.Lo}
    End Function

    Public Shared Function TdbToTt(jdTdb As Double()) As Double()
        ValidateLength(jdTdb, 2, NameOf(jdTdb))
        Dim jdIn As New JplDateNative With {.Hi = jdTdb(0), .Lo = jdTdb(1)}
        Dim jdOut As JplDateNative
        Dim status As JplStatus = Native.jpl_tdb_to_tt(jdIn, jdOut)
        If status <> JplStatus.Ok Then
            Throw New JplException(status)
        End If
        Return {jdOut.Hi, jdOut.Lo}
    End Function

    Public Shared Function ApplyAberration(geomState As Double(), obsVel As Double()) As Double()
        Return ApplyAberrationFull(geomState, obsVel, 0.0)
    End Function

    Public Shared Function ApplyAberrationFull(geomState As Double(), obsVel As Double(), sunDistanceAu As Double) As Double()
        ValidateLength(geomState, 6, NameOf(geomState))
        ValidateLength(obsVel, 3, NameOf(obsVel))
        Dim nativeIn As New JplStateNative With {.X = geomState(0), .Y = geomState(1), .Z = geomState(2),
                                                .Vx = geomState(3), .Vy = geomState(4), .Vz = geomState(5)}
        Dim nativeOut As JplStateNative
        Dim status As JplStatus = Native.jpl_apply_aberration(nativeIn, obsVel, sunDistanceAu, nativeOut)
        If status <> JplStatus.Ok Then
            Throw New JplException(status)
        End If
        Return {nativeOut.X, nativeOut.Y, nativeOut.Z, nativeOut.Vx, nativeOut.Vy, nativeOut.Vz}
    End Function

    ' Convenience: Clone returning JplEphemeris
    Public Function Clone() As JplEphemeris
        Return WithHandle(Function(h)
            Dim cloneHandle As IntPtr = IntPtr.Zero
            Dim status As JplStatus = Native.jpl_ephemeris_clone(h, cloneHandle)
            If status <> JplStatus.Ok Then
                Throw New JplException(status)
            End If
            Return New JplEphemeris(cloneHandle)
        End Function)
    End Function

    ' Convenience functions
    Public Function PlanetPosition(jdTt As Double(), planet As JplTarget, center As JplTarget) As Double()
        ValidateLength(jdTt, 2, NameOf(jdTt))
        Dim jd As New JplDateNative With {.Hi = jdTt(0), .Lo = jdTt(1)}
        Return WithHandle(Function(h)
            Dim stNative As JplStateNative
            Dim status As JplStatus = Native.jpl_planet_position(h, jd, CInt(planet), CInt(center), stNative)
            If status <> JplStatus.Ok Then
                Throw New JplException(status)
            End If
            Return {stNative.X, stNative.Y, stNative.Z, stNative.Vx, stNative.Vy, stNative.Vz}
        End Function)
    End Function

    Public Function MoonPosition(jdTt As Double()) As Double()
        ValidateLength(jdTt, 2, NameOf(jdTt))
        Dim jd As New JplDateNative With {.Hi = jdTt(0), .Lo = jdTt(1)}
        Return WithHandle(Function(h)
            Dim stNative As JplStateNative
            Dim status As JplStatus = Native.jpl_moon_position(h, jd, stNative)
            If status <> JplStatus.Ok Then
                Throw New JplException(status)
            End If
            Return {stNative.X, stNative.Y, stNative.Z, stNative.Vx, stNative.Vy, stNative.Vz}
        End Function)
    End Function

    Public Function ApparentEclipticPosition(jdTt As Double(), target As JplTarget) As ApparentEclipticResult
        ValidateLength(jdTt, 2, NameOf(jdTt))
        Dim jd As New JplDateNative With {.Hi = jdTt(0), .Lo = jdTt(1)}
        Return WithHandle(Function(h)
            Dim result As New JplApparentEclipticResultNative()
            Dim status As JplStatus = Native.jpl_apparent_ecliptic_position(
                h, jd, CInt(target), result)
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
        End Function)
    End Function

    Public Shared Function AuPerDayToKmPerS(velAuDay As Double()) As Double()
        ValidateLength(velAuDay, 3, NameOf(velAuDay))
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
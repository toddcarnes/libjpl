' Copyright (c) 2026 Todd Carnes
' SPDX-License-Identifier: MIT
' See LICENSE in the project root for the full license text.

Imports System
Imports System.Runtime.InteropServices

Namespace LibJpl

Public NotInheritable Class JplException
    Inherits Exception

    Public ReadOnly Property Status As JplStatus

    Public Sub New(status As JplStatus)
        MyBase.New($"libjpl error: {LibJplApi.StatusString(status)}")
        Me.Status = status
    End Sub

End Class

Public Structure ApparentEclipticResult
    Public LongitudeDeg As Double
    Public LatitudeDeg As Double
    Public RangeAu As Double
    Public LongitudeRateDegPerDay As Double
    Public RangeRateKmPerS As Double
End Structure

Public Structure JplPointer
    Public StartIndex As Integer
    Public CoefficientsPerComponent As Integer
    Public Subintervals As Integer
End Structure

Public NotInheritable Class JplLightTimeSolution
    Public Property RelState As Double() = Array.Empty(Of Double)()
    Public Property ObserverState As Double() = Array.Empty(Of Double)()
    Public Property LightTimeDays As Double
    Public Property Emission As Double() = Array.Empty(Of Double)()
End Class

Public Class JplHeader
    Public Property Titles As String() = Array.Empty(Of String)()
    Public Property ConstantNames As String() = Array.Empty(Of String)()
    Public Property StartJd As Double() = Array.Empty(Of Double)()
    Public Property EndJd As Double() = Array.Empty(Of Double)()
    Public Property StepDays As Double
    Public Property ConstantCount As Integer
    Public Property AstronomicalUnitKm As Double
    Public Property EarthMoonMassRatio As Double
    Public Property Pointers As JplPointer() = Array.Empty(Of JplPointer)()
    Public Property EphemerisNumber As Integer
    Public Property ReferenceFrame As JplReferenceFrame
    Public Property LibrationPointer As JplPointer
    Public Property TtTdbPointer As JplPointer
    Public Property HasTtTdb As Integer
    Public Property RecordDoubleCount As Long
    Public Property ByteOrder As JplByteOrder

    Friend Shared Function FromHandle(handle As IntPtr, getName As Func(Of Long, String)) As JplHeader
        Dim titleBytes(3 * 85 - 1) As Byte
        Check(Native.jpl_header_titles(handle, titleBytes))
        Dim headings(2) As String
        For i = 0 To headings.Length - 1
            headings(i) = FixedString(titleBytes, i * 85, 85)
        Next

        Dim start_jd As JplDateNative
        Dim end_jd As JplDateNative
        Dim step_days As Double
        Check(Native.jpl_header_coverage(handle, start_jd, end_jd, step_days))

        Dim constantCount As Integer
        Dim recordDoubleCount As ULong
        Dim byteOrder As Integer
        Check(Native.jpl_header_shape(handle, constantCount, recordDoubleCount, byteOrder))

        Dim ephemerisNumber As Integer
        Dim referenceFrame As Integer
        Check(Native.jpl_header_identity(handle, ephemerisNumber, referenceFrame))

        Dim au As Double
        Dim emrat As Double
        Dim hasTtTdb As Integer
        Check(Native.jpl_header_scalars(handle, au, emrat, hasTtTdb))

        Dim targetPointers(11) As JplPointer
        For i = 0 To targetPointers.Length - 1
            Dim pointer As JplPointerNative
            Check(Native.jpl_header_pointer(handle, i, pointer))
            targetPointers(i) = ToPointer(pointer)
        Next
        Dim libration As JplPointerNative
        Check(Native.jpl_header_libration_pointer(handle, libration))
        Dim ttTdb As JplPointerNative
        Check(Native.jpl_header_tt_tdb_pointer(handle, ttTdb))

        Dim names As String() = Array.Empty(Of String)()
        If constantCount > 0 Then
            ReDim names(constantCount - 1)
            For i = 0 To names.Length - 1
                names(i) = getName(i)
            Next
        End If

        Return New JplHeader With {
            .Titles = headings,
            .ConstantNames = names,
            .StartJd = New Double() {start_jd.Hi, start_jd.Lo},
            .EndJd = New Double() {end_jd.Hi, end_jd.Lo},
            .StepDays = step_days,
            .ConstantCount = constantCount,
            .AstronomicalUnitKm = au,
            .EarthMoonMassRatio = emrat,
            .Pointers = targetPointers,
            .EphemerisNumber = ephemerisNumber,
            .ReferenceFrame = CType(referenceFrame, JplReferenceFrame),
            .LibrationPointer = ToPointer(libration),
            .TtTdbPointer = ToPointer(ttTdb),
            .HasTtTdb = hasTtTdb,
            .RecordDoubleCount = CLng(recordDoubleCount),
            .ByteOrder = CType(byteOrder, JplByteOrder)
        }
    End Function

    Private Shared Sub Check(status As JplStatus)
        If status <> JplStatus.Ok Then
            Throw New JplException(status)
        End If
    End Sub

    Private Shared Function FixedString(bytes As Byte(), offset As Integer, length As Integer) As String
        Dim nulIndex As Integer = Array.IndexOf(bytes, CByte(0), offset, length)
        If nulIndex < 0 Then
            nulIndex = offset + length
        End If
        Return Text.Encoding.UTF8.GetString(bytes, offset, nulIndex - offset)
    End Function

    Private Shared Function ToPointer(p As JplPointerNative) As JplPointer
        Return New JplPointer With {
            .StartIndex = p.StartIndex,
            .CoefficientsPerComponent = p.CoefficientsPerComponent,
            .Subintervals = p.Subintervals
        }
    End Function
End Class

Public Module LibJplApi

    Public Function Open(path As String) As JplEphemeris
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

    Public Function OpenMmap(path As String) As JplEphemeris
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

    Public Function StatusString(status As JplStatus) As String
        Return If(Marshal.PtrToStringUTF8(Native.jpl_status_string(status)), String.Empty)
    End Function

End Module

End Namespace
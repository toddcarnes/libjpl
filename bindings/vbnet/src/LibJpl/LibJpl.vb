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
        MyBase.New($"libjpl error: {GetStatusString(status)}")
        Me.Status = status
    End Sub

    Private Shared Function GetStatusString(status As JplStatus) As String
        Return status.ToString()
    End Function

End Class

Public Structure TtMinusTdbResult
    Public Seconds As Double
    Public RatePerDay As Double
End Structure

Public Structure LightTimeResult
    Public State As Double()
    Public LightTimeDays As Double
End Structure

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

    Friend Shared Function FromNative(headerPtr As IntPtr) As JplHeader
        If headerPtr = IntPtr.Zero Then
            Throw New ArgumentException("Invalid header pointer")
        End If

        Dim native As JplHeaderNative = Marshal.PtrToStructure(Of JplHeaderNative)(headerPtr)

        Dim headings(2) As String
        For i = 0 To headings.Length - 1
            headings(i) = FixedString(native.TitlesFlat, i * 85, 85)
        Next

        Dim nameCount As Integer = Math.Clamp(native.ConstantCount, 0, 400)
        Dim names As String() = Array.Empty(Of String)()
        If nameCount > 0 Then
            ReDim names(nameCount - 1)
            For i = 0 To nameCount - 1
                names(i) = FixedString(native.ConstantNamesFlat, i * 7, 7)
            Next
        End If

        Dim targetPointers(11) As JplPointer
        For i = 0 To targetPointers.Length - 1
            targetPointers(i) = ToPointer(native.Pointers(i))
        Next

        Return New JplHeader With {
            .Titles = headings,
            .ConstantNames = names,
            .StartJd = New Double() {native.StartJd(0), native.StartJd(1)},
            .EndJd = New Double() {native.EndJd(0), native.EndJd(1)},
            .StepDays = native.StepDays,
            .ConstantCount = native.ConstantCount,
            .AstronomicalUnitKm = native.AstronomicalUnitKm,
            .EarthMoonMassRatio = native.EarthMoonMassRatio,
            .Pointers = targetPointers,
            .EphemerisNumber = native.EphemerisNumber,
            .ReferenceFrame = CType(native.ReferenceFrame, JplReferenceFrame),
            .LibrationPointer = ToPointer(native.LibrationPointer),
            .TtTdbPointer = ToPointer(native.TtTdbPointer),
            .HasTtTdb = native.HasTtTdb,
            .RecordDoubleCount = CLng(native.RecordDoubleCount),
            .ByteOrder = CType(native.ByteOrder, JplByteOrder)
        }
    End Function

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
        Return Native.jpl_status_string(status)
    End Function

End Module

End Namespace
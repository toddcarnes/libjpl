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

        Dim h As New JplHeader With {
            .EphemerisNumber = Marshal.ReadInt32(headerPtr, 3264)
        }
        Return h
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
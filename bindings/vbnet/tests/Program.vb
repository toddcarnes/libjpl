' Copyright (c) 2026 Todd Carnes
' SPDX-License-Identifier: MIT
' See LICENSE in the project root for the full license text.

Imports System
Imports System.Linq
Imports System.Runtime.InteropServices

Module Program

    Sub Main(args As String())
        If args.Length < 1 Then
            Console.Error.WriteLine($"Usage: {Environment.GetCommandLineArgs()(0)} <ephemeris-file>")
            Environment.Exit(1)
        End If

        Try
            Using eph = LibJpl.LibJplApi.Open(args(0))

                ' Check ephemeris number (DE441 = 441)
                Dim header = eph.GetHeader()
                If header.EphemerisNumber <> 441 Then
                    Console.Error.WriteLine($"Expected ephemeris number 441, got {header.EphemerisNumber}")
                    Environment.Exit(1)
                End If

                ' The managed mirror of jpl_header must match the C layout, or
                ' fields would be read from the wrong offsets.
                If Marshal.SizeOf(Of LibJpl.JplHeaderNative)() <> 3320 Then
                    Console.Error.WriteLine($"JplHeaderNative size is {Marshal.SizeOf(Of LibJpl.JplHeaderNative)()}, expected 3320")
                    Environment.Exit(1)
                End If
                Dim ephemerisOffset = Marshal.OffsetOf(Of LibJpl.JplHeaderNative)(
                    NameOf(LibJpl.JplHeaderNative.EphemerisNumber)).ToInt32()
                If ephemerisOffset <> 3264 Then
                    Console.Error.WriteLine($"EphemerisNumber offset is {ephemerisOffset}, expected 3264")
                    Environment.Exit(1)
                End If

                ' Header metadata must be populated from the native struct.
                If header.ConstantCount <= 400 OrElse header.ConstantNames.Length <> header.ConstantCount Then
                    Console.Error.WriteLine($"Unexpected ConstantNames length {header.ConstantNames.Length} vs ConstantCount {header.ConstantCount}")
                    Environment.Exit(1)
                End If
                For Each idx In New Long() {0, 399, 400, header.ConstantCount - 1}
                    If header.ConstantNames(CInt(idx)) <> eph.GetConstantName(idx) Then
                        Console.Error.WriteLine($"Constant name mismatch at {idx}: '{header.ConstantNames(CInt(idx))}' vs '{eph.GetConstantName(idx)}'")
                        Environment.Exit(1)
                    End If
                Next
                If Not (header.StartJd(0) < header.EndJd(0)) Then
                    Console.Error.WriteLine($"Bad coverage: {header.StartJd(0)} .. {header.EndJd(0)}")
                    Environment.Exit(1)
                End If
                If header.Titles.Length <> 3 OrElse header.Titles.Any(AddressOf String.IsNullOrEmpty) Then
                    Console.Error.WriteLine($"Missing header titles: {String.Join(",", header.Titles)}")
                    Environment.Exit(1)
                End If
                If header.AstronomicalUnitKm < 1.4959787E+08 OrElse header.AstronomicalUnitKm > 1.49597871E+08 Then
                    Console.Error.WriteLine($"Bad AU: {header.AstronomicalUnitKm}")
                    Environment.Exit(1)
                End If
                If header.EarthMoonMassRatio < 81.3 OrElse header.EarthMoonMassRatio > 81.31 Then
                    Console.Error.WriteLine($"Bad EMRAT: {header.EarthMoonMassRatio}")
                    Environment.Exit(1)
                End If
                ' Slot 2 is EMB (0-based file slots: 0=Mercury, 1=Venus, 2=EMB).
                If header.Pointers(2).Subintervals <= 0 Then
                    Console.Error.WriteLine("EMB pointer has no subintervals")
                    Environment.Exit(1)
                End If

                Dim constants = eph.ReadConstants()
                If constants.Length <> header.ConstantCount Then
                    Console.Error.WriteLine($"Read {constants.Length} constants, expected {header.ConstantCount}")
                    Environment.Exit(1)
                End If
                For Each value In constants
                    If Not Double.IsFinite(value) Then
                        Console.Error.WriteLine("Non-finite constant value")
                        Environment.Exit(1)
                    End If
                Next

                ' Read Earth-from-Sun at J2000.0
                Dim jdTdb = New Double() {2451545.0, 0.0}
                Dim state = eph.GetState(jdTdb, LibJpl.JplTarget.Earth, LibJpl.JplTarget.Sun)

                ' Verify all six slots are finite
                For i = 0 To 5
                    If Not Double.IsFinite(state(i)) Then
                        Console.Error.WriteLine($"State({i}) is not finite: {state(i)}")
                        Environment.Exit(1)
                    End If
                Next

                ' Test frame conversions
                Dim icrsState = LibJpl.JplEphemeris.EclipticToIcrs(state, New Double() {2451545.0, 0.0})
                For i = 0 To 5
                    If Not Double.IsFinite(icrsState(i)) Then
                        Console.Error.WriteLine($"ICRS State({i}) is not finite: {icrsState(i)}")
                        Environment.Exit(1)
                    End If
                Next

                ' Test time conversion
                Dim jdTdbTt = LibJpl.JplEphemeris.TtToTdb(New Double() {2451545.0, 0.0})
                If Not Double.IsFinite(jdTdbTt(0)) OrElse Not Double.IsFinite(jdTdbTt(1)) Then
                    Console.Error.WriteLine("TT->TDB conversion failed")
                    Environment.Exit(1)
                End If

                ' Test convenience functions
                Dim planetState = eph.PlanetPosition(New Double() {2451545.0, 0.0}, LibJpl.JplTarget.Earth, LibJpl.JplTarget.Sun)
                For i = 0 To 5
                    If Not Double.IsFinite(planetState(i)) Then
                        Console.Error.WriteLine($"Planet state({i}) is not finite: {planetState(i)}")
                        Environment.Exit(1)
                    End If
                Next

                Dim moonState = eph.MoonPosition(New Double() {2451545.0, 0.0})
                For i = 0 To 5
                    If Not Double.IsFinite(moonState(i)) Then
                        Console.Error.WriteLine($"Moon state({i}) is not finite: {moonState(i)}")
                        Environment.Exit(1)
                    End If
                Next

                ' Apparent ecliptic position returns a five-double result struct;
                ' a mismatched declaration would corrupt memory instead of
                ' merely returning wrong values.
                Dim ecliptic = eph.ApparentEclipticPosition(
                    New Double() {2451545.0, 0.0}, LibJpl.JplTarget.Moon)
                If Not (ecliptic.LongitudeDeg >= 0.0 AndAlso ecliptic.LongitudeDeg < 360.0) Then
                    Console.Error.WriteLine($"Bad ecliptic longitude: {ecliptic.LongitudeDeg}")
                    Environment.Exit(1)
                End If
                If Not (Math.Abs(ecliptic.LatitudeDeg) <= 90.0) Then
                    Console.Error.WriteLine($"Bad ecliptic latitude: {ecliptic.LatitudeDeg}")
                    Environment.Exit(1)
                End If
                If Not (ecliptic.RangeAu > 0.002 AndAlso ecliptic.RangeAu < 0.003) Then
                    Console.Error.WriteLine($"Bad Moon range: {ecliptic.RangeAu}")
                    Environment.Exit(1)
                End If
                If Not Double.IsFinite(ecliptic.LongitudeRateDegPerDay) OrElse
                   Not Double.IsFinite(ecliptic.RangeRateKmPerS) OrElse
                   Math.Abs(ecliptic.RangeRateKmPerS) >= 1.0 Then
                    Console.Error.WriteLine($"Bad ecliptic rates: {ecliptic.LongitudeRateDegPerDay}, {ecliptic.RangeRateKmPerS}")
                    Environment.Exit(1)
                End If

                ' Test aberration (observer velocity in AU/day; Earth orbital speed ~0.0172 AU/day)
                Dim geomState = New Double() {1.0, 0.0, 0.0, 0.0, 0.0, 0.0}
                Dim obsVel = New Double() {0.0, 0.0172, 0.0}
                Dim aberrated = LibJpl.JplEphemeris.ApplyAberration(geomState, obsVel)
                Dim aberratedFull = LibJpl.JplEphemeris.ApplyAberrationFull(geomState, obsVel, 1.0)
                For i = 0 To 5
                    If Not Double.IsFinite(aberrated(i)) OrElse Not Double.IsFinite(aberratedFull(i)) Then
                        Console.Error.WriteLine($"Aberrated state({i}) is not finite")
                        Environment.Exit(1)
                    End If
                Next

                ' Contract §4: omitting the solar term equals an explicit 0.0.
                Dim aberratedZero = LibJpl.JplEphemeris.ApplyAberrationFull(geomState, obsVel, 0.0)
                For i = 0 To 5
                    If aberrated(i) <> aberratedZero(i) Then
                        Console.Error.WriteLine($"Aberration 0.0 mismatch at {i}")
                        Environment.Exit(1)
                    End If
                Next

                ' Contract §5: ecliptic round-trip reproduces the input.
                Dim backToEcliptic = LibJpl.JplEphemeris.IcrsToEcliptic(icrsState, New Double() {2451545.0, 0.0})
                For i = 0 To 5
                    If Math.Abs(backToEcliptic(i) - state(i)) > 1e-9 Then
                        Console.Error.WriteLine($"Ecliptic round-trip mismatch at {i}")
                        Environment.Exit(1)
                    End If
                Next

                ' Contract: rotate_vector matches the state entry's position half.
                Dim rv = LibJpl.JplEphemeris.RotateVector(New Double() {1.0, 0.0, 0.0}, New Double() {2451545.0, 0.0})
                Dim icrsUnit = LibJpl.JplEphemeris.EclipticToIcrs(New Double() {1.0, 0.0, 0.0, 0.0, 0.0, 0.0}, New Double() {2451545.0, 0.0})
                For i = 0 To 2
                    If Math.Abs(rv(i) - icrsUnit(i)) > 1e-12 Then
                        Console.Error.WriteLine($"Rotate-vector mismatch at {i}")
                        Environment.Exit(1)
                    End If
                Next

                ' Short arrays must be rejected in managed code before P/Invoke.
                Try
                    LibJpl.JplEphemeris.ApplyAberration(geomState, New Double() {0.0, 0.0172})
                    Console.Error.WriteLine("Expected ArgumentException for short observer velocity")
                    Environment.Exit(1)
                Catch ex As ArgumentException
                End Try

                ' Test light time
                Dim ltResult = eph.GetStateLighttime(New Double() {2451545.0, 0.0}, LibJpl.JplTarget.Earth, LibJpl.JplTarget.Sun)
                If Not Double.IsFinite(ltResult.LightTimeDays) OrElse ltResult.LightTimeDays <= 0 Then
                    Console.Error.WriteLine("Light time computation failed")
                    Environment.Exit(1)
                End If

                ' Test frame rotation matrix
                Dim R = LibJpl.JplEphemeris.FrameRotationMatrix(New Double() {2451545.0, 0.0})
                For i = 0 To 2
                    For j = 0 To 2
                        If Not Double.IsFinite(R(i, j)) Then
                            Console.Error.WriteLine($"Rotation matrix({i})({j}) is not finite: {R(i, j)}")
                            Environment.Exit(1)
                        End If
                    Next
                Next

                ' Test time conversion round-trip
                jdTdb = LibJpl.JplEphemeris.TtToTdb(New Double() {2451545.0, 0.0})
                Dim jdTt = LibJpl.JplEphemeris.TdbToTt(jdTdb)
                If Not Double.IsFinite(jdTt(0)) OrElse Not Double.IsFinite(jdTt(1)) Then
                    Console.Error.WriteLine("TDB->TT conversion failed")
                    Environment.Exit(1)
                End If

                ' Test clone
                Using clone = eph.Clone()
                    Dim clonedState = clone.GetState(New Double() {2451545.0, 0.0}, LibJpl.JplTarget.Earth, LibJpl.JplTarget.Sun)
                    For i = 0 To 5
                        If Math.Abs(clonedState(i) - state(i)) > 1e-12 Then
                            Console.Error.WriteLine($"Clone state mismatch at index {i}: {clonedState(i)} vs {state(i)}")
                            Environment.Exit(1)
                        End If
                    Next
                End Using

                ' Check that opening a missing path throws
                Try
                    Using badEph = LibJpl.LibJplApi.Open("nonexistent_file.eph")
                        Console.Error.WriteLine("Expected exception for missing file")
                        Environment.Exit(1)
                    End Using
                Catch ex As LibJpl.JplException
                    If ex.Status <> LibJpl.JplStatus.ErrorIo AndAlso ex.Status <> LibJpl.JplStatus.ErrorArgument Then
                        Console.Error.WriteLine($"Wrong error status for missing file: {ex.Status}")
                        Environment.Exit(1)
                    End If
                End Try

                ' Verify StatusString returns static C string safely (must not free it)
                If LibJpl.LibJplApi.StatusString(LibJpl.JplStatus.Ok) <> "success" OrElse
                   String.IsNullOrEmpty(LibJpl.LibJplApi.StatusString(LibJpl.JplStatus.ErrorRange)) Then
                    Console.Error.WriteLine("StatusString returned unexpected value")
                    Environment.Exit(1)
                End If

                Console.WriteLine("VB.NET binding test passed")
                Environment.Exit(0)
            End Using
        Catch ex As LibJpl.JplException
            Console.Error.WriteLine($"JplException: {ex.Message} (status={ex.Status})")
            Environment.Exit(1)
        Catch ex As Exception
            Console.Error.WriteLine($"Exception: {ex.Message}")
            Environment.Exit(1)
        End Try
    End Sub

End Module
' Copyright (c) 2026 Todd Carnes
' SPDX-License-Identifier: MIT
' See LICENSE in the project root for the full license text.

Imports System

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

                ' Test aberration
                Dim geomState = New Double() {1.0, 0.0, 0.0, 0.0, 0.0, 0.0}
                Dim obsVel = New Double() {0.0, 29.78, 0.0}
                Dim aberrated = LibJpl.JplEphemeris.ApplyAberration(geomState, obsVel)
                For i = 0 To 5
                    If Not Double.IsFinite(aberrated(i)) Then
                        Console.Error.WriteLine($"Aberrated state({i}) is not finite: {aberrated(i)}")
                        Environment.Exit(1)
                    End If
                Next

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
' Copyright (c) 2026 Todd Carnes
' SPDX-License-Identifier: MIT
' See LICENSE in the project root for the full license text.

' Example: Print all planet positions at J2000.0 (2000-01-01 12:00 TT)
'
' Add this file to a .NET 8 console project referencing bindings/vbnet/src/LibJpl/LibJpl.vbproj,
' and ensure jpl.dll / libjpl.so is on PATH / LD_LIBRARY_PATH.

Imports System
Imports LibJpl

Module PlanetPositions

    Sub Main(args As String())
        If args.Length < 1 Then
            Console.Error.WriteLine($"Usage: {Environment.GetCommandLineArgs()(0)} <ephemeris-file>")
            Environment.Exit(1)
        End If

        Try
            Using eph = LibJplApi.Open(args(0))

                ' J2000.0 = 2000-01-01 12:00 TT = JD 2451545.0 TT
                Dim jdTt = New Double() {2451545.0, 0.0}
                Dim jdTdb = JplEphemeris.TtToTdb(jdTt)

                Console.WriteLine("Planet positions at J2000.0 (2000-01-01 12:00 TT)")
                Console.WriteLine($"TDB: {jdTdb(0)} + {jdTdb(1)}")
                Console.WriteLine()

                ' Planet target codes (from JplTarget enum)
                Dim planets = New (Integer Code, String Name)() {
                    (1, "Mercury"), (2, "Venus"), (3, "Earth"), (4, "Mars"),
                    (5, "Jupiter"), (6, "Saturn"), (7, "Uranus"), (8, "Neptune"),
                    (9, "Pluto"), (10, "Moon"), (11, "Sun"), (12, "SSB"),
                    (13, "EMB"), (14, "Nutations"), (15, "Librations")
                }

                Console.WriteLine("{0,-12} {1,14} {2,14} {3,14} {4,14} {5,14} {6,14}",
                    "Body", "X (AU)", "Y (AU)", "Z (AU)", "VX (AU/d)", "VY (AU/d)", "VZ (AU/d)")
                Console.WriteLine(New String("-"c, 98))

                For Each planet In planets
                    If planet.Code = 0 OrElse planet.Code > 17 Then Continue For

                    Try
                        Dim state = eph.GetState(jdTdb, CType(planet.Code, JplTarget), JplTarget.Ssb)
                        Console.WriteLine("{0,-12} {1,14:F6} {2,14:F6} {3,14:F6} {4,14:F6} {5,14:F6} {6,14:F6}",
                            planet.Name, state(0), state(1), state(2), state(3), state(4), state(5))
                    Catch ex As JplException
                        Console.Error.WriteLine($"Error for {planet.Name}: {ex.Message} (status={ex.Status})")
                    End Try
                Next

                ' Earth relative to Sun
                Console.WriteLine()
                Console.WriteLine("Earth relative to Sun:")
                Dim earthState = eph.GetState(jdTdb, JplTarget.Earth, JplTarget.Sun)
                Console.WriteLine($"  X: {earthState(0):F6} Y: {earthState(1):F6} Z: {earthState(2):F6}")
                Console.WriteLine($"  VX: {earthState(3):F6} VY: {earthState(4):F6} VZ: {earthState(5):F6}")

                ' Moon position
                Console.WriteLine()
                Console.WriteLine("Moon position (Earth-centered, ICRS):")
                Dim moonState = eph.MoonPosition(New Double() {2451545.0, 0.0})
                Console.WriteLine($"  X: {moonState(0):F6} Y: {moonState(1):F6} Z: {moonState(2):F6}")
                Console.WriteLine($"  VX: {moonState(3):F6} VY: {moonState(4):F6} VZ: {moonState(5):F6}")

                ' Frame conversions (ICRS -> mean ecliptic of date)
                Console.WriteLine()
                Console.WriteLine("Frame conversion (ICRS -> Ecliptic):")
                Dim eclState = JplEphemeris.IcrsToEcliptic(earthState, jdTt)
                Console.WriteLine($"  Ecliptic: X={eclState(0):F6} Y={eclState(1):F6} Z={eclState(2):F6}")

                ' Light time
                Dim ltResult = eph.GetStateLighttime(jdTdb, JplTarget.Earth, JplTarget.Sun)
                Console.WriteLine($"Light time (Earth-Sun): {ltResult.LightTimeDays:F6} days")

                ' Time conversions
                Console.WriteLine()
                Console.WriteLine("Time conversions:")
                jdTdb = JplEphemeris.TtToTdb(jdTt)
                Dim jdTtBack = JplEphemeris.TdbToTt(jdTdb)
                Console.WriteLine($"  TT->TDB: {jdTdb(0)} + {jdTdb(1)}")
                Console.WriteLine($"  TDB->TT: {jdTtBack(0)} + {jdTtBack(1)}")

                ' Aberration (observer velocity in AU/day; Earth orbital speed ~0.0172 AU/day)
                Console.WriteLine()
                Console.WriteLine("Aberration test:")
                Dim geom = New Double() {1.0, 0.0, 0.0, 0.0, 0.0, 0.0}
                Dim obsVel = New Double() {0.0, 0.0172, 0.0}
                Dim aberrated = JplEphemeris.ApplyAberration(geom, obsVel)
                Console.WriteLine($"  Aberrated X: {aberrated(0):F6}")

            End Using
        Catch ex As JplException
            Console.Error.WriteLine($"JplException: {ex.Message} (status={ex.Status})")
            Environment.Exit(1)
        Catch ex As Exception
            Console.Error.WriteLine($"Exception: {ex.Message}")
            Environment.Exit(1)
        End Try
    End Sub

End Module
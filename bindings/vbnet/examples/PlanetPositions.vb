' Copyright (c) 2026 Todd Carnes
' SPDX-License-Identifier: MIT
' See LICENSE in the project root for the full license text.

' Example: Print all planet positions at J2000.0 (2000-01-01 12:00 TT)
'
' Build (after configuring with -DLIBJPL_BUILD_SHARED=ON -DLIBJPL_BUILD_VBNET_BINDING=ON):
'   dotnet build bindings/vbnet/examples/PlanetPositions.vbproj
'
' Run:
'   dotnet run --project bindings/vbnet/examples/PlanetPositions.vbproj /path/to/DE441.eph

Imports System
Imports LibJpl

Module PlanetPositions

    Sub Main(args As String())
        If args.Length < 1 Then
            Console.Error.WriteLine($"Usage: {Environment.GetCommandLineArgs()(0)} <ephemeris-file>")
            Environment.Exit(1)
        End If

        Dim path As String = args(0)

        Try
            Using eph = LibJplApi.Open(args(0))

                ' J2000.0 = 2000-01-01 12:00 TT = JD 2451545.0 TT
                Dim jdTdb = JplEphemeris.TtToTdb(New Double() {2451545.0, 0.0})

                Console.WriteLine("Planet positions at J2000.0 (2000-01-01 12:00 TT)")
                Console.WriteLine($"TDB: {jdTdb(0)} + {jdTdb(1)}")
                Console.WriteLine()

                ' Planet target codes (from JplTarget enum)
                Dim planets = New (Integer Code, String Name)() {
                    (1, "Mercury"), (2, "Venus"), (3, "Earth"), (4, "Mars"),
                    (5, "Jupiter"), (6, "Saturn"), (7, "Uranus"), (8, "Neptune"),
                    (9, "Pluto"), (10, "Moon"), (11, "Sun"), (12, "SSB"),
                    (13, "EMB"), (14, "Nutations"), (15, "Librations"), (17, "TT-TDB")
                }

                Console.WriteLine("{0,-12} {1,14} {2,14} {3,14} {4,14} {5,14} {6,14}",
                    "Body", "X (AU)", "Y (AU)", "Z (AU)", "VX (AU/d)", "VY (AU/d)", "VZ (AU/d)")
                Console.WriteLine(New String("-"c, 98))

                For Each planet In planets
                    If planet.Code = 0 OrElse planet.Code > 17 Then Continue For

                    Try
                        Dim state = eph.GetState(jdTdb, planet.Code, 0)
                        Console.WriteLine("{0,-12} {1,14:F6} {2,14:F6} {3,14:F6} {4,14:F6} {5,14:F6} {6,14:F6}",
                            planet.Name, state(0), state(1), state(2), state(3), state(4), state(5))
                    Catch ex As JplException
                        Console.Error.WriteLine($"Error for {planet.Name}: {ex.Message} (status={ex.Status})")
                    End Try
                Next

                ' Earth-Moon barycenter relative to Sun
                Console.WriteLine()
                Console.WriteLine("Earth-Moon barycenter relative to Sun:")
                Dim state = eph.GetState(jdTdb, JplTarget.Earth, JplTarget.Sun)
                Console.WriteLine($"  X: {state(0):F6} Y: {state(1):F6} Z: {state(2):F6}")
                Console.WriteLine($"  VX: {state(3):F6} VY: {state(4):F6} VZ: {state(5):F6}")

                ' Moon position
                Console.WriteLine()
                Console.WriteLine("Moon position (Earth-centered, ICRS):")
                Dim moonState = eph.MoonPosition(New Double() {2451545.0, 0.0})
                Console.WriteLine($"  X: {moonState(0):F6} Y: {moonState(1):F6} Z: {moonState(2):F6}")
                Console.WriteLine($"  VX: {moonState(3):F6} VY: {moonState(4):F6} VZ: {moonState(5):F6}")

                ' Frame conversions
                Console.WriteLine()
                Console.WriteLine("Frame conversion (Ecliptic -> ICRS):")
                Dim icrsState = JplEphemeris.EclipticToIcrs(state, New Double() {2451545.0, 0.0})
                Console.WriteLine($"  ICRS: X={icrsState(0):F6} Y={icrsState(1):F6} Z={icrsState(2):F6}")

                ' Light time
                Dim ltResult = eph.GetStateLighttime(New Double() {2451545.0, 0.0}, JplTarget.Earth, JplTarget.Sun)
                Console.WriteLine($"Light time (Earth-Sun): {ltResult.LightTimeDays:F6} days")

                ' Time conversions
                Console.WriteLine()
                Console.WriteLine("Time conversions:")
                Dim jdTdb = JplEphemeris.TtToTdb(New Double() {2451545.0, 0.0})
                Dim jdTt = JplEphemeris.TdbToTt(jdTdb)
                Console.WriteLine($"  TT->TDB: {jdTdb(0)} + {jdTdb(1)}")
                Console.WriteLine($"  TDB->TT: {jdTt(0)} + {jdTt(1)}")

                ' Aberration
                Console.WriteLine()
                Console.WriteLine("Aberration test:")
                Dim geom = New Double() {1.0, 0.0, 0.0, 0.0, 0.0, 0.0}
                Dim obsVel = New Double() {0.0, 29.78, 0.0}
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
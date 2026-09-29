// Copyright (c) 2026 Todd Carnes
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.

/**
 * Example: Print all planet positions at J2000.0 (2000-01-01 12:00 TT)
 *
 * Build (after configuring with -DLIBJPL_BUILD_SHARED=ON -DLIBJPL_BUILD_CSHARP_BINDING=ON):
 *   dotnet build bindings/csharp/examples/PlanetPositions.csproj
 *
 * Run:
 *   dotnet run --project bindings/csharp/examples/PlanetPositions.csproj /path/to/DE441.eph
 */

using System;
using LibJpl;

namespace LibJpl.Examples
{
    class PlanetPositions
    {
        static int Main(string[] args)
        {
            if (args.Length < 1)
            {
                Console.Error.WriteLine($"Usage: {Environment.GetCommandLineArgs()[0]} <ephemeris-file>");
                return 1;
            }

            string path = args[0];

            try
            {
                using var eph = LibJplApi.Open(args[0]);

                // J2000.0 = 2000-01-01 12:00 TT = JD 2451545.0 TT
                double[] jdTt = { 2451545.0, 0.0 };
                double[] jdTdb = JplEphemeris.TtToTdb(new double[] { 2451545.0, 0.0 });

                Console.WriteLine("Planet positions at J2000.0 (2000-01-01 12:00 TT)");
                Console.WriteLine($"TDB: {jdTdb[0]} + {jdTdb[1]}");
                Console.WriteLine();

                // Planet target codes (from JplTarget enum)
                var planets = new (int Code, string Name)[]
                {
                    (1, "Mercury"), (2, "Venus"), (3, "Earth"), (4, "Mars"),
                    (5, "Jupiter"), (6, "Saturn"), (7, "Uranus"), (8, "Neptune"),
                    (9, "Pluto"), (10, "Moon"), (11, "Sun"), (12, "SSB"),
                    (13, "EMB"), (14, "Nutations"), (15, "Librations"), (17, "TT-TDB")
                };

                Console.WriteLine("{0,-12} {1,14} {2,14} {3,14} {4,14} {5,14} {6,14}",
                    "Body", "X (AU)", "Y (AU)", "Z (AU)", "VX (AU/d)", "VY (AU/d)", "VZ (AU/d)");
                Console.WriteLine(new string('-', 98));

                foreach (var (code, name) in planets)
                {
                    if (code == 0 || code > 17) continue;

                    try
                    {
                        var state = eph.GetState(jdTdb, code, 0);
                        Console.WriteLine("{0,-12} {1,14:F6} {2,14:F6} {3,14:F6} {4,14:F6} {5,14:F6} {6,14:F6}",
                            name, state[0], state[1], state[2], state[3], state[4], state[5]);
                    }
                    catch (JplException ex)
                    {
                        Console.Error.WriteLine($"Error for {name}: {ex.Message} (status={ex.Status})");
                    }
                }

                // Earth-Moon barycenter relative to Sun
                Console.WriteLine("\nEarth-Moon barycenter relative to Sun:");
                var state = eph.GetState(jdTdb, JplTarget.Earth, JplTarget.Sun);
                Console.WriteLine($"  X: {state[0]:F6} Y: {state[1]:F6} Z: {state[2]:F6}");
                Console.WriteLine($"  VX: {state[3]:F6} VY: {state[4]:F6} VZ: {state[5]:F6}");

                // Moon position
                Console.WriteLine("\nMoon position (Earth-centered, ICRS):");
                var moonState = eph.MoonPosition(new double[] { 2451545.0, 0.0 });
                Console.WriteLine($"  X: {moonState[0]:F6} Y: {moonState[1]:F6} Z: {moonState[2]:F6}");
                Console.WriteLine($"  VX: {moonState[3]:F6} VY: {moonState[4]:F6} VZ: {moonState[5]:F6}");

                // Frame conversions
                Console.WriteLine("\nFrame conversion (Ecliptic -> ICRS):");
                var icrsState = JplEphemeris.EclipticToIcrs(state, new double[] { 2451545.0, 0.0 });
                Console.WriteLine($"  ICRS: X={icrsState[0]:F6} Y={icrsState[1]:F6} Z={icrsState[2]:F6}");

                // Light time
                var ltResult = eph.GetStateLighttime(new double[] { 2451545.0, 0.0 }, JplTarget.Earth, JplTarget.Sun);
                Console.WriteLine($"\nLight time (Earth-Sun): {ltResult.LightTimeDays:F6} days");

                // Time conversions
                Console.WriteLine("\nTime conversions:");
                var jdTdb = JplEphemeris.TtToTdb(new double[] { 2451545.0, 0.0 });
                var jdTt = JplEphemeris.TdbToTt(jdTdb);
                Console.WriteLine($"  TT->TDB: {jdTdb[0]} + {jdTdb[1]}");
                Console.WriteLine($"  TDB->TT: {jdTt[0]} + {jdTt[1]}");

                // Aberration
                Console.WriteLine("\nAberration test:");
                var geom = new double[] { 1.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
                var obsVel = new double[] { 0.0, 29.78, 0.0 };
                var aberrated = JplEphemeris.ApplyAberration(geom, obsVel);
                Console.WriteLine($"  Aberrated X: {aberrated[0]:F6}");

            }
            catch (JplException ex)
            {
                Console.Error.WriteLine($"JplException: {ex.Message} (status={ex.Status})");
                return 1;
            }
            catch (Exception ex)
            {
                Console.Error.WriteLine($"Exception: {ex.Message}");
                return 1;
            }

            return 0;
        }
    }
}
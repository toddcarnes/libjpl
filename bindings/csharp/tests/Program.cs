/* Copyright (c) 2026 Todd Carnes
 * SPDX-License-Identifier: MIT
 * See LICENSE in the project root for the full license text.
 */
using System;
using LibJpl;

class Program
{
    static void Main(string[] args)
    {
        if (args.Length < 1)
        {
            Console.Error.WriteLine($"Usage: {Environment.GetCommandLineArgs()[0]} <ephemeris-file>");
            Environment.Exit(1);
        }

        try
        {
            using var eph = LibJplApi.Open(args[0]);

            // Check ephemeris number (DE441 = 441)
            var header = eph.GetHeader();
            if (header.EphemerisNumber != 441)
            {
                Console.Error.WriteLine($"Expected ephemeris number 441, got {header.EphemerisNumber}");
                Environment.Exit(1);
            }

            // Read Earth-from-Sun at J2000.0
            var jdTdb = new double[] { 2451545.0, 0.0 };
            var state = eph.GetState(jdTdb, JplTarget.Earth, JplTarget.Sun);

            // Verify all six slots are finite
            for (int i = 0; i < 6; ++i)
            {
                if (!double.IsFinite(state[i]))
                {
                    Console.Error.WriteLine($"State[{i}] is not finite: {state[i]}");
                    Environment.Exit(1);
                }
            }

            // Test frame conversions
            var icrsState = JplEphemeris.EclipticToIcrs(state, new double[] { 2451545.0, 0.0 });
            for (int i = 0; i < 6; ++i)
            {
                if (!double.IsFinite(icrsState[i]))
                {
                    Console.Error.WriteLine($"ICRS State[{i}] is not finite: {icrsState[i]}");
                    Environment.Exit(1);
                }
            }

            // Test time conversion
            var jdTdbTt = JplEphemeris.TtToTdb(new double[] { 2451545.0, 0.0 });
            if (!double.IsFinite(jdTdbTt[0]) || !double.IsFinite(jdTdbTt[1]))
            {
                Console.Error.WriteLine("TT->TDB conversion failed");
                Environment.Exit(1);
            }

            // Test convenience functions
            var planetState = eph.PlanetPosition(new double[] { 2451545.0, 0.0 }, JplTarget.Earth, JplTarget.Sun);
            for (int i = 0; i < 6; ++i)
            {
                if (!double.IsFinite(planetState[i]))
                {
                    Console.Error.WriteLine($"Planet state[{i}] is not finite: {planetState[i]}");
                    Environment.Exit(1);
                }
            }

            var moonState = eph.MoonPosition(new double[] { 2451545.0, 0.0 });
            for (int i = 0; i < 6; ++i)
            {
                if (!double.IsFinite(moonState[i]))
                {
                    Console.Error.WriteLine($"Moon state[{i}] is not finite: {moonState[i]}");
                    Environment.Exit(1);
                }
            }

            // Test aberration
            var geomState = new double[] { 1.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
            var obsVel = new double[] { 0.0, 29.78, 0.0 };
            var aberrated = JplEphemeris.ApplyAberration(geomState, obsVel);
            for (int i = 0; i < 6; ++i)
            {
                if (!double.IsFinite(aberrated[i]))
                {
                    Console.Error.WriteLine($"Aberrated state[{i}] is not finite: {aberrated[i]}");
                    Environment.Exit(1);
                }
            }

            // Test light time
            var ltResult = eph.GetStateLighttime(new double[] { 2451545.0, 0.0 }, JplTarget.Earth, JplTarget.Sun);
            if (!double.IsFinite(ltResult.LightTimeDays) || ltResult.LightTimeDays <= 0)
            {
                Console.Error.WriteLine("Light time computation failed");
                Environment.Exit(1);
            }

            // Test frame rotation matrix
            var R = JplEphemeris.FrameRotationMatrix(new double[] { 2451545.0, 0.0 });
            for (int i = 0; i < 3; ++i)
            {
                for (int j = 0; j < 3; ++j)
                {
                    if (!double.IsFinite(R[i, j]))
                    {
                        Console.Error.WriteLine($"Rotation matrix[{i}][{j}] is not finite: {R[i, j]}");
                        Environment.Exit(1);
                    }
                }
            }

            // Test time conversion round-trip
            jdTdb = JplEphemeris.TtToTdb(new double[] { 2451545.0, 0.0 });
            var jdTt = JplEphemeris.TdbToTt(jdTdb);
            if (!double.IsFinite(jdTt[0]) || !double.IsFinite(jdTt[1]))
            {
                Console.Error.WriteLine("TDB->TT conversion failed");
                Environment.Exit(1);
            }

            // Test clone
            using var clone = eph.Clone();
            var clonedState = clone.GetState(new double[] { 2451545.0, 0.0 }, JplTarget.Earth, JplTarget.Sun);
            for (int i = 0; i < 6; ++i)
            {
                if (Math.Abs(clonedState[i] - state[i]) > 1e-12)
                {
                    Console.Error.WriteLine($"Clone state mismatch at index {i}: {clonedState[i]} vs {state[i]}");
                    Environment.Exit(1);
                }
            }

            // Check that opening a missing path throws
            try
            {
                using var badEph = LibJplApi.Open("nonexistent_file.eph");
                Console.Error.WriteLine("Expected exception for missing file");
                Environment.Exit(1);
            }
            catch (JplException ex)
            {
                if (ex.Status != JplStatus.ErrorIo && ex.Status != JplStatus.ErrorArgument)
                {
                    Console.Error.WriteLine($"Wrong error status for missing file: {ex.Status}");
                    Environment.Exit(1);
                }
            }

            Console.WriteLine("C# binding test passed");
            Environment.Exit(0);
        }
        catch (JplException ex)
        {
            Console.Error.WriteLine($"JplException: {ex.Message} (status={ex.Status})");
            Environment.Exit(1);
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine($"Exception: {ex.Message}");
            Environment.Exit(1);
        }
    }
}
/* Copyright (c) 2026 Todd Carnes
 * SPDX-License-Identifier: MIT
 * See LICENSE in the project root for the full license text.
 */
using System;
using System.Runtime.InteropServices;
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

            // The managed mirror of jpl_header must match the C layout, or
            // fields would be read from the wrong offsets.
            if (Marshal.SizeOf<JplHeaderNative>() != 3320)
            {
                Console.Error.WriteLine($"JplHeaderNative size is {Marshal.SizeOf<JplHeaderNative>()}, expected 3320");
                Environment.Exit(1);
            }
            if (Marshal.OffsetOf<JplHeaderNative>(nameof(JplHeaderNative.EphemerisNumber)).ToInt32() != 3264)
            {
                Console.Error.WriteLine($"EphemerisNumber offset is {Marshal.OffsetOf<JplHeaderNative>(nameof(JplHeaderNative.EphemerisNumber))}, expected 3264");
                Environment.Exit(1);
            }

            // Header metadata must be populated from the native struct.
            if (header.ConstantCount <= 400 || header.ConstantNames.Length != header.ConstantCount)
            {
                Console.Error.WriteLine($"Unexpected ConstantNames length {header.ConstantNames.Length} vs ConstantCount {header.ConstantCount}");
                Environment.Exit(1);
            }
            foreach (long idx in new long[] { 0, 399, 400, header.ConstantCount - 1 })
            {
                if (header.ConstantNames[idx] != eph.GetConstantName(idx))
                {
                    Console.Error.WriteLine($"Constant name mismatch at {idx}: '{header.ConstantNames[idx]}' vs '{eph.GetConstantName(idx)}'");
                    Environment.Exit(1);
                }
            }
            if (!(header.StartJd[0] < header.EndJd[0]))
            {
                Console.Error.WriteLine($"Bad coverage: {header.StartJd[0]} .. {header.EndJd[0]}");
                Environment.Exit(1);
            }
            if (header.Titles.Length != 3 || Array.Exists(header.Titles, string.IsNullOrEmpty))
            {
                Console.Error.WriteLine($"Missing header titles: {string.Join(",", header.Titles)}");
                Environment.Exit(1);
            }
            if (header.AstronomicalUnitKm < 1.49597870e8 || header.AstronomicalUnitKm > 1.49597871e8)
            {
                Console.Error.WriteLine($"Bad AU: {header.AstronomicalUnitKm}");
                Environment.Exit(1);
            }
            if (header.EarthMoonMassRatio < 81.30 || header.EarthMoonMassRatio > 81.31)
            {
                Console.Error.WriteLine($"Bad EMRAT: {header.EarthMoonMassRatio}");
                Environment.Exit(1);
            }
            // Slot 2 is EMB (0-based file slots: 0=Mercury, 1=Venus, 2=EMB).
            if (header.Pointers[2].Subintervals <= 0)
            {
                Console.Error.WriteLine("EMB pointer has no subintervals");
                Environment.Exit(1);
            }

            var constants = eph.ReadConstants();
            if (constants.Length != header.ConstantCount)
            {
                Console.Error.WriteLine($"Read {constants.Length} constants, expected {header.ConstantCount}");
                Environment.Exit(1);
            }
            foreach (double value in constants)
            {
                if (!double.IsFinite(value))
                {
                    Console.Error.WriteLine("Non-finite constant value");
                    Environment.Exit(1);
                }
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

            // Apparent ecliptic position returns a five-double result struct;
            // a mismatched declaration would corrupt memory instead of
            // merely returning wrong values.
            var ecliptic = eph.ApparentEclipticPosition(new double[] { 2451545.0, 0.0 }, JplTarget.Moon);
            if (!(ecliptic.LongitudeDeg >= 0.0 && ecliptic.LongitudeDeg < 360.0))
            {
                Console.Error.WriteLine($"Bad ecliptic longitude: {ecliptic.LongitudeDeg}");
                Environment.Exit(1);
            }
            if (!(Math.Abs(ecliptic.LatitudeDeg) <= 90.0))
            {
                Console.Error.WriteLine($"Bad ecliptic latitude: {ecliptic.LatitudeDeg}");
                Environment.Exit(1);
            }
            if (!(ecliptic.RangeAu > 0.002 && ecliptic.RangeAu < 0.003))
            {
                Console.Error.WriteLine($"Bad Moon range: {ecliptic.RangeAu}");
                Environment.Exit(1);
            }
            if (!double.IsFinite(ecliptic.LongitudeRateDegPerDay) ||
                !double.IsFinite(ecliptic.RangeRateKmPerS) ||
                Math.Abs(ecliptic.RangeRateKmPerS) >= 1.0)
            {
                Console.Error.WriteLine($"Bad ecliptic rates: {ecliptic.LongitudeRateDegPerDay}, {ecliptic.RangeRateKmPerS}");
                Environment.Exit(1);
            }

            // Test aberration (observer velocity in AU/day; Earth orbital speed ~0.0172 AU/day)
            var geomState = new double[] { 1.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
            var obsVel = new double[] { 0.0, 0.0172, 0.0 };
            var aberrated = JplEphemeris.ApplyAberration(geomState, obsVel);
            var aberratedFull = JplEphemeris.ApplyAberrationFull(geomState, obsVel, 1.0);
            for (int i = 0; i < 6; ++i)
            {
                if (!double.IsFinite(aberrated[i]) || !double.IsFinite(aberratedFull[i]))
                {
                    Console.Error.WriteLine($"Aberrated state[{i}] is not finite");
                    Environment.Exit(1);
                }
            }

            // Short arrays must be rejected in managed code before P/Invoke.
            try
            {
                JplEphemeris.ApplyAberration(geomState, new double[] { 0.0, 0.0172 });
                Console.Error.WriteLine("Expected ArgumentException for short observer velocity");
                Environment.Exit(1);
            }
            catch (ArgumentException) { }

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

            // Verify StatusString returns static C string safely (must not free it)
            if (LibJplApi.StatusString(JplStatus.Ok) != "success" ||
                string.IsNullOrEmpty(LibJplApi.StatusString(JplStatus.ErrorRange)))
            {
                Console.Error.WriteLine("StatusString returned unexpected value");
                Environment.Exit(1);
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
<!--
Copyright (c) 2026 Todd Carnes
SPDX-License-Identifier: MIT
See LICENSE in the project root for the full license text.
-->

# C# Binding Examples

## Building

```bash
# Configure with C# binding enabled
cmake -S . -B build -DLIBJPL_BUILD_SHARED=ON -DLIBJPL_BUILD_CSHARP_BINDING=ON

# Build
cmake --build build --config Debug

# Run the C# binding test suite
ctest --test-dir build -C Debug -R jpl_csharp_binding --output-on-failure
```

To run `PlanetPositions.cs` standalone, add it to a .NET 8 console project
referencing `bindings/csharp/src/LibJpl/LibJpl.csproj` and ensure
`jpl.dll` / `libjpl.so` is on `PATH` / `LD_LIBRARY_PATH`.

## Requirements

- .NET 8 SDK (`dotnet --version >= 8`)

## API Overview

The C# binding provides a safe, idiomatic .NET wrapper:

```csharp
using LibJpl;

using var eph = LibJplApi.Open("DE441.eph");

// J2000.0 = 2000-01-01 12:00 TT = JD 2451545.0 TT
double[] jdTt = { 2451545.0, 0.0 };
double[] jdTdb = JplEphemeris.TtToTdb(new double[] { 2451545.0, 0.0 });

// Get state vector (position + velocity in AU, AU/day)
var state = eph.GetState(jdTdb, JplTarget.Earth, JplTarget.Sun);

// Frame conversions (ICRS -> mean ecliptic of date -> ICRS)
var eclipticState = JplEphemeris.IcrsToEcliptic(state, jdTt);
var icrsState = JplEphemeris.EclipticToIcrs(eclipticState, jdTt);
var R = JplEphemeris.FrameRotationMatrix(jdTt);

// Time conversion round-trip
var jdTtBack = JplEphemeris.TdbToTt(jdTdb);

// Light time
var ltResult = eph.GetStateLighttime(jdTdb, JplTarget.Earth, JplTarget.Sun);

// Aberration (observer velocity in AU/day)
double[] obsVel = { 0.0, 0.0172, 0.0 };
var aberrated = JplEphemeris.ApplyAberration(state, obsVel);
var aberratedFull = JplEphemeris.ApplyAberrationFull(state, obsVel, 1.0);

// Planet/Moon positions
var planetState = eph.PlanetPosition(jdTt, JplTarget.Earth, JplTarget.Sun);
var moonState = eph.MoonPosition(jdTt);

// Apparent ecliptic position
var appPos = eph.ApparentEclipticPosition(jdTt, JplTarget.Moon);

// Clone for thread safety
using var clone = eph.Clone();

// Error handling
try {
    using var eph2 = LibJplApi.Open("DE441.eph");
} catch (JplException ex) {
    Console.WriteLine($"Error: {ex.Message} (status={ex.Status})");
}
```

## Key Types

| Type | Description |
|------|-------------|
| `JplEphemeris : SafeHandle` | RAII handle to ephemeris |
| `LibJplApi` | Static entry point (`Open`, `OpenMmap`, `StatusString`) |
| `JplEphemeris` | Instance methods for ephemeris operations |
| `Native` | P/Invoke signatures |
| `JplStatus` | Error codes enum |
| `JplTarget` | Target body codes enum |
| `JplByteOrder` | Byte order enum |
| `JplReferenceFrame` | Reference frame enum |

## Error Handling

```csharp
try {
    using var eph = LibJplApi.Open("DE441.eph");
    var state = eph.GetState(jdTdb, JplTarget.Earth, JplTarget.Sun);
} catch (JplException ex) {
    Console.WriteLine($"Error: {ex.Message} (status={ex.Status})");
    // Check specific status
    if (ex.Status == JplStatus.ErrorIo) { /* file not found */ }
    if (ex.Status == JplStatus.ErrorRange) { /* date out of range */ }
}
```

## Enums

```csharp
public enum JplTarget {
    Mercury = 1, Venus = 2, Earth = 3, Mars = 4, Jupiter = 5, Saturn = 6,
    Uranus = 7, Neptune = 8, Pluto = 9, Moon = 10, Sun = 11,
    SolarSystemBarycenter = 12, EarthMoonBarycenter = 13,
    Nutations = 14, Librations = 15, TtTdb = 17, Ssb = 0
}

public enum JplReferenceFrame {
    Unknown = 0, Dynamical1950 = 1, Dynamical2000 = 2, Icrf = 3
}

public enum JplByteOrder { LittleEndian = 0, BigEndian = 1 }
```

## Structs and Classes

```csharp
public struct ApparentEclipticResult {
    public double LongitudeDeg, LatitudeDeg, RangeAu;
    public double LongitudeRateDegPerDay, RangeRateKmPerS;
}
public struct JplPointer {
    public int StartIndex, CoefficientsPerComponent, Subintervals;
}
public class JplHeader {
    public string[] Titles { get; }
    public string[] ConstantNames { get; }
    public double[] StartJd { get; }
    public double[] EndJd { get; }
    public double StepDays { get; }
    public int ConstantCount { get; }
    public double AstronomicalUnitKm { get; }
    public double EarthMoonMassRatio { get; }
    public JplPointer[] Pointers { get; }
    public int EphemerisNumber { get; }
    public JplReferenceFrame ReferenceFrame { get; }
    public JplPointer LibrationPointer { get; }
    public JplPointer TtTdbPointer { get; }
    public int HasTtTdb { get; }
    public long RecordDoubleCount { get; }
    public JplByteOrder ByteOrder { get; }
}
```

## Running the Binding Test

```bash
ctest --test-dir build -C Debug -R jpl_csharp_binding --output-on-failure
```

## Key Differences from C API

| C API | C# Binding |
|-------|------------|
| `jpl_open` | `LibJplApi.Open` (returns `JplEphemeris`) |
| `jpl_close` | Automatic via `SafeHandle`/`using` |
| `jpl_get_state` | `eph.GetState()` returns `double[]` |
| `jpl_get_state_lighttime` | `eph.GetStateLighttime()` returns `(double[] State, double LightTimeDays)` |
| `jpl_apply_aberration` | `JplEphemeris.ApplyAberration()` static |
| `jpl_planet_position` | `eph.PlanetPosition()` |
| `jpl_moon_position` | `eph.MoonPosition()` |
| `jpl_apparent_ecliptic_position` | `eph.ApparentEclipticPosition()` returns `ApparentEclipticResult` |
| `jpl_tt_to_tdb` | `JplEphemeris.TtToTdb()` static |
| `jpl_status_string` | `LibJplApi.StatusString()` static |

## Error Handling

All methods throw `JplException` on failure, which wraps the `JplStatus` error code. The `SafeHandle` base class ensures the native handle is properly closed even if exceptions occur.
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

# Run example
dotnet run --project bindings/csharp/examples/PlanetPositions.csproj /path/to/DE441.eph
```

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

// Frame conversions
var icrsState = JplEphemeris.EclipticToIcrs(state, new double[] { 2451545.0, 0.0 });
var eclipticState = JplEphemeris.IcrsToEcliptic(state, new double[] { 2451545.0, 0.0 });
var R = JplEphemeris.FrameRotationMatrix(new double[] { 2451545.0, 0.0 });

// Time conversions
var jdTdb = JplEphemeris.TtToTdb(new double[] { 2451545.0, 0.0 });
var jdTt = JplEphemeris.TdbToTt(jdTdb);

// Light time
var ltResult = eph.GetStateLighttime(jdTdb, JplTarget.Earth, JplTarget.Sun);

// Aberration
var aberrated = JplEphemeris.ApplyAberration(geomState, obsVel);
var aberratedFull = JplEphemeris.ApplyAberrationFull(geomState, obsVel, sunDistAu);

// Planet/Moon positions
var planetState = eph.PlanetPosition(jdTt, JplTarget.Earth, JplTarget.Sun);
var moonState = eph.MoonPosition(jdTt);

// Apparent ecliptic position
var appPos = eph.ApparentEclipticPosition(jdTt, JplTarget.Moon);

// Light time
var ltResult = eph.GetStateLighttime(jdTdb, JplTarget.Earth, JplTarget.Sun);

// Clone for thread safety
using var clone = eph.Clone();

// Error handling
try {
    using var eph = LibJplApi.Open("DE441.eph");
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
| `Native` | P/Invoke signatures (internal) |
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
    Mercury = 1, Venus, Earth, Mars, Jupiter, Saturn, Uranus, Neptune,
    Pluto, Moon, Sun, SolarSystemBarycenter, EarthMoonBarycenter,
    Nutations, Librations, TtTdb, Ssb = 0
}

public enum JplReferenceFrame {
    Unknown = 0, Dynamical1950, Dynamical2000, Icrf
}

public enum JplByteOrder { LittleEndian, BigEndian }
```

## Structs

```csharp
public struct TtMinusTdbResult { public double Seconds; public double RatePerDay; }
public struct LightTimeResult { public double[] State; public double LightTimeDays; }
public struct ApparentEclipticResult {
    public double LongitudeDeg, LatitudeDeg, RangeAu;
    public double LongitudeRateDegPerDay, RangeRateKmPerS;
}
public struct JplPointer {
    public int StartIndex, CoefficientsPerComponent, Subintervals;
}
public struct JplHeader {
    public string[] Titles, ConstantNames;
    public double[] StartJd, EndJd;
    public double StepDays, ConstantCount, AstronomicalUnitKm, EarthMoonMassRatio;
    public JplPointer[] Pointers;
    public int EphemerisNumber, ReferenceFrame, HasTtTdb;
    public long RecordDoubleCount;
    public JplByteOrder ByteOrder;
    public JplPointer LibrationPointer, TtTdbPointer;
}
```

## Running the Example

```bash
# Build
dotnet build bindings/csharp/examples/PlanetPositions.csproj

# Run
dotnet run --project bindings/csharp/examples/PlanetPositions.csproj /path/to/DE441.eph
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
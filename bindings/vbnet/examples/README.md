<!--
Copyright (c) 2026 Todd Carnes
SPDX-License-Identifier: MIT
See LICENSE in the project root for the full license text.
-->

# VB.NET Binding Examples

## Building

```bash
# Configure with VB.NET binding enabled
cmake -S . -B build -DLIBJPL_BUILD_SHARED=ON -DLIBJPL_BUILD_VBNET_BINDING=ON

# Build
cmake --build build --config Debug

# Run example
dotnet run --project bindings/vbnet/examples/PlanetPositions.vbproj /path/to/DE441.eph
```

## Requirements

- .NET 8 SDK (`dotnet --version >= 8`)

## API Overview

The VB.NET binding mirrors the C# API exactly:

```vb
Imports LibJpl

Using eph = LibJplApi.Open("DE441.eph")

    ' J2000.0 = 2000-01-01 12:00 TT = JD 2451545.0 TT
    Dim jdTdb = JplEphemeris.TtToTdb(New Double() {2451545.0, 0.0})

    ' Get state vector
    Dim state = eph.GetState(jdTdb, JplTarget.Earth, JplTarget.Sun)

    ' Frame conversions
    Dim icrsState = JplEphemeris.EclipticToIcrs(state, New Double() {2451545.0, 0.0})
    Dim eclipticState = JplEphemeris.IcrsToEcliptic(state, New Double() {2451545.0, 0.0})
    Dim R = JplEphemeris.FrameRotationMatrix(New Double() {2451545.0, 0.0})

    ' Time conversions
    Dim jdTdb = JplEphemeris.TtToTdb(New Double() {2451545.0, 0.0})
    Dim jdTt = JplEphemeris.TdbToTt(jdTdb)

    ' Light time
    Dim ltResult = eph.GetStateLighttime(New Double() {2451545.0, 0.0}, JplTarget.Earth, JplTarget.Sun)

    ' Aberration
    Dim aberrated = JplEphemeris.ApplyAberration(geomState, obsVel)
    Dim aberratedFull = JplEphemeris.ApplyAberrationFull(geomState, obsVel, sunDistAu)

    ' Planet/Moon positions
    Dim planetState = eph.PlanetPosition(New Double() {2451545.0, 0.0}, JplTarget.Earth, JplTarget.Sun)
    Dim moonState = eph.MoonPosition(New Double() {2451545.0, 0.0})

    ' Apparent ecliptic position
    Dim appPos = eph.ApparentEclipticPosition(New Double() {2451545.0, 0.0}, JplTarget.Moon)

    ' Light time
    Dim ltResult = eph.GetStateLighttime(New Double() {2451545.0, 0.0}, JplTarget.Earth, JplTarget.Sun)

    ' Clone for thread safety
    Using clone = eph.Clone()

    ' Error handling
    Try
        Using eph = LibJplApi.Open("DE441.eph")
        Catch ex As JplException
            Console.WriteLine($"Error: {ex.Message} (status={ex.Status})")
        End Try
    End Using
End Using
```

## Key Types

| Type | Description |
|------|-------------|
| `JplEphemeris : SafeHandle` | RAII handle to ephemeris |
| `LibJplApi` | Module with `Open`, `OpenMmap`, `StatusString` |
| `JplEphemeris` | Instance methods for ephemeris operations |
| `Native` | P/Invoke signatures (internal) |
| `JplStatus` | Error codes enum |
| `JplTarget` | Target body codes enum |
| `JplByteOrder` | Byte order enum |
| `JplReferenceFrame` | Reference frame enum |

## Error Handling

```vb
Try
    Using eph = LibJplApi.Open("DE441.eph")
        Dim state = eph.GetState(jdTdb, JplTarget.Earth, JplTarget.Sun)
    End Using
Catch ex As JplException
    Console.WriteLine($"Error: {ex.Message} (status={ex.Status})")
    If ex.Status = JplStatus.ErrorIo Then ' file not found '
    If ex.Status = JplStatus.ErrorRange Then ' date out of range '
End Try
```

## Enums

```vb
Public Enum JplTarget
    Mercury = 1, Venus, Earth, Mars, Jupiter, Saturn, Uranus, Neptune,
    Pluto, Moon, Sun, SolarSystemBarycenter, EarthMoonBarycenter,
    Nutations, Librations, TtTdb, Ssb = 0
End Enum

Public Enum JplReferenceFrame
    Unknown = 0, Dynamical1950, Dynamical2000, Icrf
End Enum

Public Enum JplByteOrder
    LittleEndian, BigEndian
End Enum
```

## Structs

```vb
Public Structure TtMinusTdbResult
    Public Seconds As Double
    Public RatePerDay As Double
End Structure

Public Structure LightTimeResult
    Public State As Double()
    Public LightTimeDays As Double
End Structure

Public Structure ApparentEclipticResult
    Public LongitudeDeg, LatitudeDeg, RangeAu As Double
    Public LongitudeRateDegPerDay, RangeRateKmPerS As Double
End Structure

Public Structure JplPointer
    Public StartIndex, CoefficientsPerComponent, Subintervals As Integer
End Structure

Public Structure JplHeader
    Public Titles, ConstantNames As String()
    Public StartJd, EndJd As Double()
    Public StepDays, ConstantCount, AstronomicalUnitKm, EarthMoonMassRatio As Double
    Public Pointers As JplPointer()
    Public EphemerisNumber, ReferenceFrame, HasTtTdb As Integer
    Public RecordDoubleCount As Long
    Public ByteOrder As JplByteOrder
    Public LibrationPointer, TtTdbPointer As JplPointer
End Structure
```

## Running the Example

```bash
# Build
dotnet build bindings/vbnet/examples/PlanetPositions.vbproj

# Run
dotnet run --project bindings/vbnet/examples/PlanetPositions.vbproj /path/to/DE441.eph
```

## Key Differences from C API

| C API | VB.NET Binding |
|-------|----------------|
| `jpl_open` | `LibJplApi.Open` (returns `JplEphemeris`) |
| `jpl_close` | Automatic via `SafeHandle`/`Using` |
| `jpl_get_state` | `eph.GetState()` returns `Double()` |
| `jpl_get_state_lighttime` | `eph.GetStateLighttime()` returns `(Double(), Double)` tuple |
| `jpl_apply_aberration` | `JplEphemeris.ApplyAberration()` `Shared` |
| `jpl_planet_position` | `eph.PlanetPosition()` |
| `jpl_moon_position` | `eph.MoonPosition()` |
| `jpl_apparent_ecliptic_position` | `eph.ApparentEclipticPosition()` returns `ApparentEclipticResult` |
| `jpl_tt_to_tdb` | `JplEphemeris.TtToTdb()` `Shared` |
| `jpl_status_string` | `LibJplApi.StatusString()` `Shared` |

## Error Handling

All methods throw `JplException` on failure, which wraps the `JplStatus` error code. The `SafeHandle` base class ensures the native handle is properly closed even if exceptions occur.
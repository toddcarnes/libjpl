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

# Run the VB.NET binding test suite
ctest --test-dir build -C Debug -R jpl_vbnet_binding --output-on-failure
```

To run `PlanetPositions.vb` standalone, add it to a .NET 8 console project
referencing `bindings/vbnet/src/LibJpl/LibJpl.vbproj` and ensure
`jpl.dll` / `libjpl.so` is on `PATH` / `LD_LIBRARY_PATH`.

## Requirements

- .NET 8 SDK (`dotnet --version >= 8`)

## API Overview

The VB.NET binding mirrors the C# API exactly:

```vb
Imports LibJpl

Using eph = LibJplApi.Open("DE441.eph")

    ' J2000.0 = 2000-01-01 12:00 TT = JD 2451545.0 TT
    Dim jdTt = New Double() {2451545.0, 0.0}
    Dim jdTdb = JplEphemeris.TtToTdb(jdTt)

    ' Get state vector
    Dim state = eph.GetState(jdTdb, JplTarget.Earth, JplTarget.Sun)

    ' Frame conversions (ICRS -> mean ecliptic of date -> ICRS)
    Dim eclipticState = JplEphemeris.IcrsToEcliptic(state, jdTt)
    Dim icrsState = JplEphemeris.EclipticToIcrs(eclipticState, jdTt)
    Dim R = JplEphemeris.FrameRotationMatrix(jdTt)

    ' Time conversion round-trip
    Dim jdTtBack = JplEphemeris.TdbToTt(jdTdb)

    ' Light time
    Dim ltResult = eph.GetStateLighttime(jdTdb, JplTarget.Earth, JplTarget.Sun)

    ' Aberration (observer velocity in AU/day)
    Dim obsVel = New Double() {0.0, 0.0172, 0.0}
    Dim aberrated = JplEphemeris.ApplyAberration(state, obsVel)
    Dim aberratedFull = JplEphemeris.ApplyAberrationFull(state, obsVel, 1.0)

    ' Planet/Moon positions
    Dim planetState = eph.PlanetPosition(jdTt, JplTarget.Earth, JplTarget.Sun)
    Dim moonState = eph.MoonPosition(jdTt)

    ' Apparent ecliptic position
    Dim appPos = eph.ApparentEclipticPosition(jdTt, JplTarget.Moon)

    ' Clone for thread safety
    Using clone = eph.Clone()
        Dim clonedState = clone.GetState(jdTdb, JplTarget.Earth, JplTarget.Sun)
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
    If ex.Status = JplStatus.ErrorIo Then
        ' file not found
    ElseIf ex.Status = JplStatus.ErrorRange Then
        ' date out of range
    End If
End Try
```

## Enums

```vb
Public Enum JplTarget
    Ssb = 0
    Mercury = 1
    Venus = 2
    Earth = 3
    Mars = 4
    Jupiter = 5
    Saturn = 6
    Uranus = 7
    Neptune = 8
    Pluto = 9
    Moon = 10
    Sun = 11
    SolarSystemBarycenter = 12
    EarthMoonBarycenter = 13
    Nutations = 14
    Librations = 15
    TtTdb = 17
End Enum

Public Enum JplReferenceFrame
    Unknown = 0
    Dynamical1950 = 1
    Dynamical2000 = 2
    Icrf = 3
End Enum

Public Enum JplByteOrder
    LittleEndian = 0
    BigEndian = 1
End Enum
```

## Structs and Classes

```vb
Public Structure ApparentEclipticResult
    Public LongitudeDeg, LatitudeDeg, RangeAu As Double
    Public LongitudeRateDegPerDay, RangeRateKmPerS As Double
End Structure

Public Structure JplPointer
    Public StartIndex, CoefficientsPerComponent, Subintervals As Integer
End Structure

Public Class JplHeader
    Public Property Titles As String()
    Public Property ConstantNames As String()
    Public Property StartJd As Double()
    Public Property EndJd As Double()
    Public Property StepDays As Double
    Public Property ConstantCount As Integer
    Public Property AstronomicalUnitKm As Double
    Public Property EarthMoonMassRatio As Double
    Public Property Pointers As JplPointer()
    Public Property EphemerisNumber As Integer
    Public Property ReferenceFrame As JplReferenceFrame
    Public Property LibrationPointer As JplPointer
    Public Property TtTdbPointer As JplPointer
    Public Property HasTtTdb As Integer
    Public Property RecordDoubleCount As Long
    Public Property ByteOrder As JplByteOrder
End Class
```

## Running the Binding Test

```bash
ctest --test-dir build -C Debug -R jpl_vbnet_binding --output-on-failure
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
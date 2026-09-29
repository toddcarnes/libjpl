' Copyright (c) 2026 Todd Carnes
' SPDX-License-Identifier: MIT
' See LICENSE in the project root for the full license text.

Namespace LibJpl

Public Enum JplStatus
    Ok = 0
    ErrorArgument = 1
    ErrorIo = 2
    ErrorFormat = 3
    ErrorRange = 4
    ErrorMemory = 5
    ErrorUnsupported = 6
    ErrorConvergence = 7
    ErrorNumeric = 8
End Enum

Public Enum JplByteOrder
    LittleEndian = 0
    BigEndian = 1
End Enum

Public Enum JplTarget
    Mercury = 0
    Venus = 1
    Earth = 2
    Mars = 3
    Jupiter = 4
    Saturn = 5
    Uranus = 6
    Neptune = 7
    Pluto = 8
    Moon = 9
    Sun = 10
    SolarSystemBarycenter = 11
    EarthMoonBarycenter = 12
    Nutations = 13
    Librations = 14
    TtTdb = 15
    Ssb = 16
End Enum

Public Enum JplReferenceFrame
    Unknown = 0
    Dynamical1950 = 1
    Dynamical2000 = 2
    Icrf = 3
End Enum

End Namespace
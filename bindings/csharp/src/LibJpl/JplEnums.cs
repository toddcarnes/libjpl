/* Copyright (c) 2026 Todd Carnes
 * SPDX-License-Identifier: MIT
 * See LICENSE in the project root for the full license text.
 */
using System;
using System.Runtime.InteropServices;

namespace LibJpl;

public enum JplStatus
{
    Ok = 0,
    ErrorArgument = 1,
    ErrorIo = 2,
    ErrorFormat = 3,
    ErrorRange = 4,
    ErrorMemory = 5,
    ErrorUnsupported = 6,
    ErrorConvergence = 7,
    ErrorNumeric = 8,
}

public enum JplByteOrder
{
    LittleEndian = 0,
    BigEndian = 1,
}

public enum JplTarget
{
    Mercury = 1,
    Venus = 2,
    Earth = 3,
    Mars = 4,
    Jupiter = 5,
    Saturn = 6,
    Uranus = 7,
    Neptune = 8,
    Pluto = 9,
    Moon = 10,
    Sun = 11,
    SolarSystemBarycenter = 12,
    EarthMoonBarycenter = 13,
    Nutations = 14,
    Librations = 15,
    TtTdb = 17,
    Ssb = 0,
}

public enum JplReferenceFrame
{
    Unknown = 0,
    Dynamical1950 = 1,
    Dynamical2000 = 2,
    Icrf = 3,
}
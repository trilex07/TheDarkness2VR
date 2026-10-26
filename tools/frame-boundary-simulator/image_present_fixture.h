#pragma once
#include <windows.h>
#include <d3d9.h>
// An owned native DLL fixture with private methods beyond public COM lengths.
// This extent is OUR implementation, never an estimate of a D3D9 table.
struct FixtureStats {unsigned creates,queries,adds,releases,destroyed,native,overlay,privateDevice,privateChain;};
inline const GUID privateFixtureIID={0x148ae381,0x55ce,0x4835,{0x88,0x98,0x12,0x33,0x44,0x55,0x66,0x77}};

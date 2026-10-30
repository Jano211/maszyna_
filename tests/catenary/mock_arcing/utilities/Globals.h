#pragma once
// atrapa ustawień symulatora do testu iskrzenia
struct mock_globals { float AirTemperature { 15.f }; bool CatenaryArcing { true }; float CatenaryArcingScale { 1.f }; float fFogEnd { 7500.f }; float Overcast { 0.1f }; };
extern mock_globals Global;

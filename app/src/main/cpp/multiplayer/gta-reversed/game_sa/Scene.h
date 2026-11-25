/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#pragma once

#include "gta-reversed/game_sa/common.h"

extern uintptr_t g_libGTASA;

class CScene {
public:
    RpWorld*  m_pRpWorld;
    RwCamera* m_pRwCamera;
};
VALIDATE_SIZE(CScene, 0x10);

inline CScene& GetGameScene() {
    return *reinterpret_cast<CScene*>(g_libGTASA + 0xCD0868);
}

#define Scene GetGameScene()

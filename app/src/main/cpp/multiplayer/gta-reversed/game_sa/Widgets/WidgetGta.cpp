//
// Created by x1y2z on 03.02.2023.
//

#include "gta-reversed/game_sa/Widgets/WidgetGta.h"
#include "samp/main.h"
#include "samp/Multiplayer/Multiplayer.h"
#include "samp/Network/Network.h"
#include "vendor/patch/patch.h"
#include "gta-reversed/game_sa/Widgets/WidgetRegionLook.h"
#include "gta-reversed/game_sa/Widgets/TouchInterface.h"
#include "gta-reversed/game_sa/Widgets/WidgetButton.h"

extern CNetGame *pNetGame;
extern CGame *pGame;
bool bNeedEnterVehicleDriver;
void CWidgetGta::SetEnabled(bool bEnabled) {
    m_bEnabled = bEnabled;
}

void updateWidgetVisibility(CWidgetGta* widget, bool shouldShow) {
    // Disabled destructive coordinate shifting (+/- 10000.0f) which caused disappearing buttons
    if (widget && widget->m_fOriginY < -5000.0f) {
        widget->m_fOriginY += 10000.0f; // Heal any previously shifted buttons
    }
}

bool (*CWidget__IsTouched)(uintptr_t *thiz, CVector2D *pVecOut);
bool CWidget__IsTouched_hook(uintptr_t *thiz, CVector2D *pVecOut) {
    return CWidget__IsTouched(thiz, pVecOut);
}

void (*CWidgetButton__Update)(CWidgetButton* thiz);
void CWidgetButton__Update_hook(CWidgetButton* thiz) {
    if (CTouchInterface::m_pWidgets[WidgetIDs::WIDGET_RADAR]) {
        CTouchInterface::m_pWidgets[WidgetIDs::WIDGET_RADAR]->m_fTapHoldTime = 1.0;
    }
    if (CTouchInterface::m_pWidgets[WidgetIDs::WIDGET_PHONE]) {
        CTouchInterface::m_pWidgets[WidgetIDs::WIDGET_PHONE]->m_bEnabled = false; // voice
    }

    // Auto-heal any button coordinates that were corrupted by the old -10000 offset
    if (thiz && thiz->m_fOriginY < -5000.0f) {
        thiz->m_fOriginY += 10000.0f;
    }

    if(pNetGame) {
        CPlayerPed *pPlayerPed = pGame->FindPlayerPed();
        if (pPlayerPed) {
            bool isDriver = pPlayerPed->m_pPed->IsADriver();
            const auto pVehicle = pPlayerPed->GetCurrentVehicle();
            if (pVehicle && isDriver &&
                (pPlayerPed->GetCurrentWeapon() == WEAPON_MP5 || pPlayerPed->GetCurrentWeapon() == WEAPON_MICRO_UZI) &&
                (pVehicle->GetVehicleSubtype() == VEHICLE_SUBTYPE_BIKE || pVehicle->GetVehicleSubtype() == VEHICLE_SUBTYPE_PUSHBIKE))
            {
                if (CTouchInterface::m_pWidgets[WidgetIDs::WIDGET_VEHICLE_SHOOT_RIGHT])
                    CTouchInterface::m_pWidgets[WidgetIDs::WIDGET_VEHICLE_SHOOT_RIGHT]->m_bEnabled = true;
                if (CTouchInterface::m_pWidgets[WidgetIDs::WIDGET_VEHICLE_SHOOT_LEFT])
                    CTouchInterface::m_pWidgets[WidgetIDs::WIDGET_VEHICLE_SHOOT_LEFT]->m_bEnabled = true;
            }
        }

        if(CTouchInterface::m_pWidgets[WidgetIDs::WIDGET_ENTER_CAR] &&
           CTouchInterface::m_pWidgets[WidgetIDs::WIDGET_ENTER_CAR]->IsTouched(nullptr))
        {
            if (bNeedEnterVehicleDriver == false) {
                CPlayerPed *pPlayerPed = pGame->FindPlayerPed();
                if (pPlayerPed) {
                    if (!pPlayerPed->IsInVehicle()) {
                        bNeedEnterVehicleDriver = true;
                    } else {
                        pPlayerPed->ExitCurrentVehicle();
                    }
                }
            }
        }
    }

    CWidgetButton__Update(thiz);
}

void (*CWidgetRegionLook__Update)(CWidgetRegionLook *thiz);

void CWidgetRegionLook__Update_hook(CWidgetRegionLook *thiz) {
    CWidgetRegionLook__Update(thiz);

    if (thiz->m_bLookBack) {
        thiz->m_bLookBack = false;
        // CActionsPed::bPressed = true;
        return;
    }
    // CActionsPed::bPressed = false;
}

void (*CWidgetButtonEnterCar__Draw)(uintptr_t *thiz);
void CWidgetButtonEnterCar__Draw_hook(uintptr_t *thiz) {


    // if(!CHUD::bIsShowEnterExitButt)
    //  return;

    CWidgetButtonEnterCar__Draw(thiz);
}

void (*CWidgetButton__Enabled)(CWidgetButton* thiz, bool bEnabled);
void CWidgetButton__Enabled_hook(CWidgetButton* thiz, bool bEnabled) {

    if(pNetGame) {
        CPlayerPed *pPlayerPed = pGame->FindPlayerPed();
        if (pPlayerPed) {
            bool bSwimming = pPlayerPed->m_pPed->physicalFlags.bSubmergedInWater;
            bool isInVehicle = pPlayerPed->m_pPed->IsInVehicle();
            bool isInDriver = pPlayerPed->m_pPed->IsADriver();

            struct WidgetCondition {
                CWidgetGta* widget;
                bool (*condition)(bool, bool, bool);
            };

            WidgetCondition widgetConditions[] = {
                    { CTouchInterface::m_pWidgets[WidgetIDs::WIDGET_BUTTON_SWIM], [](bool swimming, bool, bool) { return swimming; }},
                    { CTouchInterface::m_pWidgets[WidgetIDs::WIDGET_BUTTON_DIVE], [](bool swimming, bool, bool) { return swimming; }},
                    { CTouchInterface::m_pWidgets[WidgetIDs::WIDGET_SPRINT], [](bool swimming, bool inVehicle, bool) { return (swimming || inVehicle) ? false : true; }},
                    { CTouchInterface::m_pWidgets[WidgetIDs::WIDGET_PED_MOVE], [](bool, bool inVehicle, bool) { return inVehicle ? false : true; }},
                    { CTouchInterface::m_pWidgets[WidgetIDs::WIDGET_ACCELERATE], [](bool swimming, bool, bool isDriver) { return (!swimming && isDriver) ? true : false; }},
                    { CTouchInterface::m_pWidgets[WidgetIDs::WIDGET_BRAKE], [](bool swimming, bool, bool isDriver) { return (!swimming && isDriver) ? true : false; }},
                    { CTouchInterface::m_pWidgets[WidgetIDs::WIDGET_HANDBRAKE], [](bool swimming, bool, bool isDriver) { return (!swimming && isDriver) ? true : false; }},
                    { CTouchInterface::m_pWidgets[WidgetIDs::WIDGET_VEHICLE_STEER_ANALOG], [](bool swimming, bool, bool isDriver) { return (!swimming && isDriver) ? true : false; }},
                    { CTouchInterface::m_pWidgets[WidgetIDs::WIDGET_HORN], [](bool swimming, bool, bool isDriver) { return (!swimming && isDriver) ? true : false; }},
            };

            for (const auto& widgetCondition : widgetConditions) {
                if (thiz == widgetCondition.widget) {
                    bEnabled = widgetCondition.condition(bSwimming, isInVehicle, isInDriver);
                    break;
                }
            }
        }
    }

    CWidgetButton__Enabled(thiz, bEnabled);
}

void CWidgetGta::InjectHooks() {
    //CHook::InstallPLT(g_libGTASA + 0x821EE0, &CWidgetRegionLook__Update_hook, &CWidgetRegionLook__Update);
    CHook::InlineHook("_ZN21CWidgetButtonEnterCar4DrawEv", &CWidgetButtonEnterCar__Draw_hook, &CWidgetButtonEnterCar__Draw);
    CHook::InlineHook("_ZN7CWidget10SetEnabledEb", &CWidgetButton__Enabled_hook, &CWidgetButton__Enabled);
    CHook::InstallPLT(g_libGTASA + 0x83EEA8, &CWidgetButton__Update_hook, &CWidgetButton__Update);
}

void CWidgetGta::SetTexture(const char *name) {
    m_Sprite.m_pTexture = CUtil::LoadTextureFromDB("mobile", name);
}

bool CWidgetGta::IsReleased(CVector2D *pVecOut) {
    return CHook::CallFunction<bool>(g_libGTASA + 0x372794, this, pVecOut);
}

bool CWidgetGta::IsTouched(CVector2D *pVecOut) {
    return CHook::CallFunction<bool>(g_libGTASA + 0x504408, this, pVecOut);
}

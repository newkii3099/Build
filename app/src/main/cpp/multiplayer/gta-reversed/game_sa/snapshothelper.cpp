#include "samp/main.h"
#include "samp/Multiplayer/Multiplayer.h"
#include "gta-reversed/game_sa/RenderWare/RenderWare.h"
#include "gta-reversed/game_sa/Streaming.h"
#include "Scene.h"
#include "VisibilityPlugins.h"
#include "gta-reversed/game_sa/Models/ModelInfo.h"
#include "gta-reversed/game_sa/Plugins/RpAnimBlendPlugin/RpAnimBlend.h"
#include <GLES2/gl2.h>

extern CGame* pGame;

CSnapShotHelper::CSnapShotHelper()
{
    m_camera = 0;
    m_light = 0;
    m_frame = 0;
    m_zBuffer = 0;
    m_raster = 0;

    SetUpScene();
}

void CSnapShotHelper::SetUpScene()
{
    m_light = RpLightCreate(2);
    if (m_light) {
        float rwColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        RpLightSetColor(m_light, reinterpret_cast<const RwRGBAReal *>(rwColor));
    }

    m_zBuffer = RwRasterCreate(256, 256, 0, rwRASTERTYPEZBUFFER);
    m_camera = RwCameraCreate();
    m_frame = RwFrameCreate();

    if (!m_camera || !m_frame || !m_zBuffer) return;

    CVector v = { 0.0f, 0.0f, 50.0f };
    RwFrameTranslate(m_frame, &v, rwCOMBINEPRECONCAT);
    CVector v1 = { 1.0f, 0.0f, 0.0f };
    RwFrameRotate(m_frame, &v1, 90.0f, rwCOMBINEPRECONCAT);

    m_camera->zBuffer = m_zBuffer;

    _rwObjectHasFrameSetFrame(m_camera, m_frame);
    RwCameraSetFarClipPlane(m_camera, 300.0f);
    RwCameraSetNearClipPlane(m_camera, 0.01f);
    RwV2d view = { 0.5f, 0.5f };
    RwCameraSetViewWindow(m_camera, &view);
    RwCameraSetProjection(m_camera, rwPERSPECTIVE);

    if (Scene.m_pRpWorld) {
        RpWorldAddCamera(Scene.m_pRpWorld, m_camera);
        m_bCameraAttached = true;
    }
}

bool CSnapShotHelper::EnsureSceneReady()
{
    if (!Scene.m_pRpWorld || !m_camera || !m_light || !m_zBuffer) {
        return false;
    }
    if (!m_bCameraAttached) {
        RpWorldAddCamera(Scene.m_pRpWorld, m_camera);
        m_bCameraAttached = true;
    }
    return true;
}

static void RestoreMainRenderWareCamera()
{
    CCamera* pTheCamera = reinterpret_cast<CCamera*>(g_libGTASA + 0x9F86F8);
    if (pTheCamera && pTheCamera->m_pRwCamera) {
        CVisibilityPlugins::SetRenderWareCamera(pTheCamera->m_pRwCamera);
    }
}

bool CSnapShotHelper::BeginCameraSnapshot(RwRaster* raster, uint32_t dwColor, bool bObjectFlags)
{
    m_camera->frameBuffer = raster;
    CVisibilityPlugins::SetRenderWareCamera(m_camera);
    RwCameraClear(m_camera, reinterpret_cast<RwRGBA*>(&dwColor), 3);

    if (!RwCameraBeginUpdate((RwCamera*)m_camera)) {
        m_camera->frameBuffer = nullptr;
        RestoreMainRenderWareCamera();
        return false;
    }

    RpWorldAddLight(Scene.m_pRpWorld, m_light);
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE, (void*)true);
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, (void*)true);
    RwRenderStateSet(rwRENDERSTATESHADEMODE, (void*)rwSHADEMODEGOURAUD);
    if (bObjectFlags) {
        RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTIONREF, (void*)0);
        RwRenderStateSet(rwRENDERSTATECULLMODE, (void*)rwCULLMODENACULLMODE);
    }
    RwRenderStateSet(rwRENDERSTATEFOGENABLE, (void*)false);
    DefinedState();
    return true;
}

void CSnapShotHelper::EndCameraSnapshot()
{
    RwCameraEndUpdate((RwCamera*)m_camera);
    RpWorldRemoveLight(Scene.m_pRpWorld, m_light);
    m_camera->frameBuffer = nullptr;
    RestoreMainRenderWareCamera();
}

// 0.3.7
RwTexture* CSnapShotHelper::CreateObjectSnapShot(int iModel, uint32_t dwColor, CVector* vecRot, float fZoom)
{
    if (iModel <= 0 || iModel >= 20000 || !EnsureSceneReady()) {
        return nullptr;
    }
    if (!CStreaming::TryLoadModel(iModel)) {
        return nullptr;
    }

    auto pRwObject = ModelInfoCreateInstance(iModel);
    if (pRwObject == nullptr) {
        return nullptr;
    }

    RwRaster* raster = RwRasterCreate(256, 256, 32, rwRASTERFORMAT8888 | rwRASTERTYPECAMERATEXTURE);
    if (!raster) {
        DestroyAtomicOrClump(reinterpret_cast<uintptr_t>(pRwObject));
        return nullptr;
    }
    RwTexture* bufferTexture = RwTextureCreate(raster);
    if (!bufferTexture) {
        RwRasterDestroy(raster);
        DestroyAtomicOrClump(reinterpret_cast<uintptr_t>(pRwObject));
        return nullptr;
    }

    auto pModelInfo = CModelInfo::GetModelInfo(iModel);
    float fRadius = (pModelInfo && pModelInfo->m_pColModel) ? pModelInfo->m_pColModel->GetBoundRadius() : 1.0f;
    CVector vecCenter = (pModelInfo && pModelInfo->m_pColModel) ? pModelInfo->m_pColModel->GetBoundCenter() : CVector{ 0.0f, 0.0f, 0.0f };
    RwFrame* parent = static_cast<RwFrame *>(pRwObject->parent);
    if (!parent) {
        RwTextureDestroy(bufferTexture);
        DestroyAtomicOrClump(reinterpret_cast<uintptr_t>(pRwObject));
        return nullptr;
    }

    float safeZoom = (fZoom > 0.0f) ? fZoom : 1.0f;
    float camDist = (-0.1f - fRadius * 2.25f) * safeZoom;
    RwV3d v = {
            -vecCenter.x,
            camDist,
            50.0f - vecCenter.z
    };
    RwFrameTranslate(parent, &v, rwCOMBINEPRECONCAT);
    if (iModel == 18631) {
        RwV3d axisY = { 0.0f, 1.0f, 0.0f };
        RwFrameRotate(parent, &axisY, 180.0f, rwCOMBINEPRECONCAT);
    }
    else if (vecRot)
    {
        if (vecRot->x != 0.0f) {
            RwV3d axisX = { 1.0f, 0.0f, 0.0f };
            RwFrameRotate(parent, &axisX, vecRot->x, rwCOMBINEPRECONCAT);
        }
        if (vecRot->y != 0.0f) {
            RwV3d axisY = { 0.0f, 1.0f, 0.0f };
            RwFrameRotate(parent, &axisY, vecRot->y, rwCOMBINEPRECONCAT);
        }
        if (vecRot->z != 0.0f) {
            RwV3d axisZ = { 0.0f, 0.0f, 1.0f };
            RwFrameRotate(parent, &axisZ, vecRot->z, rwCOMBINEPRECONCAT);
        }
    }
    if (!BeginCameraSnapshot(raster, dwColor, true)) {
        RwTextureDestroy(bufferTexture);
        DestroyAtomicOrClump(reinterpret_cast<uintptr_t>(pRwObject));
        return nullptr;
    }
    RenderClumpOrAtomic((uintptr_t)pRwObject);
    EndCameraSnapshot();
    DestroyAtomicOrClump(reinterpret_cast<uintptr_t>(pRwObject));
    return bufferTexture;
}
// 0.3.7
RwTexture* CSnapShotHelper::CreatePedSnapShot(int iModel, uint32_t dwColor, CVector* vecRot, float fZoom)
{
    if (!EnsureSceneReady()) return nullptr;
    FLog("Ped snapshot: %d", iModel);

    RwRaster* raster = RwRasterCreate(256, 256, 32, rwRASTERFORMAT8888 | rwRASTERTYPECAMERATEXTURE);
    // RwTextureCreate
    RwTexture* bufferTexture = RwTextureCreate(raster);

    CPlayerPed* pPed = nullptr;
    try {
        pPed = new CPlayerPed(208, 0, 0.0f, 0.0f, 0.0f, 0.0f);
    } catch (...) {
        pPed = nullptr;
    }

    if (!raster || !bufferTexture || !pPed || !pPed->m_pPed) {
        if (pPed) delete pPed;
        if (bufferTexture) RwTextureDestroy(bufferTexture);
        else if (raster) RwRasterDestroy(raster);
        return 0;
    }

    float posZ = iModel == 162 ? 50.15f : 50.05f;
    float posY = fZoom * -2.25f;
    pPed->m_pPed->SetPosn(0.0f, posY, posZ);
    pPed->SetModelIndex(iModel);
    //pPed->m_pPed->SetGravityProcessing(false);
    pPed->m_pPed->SetCollisionChecking(false);

    RwMatrix mat = pPed->m_pPed->GetMatrix().ToRwMatrix();

    CVector axis { 1.0f, 0.0f, 0.0f };
    if (vecRot->x != 0.0f)
    {
        RwMatrixRotate(&mat, &axis, vecRot->x);
    }
    axis.Set( 0.0f, 1.0f, 0.0f );
    if (vecRot->y != 0.0f)
    {
        RwMatrixRotate(&mat, &axis, vecRot->y);
    }
    axis.Set( 0.0f, 0.0f, 1.0f );
    if (vecRot->z != 0.0f)
    {
        RwMatrixRotate(&mat, &axis, vecRot->z);
    }

    pPed->m_pPed->SetMatrix((CMatrix&)mat);

    if (!BeginCameraSnapshot(raster, dwColor, false)) {
        delete pPed;
        RwTextureDestroy(bufferTexture);
        CStreaming::RemoveModelIfNoRefs(iModel);
        return nullptr;
    }

    pPed->m_pPed->Add();

    RpAnimBlendClumpUpdateAnimations(pPed->m_pPed->m_pRwClump, 100.0f, 1);
    RenderEntity(pPed->m_pPed);

    EndCameraSnapshot();

    pPed->m_pPed->Remove();

    delete pPed;

    CStreaming::RemoveModelIfNoRefs(iModel);

    return bufferTexture;
}

RwTexture* CSnapShotHelper::CreateVehicleSnapShot(int iModel, uint32_t dwColor, CVector* vecRot, float fZoom, uint32_t dwColor1, uint32_t dwColor2)
{
    if (!EnsureSceneReady()) return nullptr;
    RwRaster* raster = RwRasterCreate(256, 256, 32, rwRASTERFORMAT8888 | rwRASTERTYPECAMERATEXTURE);
    // RwTextureCreate
    RwTexture* bufferTexture = RwTextureCreate(raster);

    if (iModel == 570) {
        iModel = 538;
    }
    else if (iModel == 569) {
        iModel = 537;
    }

    CVehicle* pVehicle = nullptr;
    try {
        pVehicle = new CVehicle(iModel, 0.0f, 0.0f, 50.0f, 0.0f, false, false);
    } catch (...) {
        pVehicle = nullptr;
    }

    if (!raster || !bufferTexture || !pVehicle || !pVehicle->m_pVehicle) {
        FLog("somethign went wrong in snapshot");
        if (pVehicle) delete pVehicle;
        if (bufferTexture) RwTextureDestroy(bufferTexture);
        else if (raster) RwRasterDestroy(raster);
        return 0;
    }

    //pVehicle->m_pVehicle->SetGravityProcessing(false);
    pVehicle->m_pVehicle->SetCollisionChecking(false);
    auto pModelInfo = CModelInfo::GetModelInfo(iModel);
    float radius = (pModelInfo && pModelInfo->m_pColModel) ? pModelInfo->m_pColModel->GetBoundRadius() : 5.0f;
    float posY = (-1.0f - (radius + radius)) * fZoom;
    if (pVehicle->GetVehicleSubtype() == VEHICLE_SUBTYPE_BOAT) {
        posY = -5.5f - radius * 2.5f;
    }
    pVehicle->m_pVehicle->SetPosn(0.0f, posY, 50.0f);
    if (dwColor1 != 0xFFFFFFFF && dwColor2 != 0xFFFFFFFF) {
        pVehicle->SetColor(dwColor1, dwColor2);
    }
    RwMatrix mat = pVehicle->m_pVehicle->GetMatrix().ToRwMatrix();

    CVector axis { 1.0f, 0.0f, 0.0f };
    if (vecRot->x != 0.0f)
    {
        RwMatrixRotate(&mat, &axis, vecRot->x);
    }
    axis.Set( 0.0f, 1.0f, 0.0f );
    if (vecRot->y != 0.0f)
    {
        RwMatrixRotate(&mat, &axis, vecRot->y);
    }
    axis.Set( 0.0f, 0.0f, 1.0f );
    if (vecRot->z != 0.0f)
    {
        RwMatrixRotate(&mat, &axis, vecRot->z);
    }

    pVehicle->m_pVehicle->SetMatrix((CMatrix&)mat);
    pVehicle->m_pVehicle->UpdateRW();
    if (!BeginCameraSnapshot(raster, dwColor, false)) {
        delete pVehicle;
        RwTextureDestroy(bufferTexture);
        return nullptr;
    }
    pVehicle->m_pVehicle->Add();

    RenderEntity(pVehicle->m_pVehicle);
    EndCameraSnapshot();
    pVehicle->m_pVehicle->Remove();
    delete pVehicle;

    return bufferTexture;
}
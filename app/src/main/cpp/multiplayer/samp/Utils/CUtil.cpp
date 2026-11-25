//
// Created by roman on 11/19/2024.
//

#include <dlfcn.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <unordered_map>
#include <unordered_set>
#include "CUtil.h"
#include "gta-reversed/game_sa/Textures/TextureDatabaseRuntime.h"


uintptr_t CUtil::FindLib(const char* libname)
{
    void* handle = dlopen(libname, RTLD_LAZY);
    if (handle) {
        void* symbol = dlsym(handle, "JNI_OnLoad");
        if (symbol) {
            Dl_info info;
            if (dladdr(symbol, &info) != 0) {
                return reinterpret_cast<uintptr_t>(info.dli_fbase);
            }
        }
        dlclose(handle);
    }

    return 0;
}

static bool FileExistsReadable(const std::string& fullPath)
{
    FILE* f = fopen(fullPath.c_str(), "rb");
    if (f) {
        fclose(f);
        return true;
    }
    return false;
}

static std::string ToLowerAscii(const char* s)
{
    std::string out = s ? s : "";
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return (char)std::tolower(c); });
    return out;
}

RwTexture* CUtil::LoadTextureFromDB(const char* dbname, const char* texture)
{
    if (!dbname || !dbname[0] || !texture || !texture[0]) {
        return nullptr;
    }

    static std::unordered_set<std::string> s_attemptedDbs;
    std::string lowerDb = ToLowerAscii(dbname);

    TextureDatabaseRuntime* db_handle = TextureDatabaseRuntime::GetDatabase(dbname);
    if (!db_handle && lowerDb != dbname) {
        db_handle = TextureDatabaseRuntime::GetDatabase(lowerDb.c_str());
    }

    if (!db_handle && s_attemptedDbs.find(lowerDb) == s_attemptedDbs.end()) {
        s_attemptedDbs.insert(lowerDb);
        const char* candidates[2] = { dbname, lowerDb.c_str() };
        for (int i = 0; i < 2; i++) {
            if (i == 1 && lowerDb == dbname) continue;
            std::string relTxt = std::string("texdb/") + candidates[i] + "/" + candidates[i] + ".txt";
            if (FileExistsReadable(BuildStoragePath(relTxt.c_str()))) {
                FLog("[TXD] Loading custom TextureDatabase: %s", candidates[i]);
                db_handle = TextureDatabaseRuntime::Load(candidates[i], false, TextureDatabaseFormat::DF_Default);
                if (db_handle) break;
            }
        }
    }

    if (!db_handle) {
        return nullptr;
    }

    TextureDatabaseRuntime::Register(db_handle);
    auto tex = CUtil::GetTexture(texture);
    TextureDatabaseRuntime::Unregister(db_handle);

    return tex;
}

RwTexture* CUtil::LoadTextureFromTxdFile(const char* txdName, const char* texture)
{
    if (!txdName || !txdName[0] || !texture || !texture[0]) {
        return nullptr;
    }

    static std::unordered_map<std::string, uintptr_t> s_txdCache;
    std::string lowerTxd = ToLowerAscii(txdName);

    auto it = s_txdCache.find(lowerTxd);
    if (it == s_txdCache.end()) {
        uintptr_t dict = 0;
        const char* names[2] = { txdName, lowerTxd.c_str() };
        const char* dirs[] = { "models/txd/", "txd/", "texdb/", "SAMP/txd/", "SAMP/", "models/" };

        for (int n = 0; n < 2 && !dict; n++) {
            if (n == 1 && lowerTxd == txdName) continue;
            for (const char* dir : dirs) {
                std::string fullPath = BuildStoragePath((std::string(dir) + names[n] + ".txd").c_str());
                if (FileExistsReadable(fullPath)) {
                    dict = ((uintptr_t(*)(const char*))(g_libGTASA + 0x5500A4))(fullPath.c_str()); // CFileLoader::LoadTexDictionary
                    FLog("[TXD] Loaded custom .txd file '%s' -> 0x%lx", fullPath.c_str(), (unsigned long)dict);
                    break;
                }
            }
        }
        s_txdCache[lowerTxd] = dict;
        it = s_txdCache.find(lowerTxd);
    }

    uintptr_t dict = it->second;
    if (!dict) {
        return nullptr;
    }

    RwTexture* tex = ((RwTexture*(*)(uintptr_t, const char*))(g_libGTASA + 0x76CD30))(dict, texture); // RwTexDictionaryFindNamedTexture
    if (tex) {
        ++tex->refCount;
    }
    return tex;
}

RwTexture* CUtil::GetTexture(const char* name)
{
    auto tex = TextureDatabaseRuntime::GetTexture(name);
    if (!tex)
    {
        //tex = CUtil::LoadTextureFromDB("gta3", "ahoodfence2");
        FLog("WARNING! No tex = %s", name);
        return nullptr;
    }
    ++tex->refCount;

    return tex;
}

void __fastcall CUtil::TransformPoint(RwV3d &result, const CSimpleTransform &t, const RwV3d &v)
{
    float cos_heading = cosf(t.m_fHeading);
    float sin_heading = sinf(t.m_fHeading);

    result = {
            t.m_vPosn.x + cos_heading * v.x - sin_heading * v.y,
            t.m_vPosn.y + sin_heading * v.x + cos_heading * v.y,
            v.z + t.m_vPosn.z
    };
}

float CUtil::GetDistanceBetween3DPoints(const RwV3d f, const RwV3d s)
{
    return sqrt(pow(s.x - f.x, 2) + pow(s.y - f.y, 2) + pow(s.z - f.z, 2));
}
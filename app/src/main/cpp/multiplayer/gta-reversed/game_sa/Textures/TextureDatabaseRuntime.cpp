//
// Created by x1y2z on 11.01.2023.
//

#include "TextureDatabaseRuntime.h"
#include "vendor/patch/patch.h"

TextureDatabaseRuntime* TextureDatabaseRuntime::Load(const char *withName, bool fullyLoad, TextureDatabaseFormat forcedFormat)
{
    return CHook::CallFunction<TextureDatabaseRuntime*>(g_libGTASA + 0x797A84, withName, fullyLoad, forcedFormat);
}

void TextureDatabaseRuntime::Register(TextureDatabaseRuntime *toRegister) {
    if (!toRegister) return;
    CHook::CallFunction<void>(g_libGTASA + 0x7967D8, toRegister);
}

void TextureDatabaseRuntime::Unregister(TextureDatabaseRuntime *toUnregister) {
    if (!toUnregister) return;
    CHook::CallFunction<void>(g_libGTASA + 0x7968C4, toUnregister);
}

RwTexture* TextureDatabaseRuntime::GetTexture(const char *name) {
    if (!name || name[0] == '\0') return nullptr;
    return ((RwTexture*(*)(const char*))(g_libGTASA + 0x79693C))(name);
}

void TextureDatabaseRuntime::UpdateStreaming(float deltaTime, bool flush) {
    CHook::CallFunction<void>(g_libGTASA + 0x795E3C, deltaTime, flush);
}

TextureDatabaseRuntime* TextureDatabaseRuntime::GetDatabase(const char *dbName) {
    if (!dbName || dbName[0] == '\0') return nullptr;
    return CHook::CallFunction<TextureDatabaseRuntime*>(g_libGTASA + 0x797E90, dbName);
}

void TextureDatabaseRuntime::InjectHooks() {
//    CHook::Write(g_libGTASA + 0x84A020, &detailTextures);
//    CHook::Write(g_libGTASA + 0x84AEB0, &storedTexels);
//    CHook::Write(g_libGTASA + 0x850EE8, &loaded);
//    CHook::Write(g_libGTASA + 0x851CF8, &registered);
}

RwBool TextureAnnihilate(RwTexture *texture) {
    return CHook::CallFunction<RwBool>(g_libGTASA + 0x76C568, texture);
}

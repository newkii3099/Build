#include "JNIUtil.h"
#include "samp/Multiplayer/Multiplayer.h"
extern CGame *pGame;

JNIEnv* CJavaWrapper::GetEnv()
{
    JNIEnv* env = nullptr;
    int getEnvStat = javaVM->GetEnv((void**)& env, JNI_VERSION_1_4);

    if (getEnvStat == JNI_EDETACHED)
    {
        FLog("GetEnv: not attached");
        if (javaVM->AttachCurrentThread(&env, NULL) != 0)
        {
            FLog("Failed to attach");
            return nullptr;
        }
    }
    if (getEnvStat == JNI_EVERSION)
    {
        FLog("GetEnv: version not supported");
        return nullptr;
    }

    if (getEnvStat == JNI_ERR)
    {
        FLog("GetEnv: JNI_ERR");
        return nullptr;
    }

    return env;
}

CJavaWrapper::CJavaWrapper(JNIEnv *env, jobject activity)
{
    this->activity = env->NewGlobalRef(activity);

    jclass clas = env->GetObjectClass(activity);
    if(!clas)
    {
        FLog("no clas");
        return;
    }

    s_showLoadingScreen = env->GetMethodID(clas, "showLoadingScreen", "()V");
    s_hideLoadingScreen = env->GetMethodID(clas, "hideLoadingScreen", "()V");

    s_setPauseState = env->GetMethodID(clas, "setPauseState", "(Z)V");
    
    s_ShowDialog = env->GetMethodID(clas, "showDialog", "(II[B[B[B[B)V");

	s_showInputLayout = env->GetMethodID(clas, "showKeyboard", "()V");
    s_hideInputLayout = env->GetMethodID(clas, "hideKeyboard", "()V");

    s_exitGame = env->GetMethodID(clas, "exitGame", "()V");

    s_showEditObject = env->GetMethodID(clas, "showEditObject", "()V");
    s_hideEditObject = env->GetMethodID(clas, "hideEditObject", "()V");

    s_showTabWindow = env->GetMethodID(clas, "showTabWindow", "()V");
    s_addChatMessage = env->GetMethodID(clas, "addChatMessage", "([B[BII)V");
    if (env->ExceptionCheck()) env->ExceptionClear();
    s_setTabStat = env->GetMethodID(clas, "setTabStat", "(ILjava/lang/String;II)V");

    s_updateHudInfo = env->GetMethodID(clas, "updateHudInfo", "(IIIIIIIIII)V");
    s_showHud = env->GetMethodID(clas, "showHud", "()V");
    s_hideHud = env->GetMethodID(clas, "hideHud", "()V");

    s_updateSpeedInfo = env->GetMethodID(clas, "updateSpeedInfo", "(IIIIIIII)V");
    s_showSpeed = env->GetMethodID(clas, "showSpeed", "()V");
    s_hideSpeed = env->GetMethodID(clas, "hideSpeed", "()V");

    s_clearInventory = env->GetMethodID(clas, "clearInventorySync", "()V");
    if (env->ExceptionCheck()) env->ExceptionClear();
    s_setInventoryBagSlot = env->GetMethodID(clas, "setInventoryBagSlot", "(II[BII)V");
    if (env->ExceptionCheck()) env->ExceptionClear();
    s_setInventoryHotbarSlot = env->GetMethodID(clas, "setInventoryHotbarSlot", "(II[BI)V");
    if (env->ExceptionCheck()) env->ExceptionClear();
    s_refreshInventory = env->GetMethodID(clas, "refreshInventorySync", "()V");
    if (env->ExceptionCheck()) env->ExceptionClear();

    env->DeleteLocalRef(clas);
}

void CJavaWrapper::ShowKeyboard()
{
    JNIEnv* p = GetEnv();
    if (!p || !activity || !s_showInputLayout) return;
    p->CallVoidMethod(activity, s_showInputLayout);
    EXCEPTION_CHECK(p);
}

void CJavaWrapper::HideKeyboard()
{
    JNIEnv* p = GetEnv();
    if (!p || !activity || !s_hideInputLayout) return;
    p->CallVoidMethod(activity, s_hideInputLayout);
    EXCEPTION_CHECK(p);
}

void CJavaWrapper::ShowLoadingScreen()
{
    JNIEnv* p = GetEnv();
    if (!p || !activity || !s_showLoadingScreen) return;
    p->CallVoidMethod(activity, s_showLoadingScreen);
    EXCEPTION_CHECK(p);
}

void CJavaWrapper::HideLoadingScreen()
{
    JNIEnv* p = GetEnv();
    if (!p || !activity || !s_hideLoadingScreen) return;
    p->CallVoidMethod(activity, s_hideLoadingScreen);
    EXCEPTION_CHECK(p);
}

void CJavaWrapper::SetPauseState(bool pause)
{
    JNIEnv* p = GetEnv();
    if (!p || !activity || !s_setPauseState) return;
    p->CallVoidMethod(activity, s_setPauseState, pause);
    EXCEPTION_CHECK(p);
}

void CJavaWrapper::ShowDialog(int dialogStyle, int dialogID, char* title, char* text, char* button1, char* button2)
{
	JNIEnv* env = GetEnv();
	if (!env || !activity || !s_ShowDialog) return;

	std::string sTitle(title ? title : "");
	std::string sText(text ? text : "");
	std::string sButton1(button1 ? button1 : "");
	std::string sButton2(button2 ? button2 : "");

	jbyteArray jstrTitle = as_byte_array((unsigned char*)sTitle.c_str(), sTitle.length());
	jbyteArray jstrText = as_byte_array((unsigned char*)sText.c_str(), sText.length());
	jbyteArray jstrButton1 = as_byte_array((unsigned char*)sButton1.c_str(), sButton1.length());
	jbyteArray jstrButton2 = as_byte_array((unsigned char*)sButton2.c_str(), sButton2.length());

	env->CallVoidMethod(activity, s_ShowDialog, dialogID, dialogStyle, jstrTitle, jstrText, jstrButton1, jstrButton2);

	if (jstrTitle) env->DeleteLocalRef(jstrTitle);
	if (jstrText) env->DeleteLocalRef(jstrText);
	if (jstrButton1) env->DeleteLocalRef(jstrButton1);
	if (jstrButton2) env->DeleteLocalRef(jstrButton2);

	EXCEPTION_CHECK(env);
}

void CJavaWrapper::UpdateHudInfo(int health, int armour, int weaponid, int ammo, int ammoinclip,
                                 int money, int eat, int drink, int bankMoney, int playerid)
{
    JNIEnv* env = GetEnv();
    if (!env || !this->activity || !this->s_updateHudInfo) return;

    env->CallVoidMethod(this->activity, this->s_updateHudInfo,
                        health, armour, weaponid, ammo, ammoinclip,
                        money, eat, drink, bankMoney, playerid);
    EXCEPTION_CHECK(env);
}

void CJavaWrapper::ShowHud()
{
    JNIEnv* env = GetEnv();
    if (!env || !this->activity || !this->s_showHud) return;
    env->CallVoidMethod(this->activity, this->s_showHud);
    EXCEPTION_CHECK(env);
}

void CJavaWrapper::HideHud()
{
    JNIEnv* env = GetEnv();
    if (!env || !this->activity || !this->s_hideHud) return;
    env->CallVoidMethod(this->activity, this->s_hideHud);
    EXCEPTION_CHECK(env);
}

void CJavaWrapper::ShowSpeed()
{
    // Permanently disabled
    return;
}

void CJavaWrapper::HideSpeed()
{
    JNIEnv* env = GetEnv();
    if (!env || !this->activity || !this->s_hideSpeed) return;
    env->CallVoidMethod(this->activity, this->s_hideSpeed);
}

void CJavaWrapper::UpdateSpeedInfo(int speed, int fuel, int hp, int mileage, int engine, int light, int belt, int lock)
{
    JNIEnv* env = GetEnv();
    if (!env || !this->activity || !this->s_updateSpeedInfo) return;
    env->CallVoidMethod(this->activity, this->s_updateSpeedInfo, speed, fuel, hp, mileage, engine, light, belt, lock);
}

bool CJavaWrapper::AddChatMessage(const std::string& nick, const std::string& text, uint32_t nickRgb, uint32_t textRgb)
{
    if (!s_addChatMessage) return false;
    JNIEnv* env = GetEnv();
    if (!env) return false;

    jbyteArray jNick = as_byte_array((unsigned char*)nick.data(), (int)nick.size());
    jbyteArray jText = as_byte_array((unsigned char*)text.data(), (int)text.size());
    if (!jNick || !jText) {
        if (jNick) env->DeleteLocalRef(jNick);
        if (jText) env->DeleteLocalRef(jText);
        return false;
    }
    env->CallVoidMethod(activity, s_addChatMessage, jNick, jText, (jint)nickRgb, (jint)textRgb);
    env->DeleteLocalRef(jNick);
    env->DeleteLocalRef(jText);

    if (env->ExceptionCheck()) { env->ExceptionClear(); return false; }
    return true;
}

void CJavaWrapper::ShowTabWindow()
{
    JNIEnv* env = GetEnv();
    if (!env || !this->activity || !this->s_showTabWindow) return;
    env->CallVoidMethod(this->activity, this->s_showTabWindow);
    EXCEPTION_CHECK(env);
}

void CJavaWrapper::SetTabStat(int id, char* name, int score, int ping) {

    JNIEnv* env = GetEnv();
    if (!env || !this->activity || !this->s_setTabStat) return;

    jclass strClass = env->FindClass("java/lang/String");
    jmethodID ctorID = env->GetMethodID(strClass, "<init>", "([BLjava/lang/String;)V");
    jstring encoding = env->NewStringUTF("UTF-8");

    jbyteArray bytes = env->NewByteArray(strlen(name));
    env->SetByteArrayRegion(bytes, 0, strlen(name), (jbyte*)name);
    jstring jname = (jstring) env->NewObject(strClass, ctorID, bytes, encoding);

    env->CallVoidMethod(this->activity, this->s_setTabStat, id, jname, score, ping);

    if (jname) env->DeleteLocalRef(jname);
    if (bytes) env->DeleteLocalRef(bytes);
    if (encoding) env->DeleteLocalRef(encoding);
    if (strClass) env->DeleteLocalRef(strClass);
    EXCEPTION_CHECK(env);
}

void CJavaWrapper::exitGame() {

    JNIEnv* env = GetEnv();
    if (!env || !this->activity || !this->s_exitGame) return;
    env->CallVoidMethod(this->activity, this->s_exitGame);
    EXCEPTION_CHECK(env);
}

void CJavaWrapper::ShowEditObject() {

    JNIEnv* env = GetEnv();
    if (!env || !this->activity || !this->s_showEditObject) return;
    env->CallVoidMethod(this->activity, this->s_showEditObject);
    EXCEPTION_CHECK(env);
}

void CJavaWrapper::HideEditObject() {

    JNIEnv* env = GetEnv();
    if (!env || !this->activity || !this->s_hideEditObject) return;
    env->CallVoidMethod(this->activity, this->s_hideEditObject);
    EXCEPTION_CHECK(env);
}

void CJavaWrapper::ClearInventorySync()
{
    JNIEnv* env = GetEnv();
    if (!env || !this->activity || !this->s_clearInventory) return;
    env->CallVoidMethod(this->activity, this->s_clearInventory);
    EXCEPTION_CHECK(env);
}

void CJavaWrapper::SetInventoryBagSlot(int slot, int itemId, const std::string& nameUtf8, int amount, int maxStack)
{
    JNIEnv* env = GetEnv();
    if (!env || !this->activity || !this->s_setInventoryBagSlot) return;
    jbyteArray jName = as_byte_array((unsigned char*)nameUtf8.data(), (int)nameUtf8.size());
    if (!jName) return;
    env->CallVoidMethod(this->activity, this->s_setInventoryBagSlot, slot, itemId, jName, amount, maxStack);
    env->DeleteLocalRef(jName);
    EXCEPTION_CHECK(env);
}

void CJavaWrapper::SetInventoryHotbarSlot(int hotbarSlot, int itemId, const std::string& nameUtf8, int amount)
{
    JNIEnv* env = GetEnv();
    if (!env || !this->activity || !this->s_setInventoryHotbarSlot) return;
    jbyteArray jName = as_byte_array((unsigned char*)nameUtf8.data(), (int)nameUtf8.size());
    if (!jName) return;
    env->CallVoidMethod(this->activity, this->s_setInventoryHotbarSlot, hotbarSlot, itemId, jName, amount);
    env->DeleteLocalRef(jName);
    EXCEPTION_CHECK(env);
}

void CJavaWrapper::RefreshInventorySync()
{
    JNIEnv* env = GetEnv();
    if (!env || !this->activity || !this->s_refreshInventory) return;
    env->CallVoidMethod(this->activity, this->s_refreshInventory);
    EXCEPTION_CHECK(env);
}
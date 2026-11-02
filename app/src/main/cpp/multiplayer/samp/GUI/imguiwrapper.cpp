#include "samp/main.h"
#include "imguiwrapper.h"
#include "uisettings.h"
#include <unistd.h>

#include "samp/Multiplayer/Multiplayer.h"
#include "samp/Network/Network.h"

// voice
#include "samp/Voice/MicroIcon.h"
#include "samp/Voice/SpeakerList.h"
#include "samp/Voice/include/util/Render.h"


extern UI* pUI;
extern CNetGame* pNetGame;


ImGuiWrapper::ImGuiWrapper(const ImVec2& display_size, const std::string& font_path)
{
	m_displaySize = display_size;
	m_renderer = 0;
	m_fontRaster = nullptr;
	m_fontPath = font_path;

	m_vertexBuffer = nullptr;
	m_vertexBufferSize = 10000;
}

ImGuiWrapper::~ImGuiWrapper()
{
	shutdown();
}

bool ImGuiWrapper::initialize()
{
    Log::traceLastFunc("ImGuiWrapper::initialize");

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = m_displaySize;
    io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);

    Log::addParameter("Loading font: %s", m_fontPath.c_str());
    ImFontConfig fontCfg;
    fontCfg.GlyphExtraSpacing.x = UISettings::outlineSize();

    static const ImWchar ranges[] =
    {
        0x0020, 0x00FF, // Basic Latin + Latin Supplement
        0x0400, 0x04FF, // Cyrillic
        0x0E00, 0x0E7F, // Full Thai (Consonants, Vowels, Tone marks, Numerals)
        0x0E80, 0x0EFF, // Full Lao (Consonants, Vowels, Tone marks, Numerals)
        0x2DE0, 0x2DFF, // Cyrillic Extended-A
        0xA640, 0xA69F, // Cyrillic Extended-B
        0xF020, 0xF0FF, // Half-width characters
        0x2010, 0x205E, // Punctuations
        0
    };

    // Base font: Prioritize Prompt-Regular.ttf (matching HTML font styling)
    std::string sampPromptFont = BuildStoragePath("SAMP/fonts/Prompt-Regular.ttf");
    const char* baseFontCandidates[] = {
        sampPromptFont.c_str(),
        "/data/data/com.kurdish.roleplay/files/Prompt-Regular.ttf",
        m_fontPath.c_str(),
        "/system/fonts/Roboto-Regular.ttf",
        nullptr
    };

    ImFont* font = nullptr;
    for (int b = 0; baseFontCandidates[b]; b++) {
        if (access(baseFontCandidates[b], R_OK) == 0) {
            font = io.Fonts->AddFontFromFileTTF(baseFontCandidates[b],
                                                UISettings::fontSize(), &fontCfg, ranges);
            if (font != nullptr) {
                Log::addParameter("Loaded base font: %s", baseFontCandidates[b]);
                break;
            }
        }
    }

    if (font == nullptr)
    {
        Log::addParameter("Base font failed, fallback to system font: %s", m_fontPath.c_str());
        font = io.Fonts->AddFontFromFileTTF("/system/fonts/Roboto-Regular.ttf",
                                            UISettings::fontSize(), &fontCfg, ranges);
        if (font == nullptr) return false;
    }

    // Merge Android System Noto Sans Thai Font for sharp modern Thai rendering
    ImFontConfig mergeCfgThai;
    mergeCfgThai.MergeMode = true;
    static const ImWchar thaiRanges[] = { 0x0E00, 0x0E7F, 0 };
    const char* thaiFontPaths[] = {
        "/system/fonts/NotoSansThai-Bold.ttf",
        "/system/fonts/NotoSansThaiUI-Bold.ttf",
        "/system/fonts/NotoSansThai-Regular.ttf",
        "/system/fonts/NotoSansThaiUI-Regular.ttf",
        nullptr
    };
    for (int t = 0; thaiFontPaths[t]; t++) {
        if (access(thaiFontPaths[t], R_OK) == 0) {
            io.Fonts->AddFontFromFileTTF(thaiFontPaths[t], UISettings::fontSize(), &mergeCfgThai, thaiRanges);
            Log::addParameter("Merged Thai system font: %s", thaiFontPaths[t]);
            break;
        }
    }

    // Merge Android System Noto Sans Lao Font for sharp modern Lao rendering (with assets fallback)
    ImFontConfig mergeCfgLao;
    mergeCfgLao.MergeMode = true;
    static const ImWchar laoRanges[] = { 0x0E80, 0x0EFF, 0 };
    std::string sampLaoFont = BuildStoragePath("SAMP/fonts/NotoSansLao-Regular.ttf");
    const char* laoFontPaths[] = {
        "/system/fonts/NotoSansLao-Bold.ttf",
        "/system/fonts/NotoSansLaoUI-Bold.ttf",
        "/system/fonts/NotoSansLao-Regular.ttf",
        "/system/fonts/NotoSansLaoUI-Regular.ttf",
        sampLaoFont.c_str(),
        "/data/data/com.kurdish.roleplay/files/NotoSansLao-Regular.ttf",
        nullptr
    };
    for (int l = 0; laoFontPaths[l]; l++) {
        if (access(laoFontPaths[l], R_OK) == 0) {
            io.Fonts->AddFontFromFileTTF(laoFontPaths[l], UISettings::fontSize(), &mergeCfgLao, laoRanges);
            Log::addParameter("Merged Lao font: %s", laoFontPaths[l]);
            break;
        }
    }

    createFontTexture();

    m_renderer = new ImGuiRenderer(ImGui::GetBackgroundDrawList(), font);

    // voice
	for (const auto& deviceInitCallback : CVoiceRender::deviceInitCallbacks) {
		if (deviceInitCallback != nullptr) {
			deviceInitCallback();
		}
	}

    return true;
}


void ImGuiWrapper::render()
{
	ImGui::NewFrame();


	// voice
	/*if (pNetGame) {

		if (pUI->dialog()->visible() || pUI->playertablist()->visible() || pNetGame->GetTextDrawPool()->GetState()) {
			SpeakerList::Hide();
			MicroIcon::Hide();
		}
		else if (MicroIcon::hasShowed) {
			SpeakerList::Show();
			MicroIcon::Show();
		}

		for (const auto& renderCallback : Render::renderCallbacks) {
			if (renderCallback != nullptr) {
				renderCallback();
			}
		}
	}*/

	drawList();
	ImGui::EndFrame();
	ImGui::Render();
	renderDrawData(ImGui::GetDrawData());
}

void ImGuiWrapper::shutdown()
{
	Log::traceLastFunc("ImGuiWrapper::shutdown");

	// voice
	for (const auto& deviceFreeCallback : CVoiceRender::deviceFreeCallbacks) {
		if (deviceFreeCallback != nullptr) {
			deviceFreeCallback();
		}
	}

	destroyFontTexture();
}

void ImGuiWrapper::setupRenderState(ImDrawData* draw_data)
{
	RwRenderStateSet(rwRENDERSTATEZTESTENABLE, (void*)0);
	RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, (void*)0);
	RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, (void*)1);
	RwRenderStateSet(rwRENDERSTATESRCBLEND, (void*)rwBLENDSRCALPHA);
	RwRenderStateSet(rwRENDERSTATEDESTBLEND, (void*)rwBLENDINVSRCALPHA);
	RwRenderStateSet(rwRENDERSTATEFOGENABLE, (void*)0);
	RwRenderStateSet(rwRENDERSTATECULLMODE, (void*)rwCULLMODECULLNONE);
	RwRenderStateSet(rwRENDERSTATEBORDERCOLOR, (void*)0);
	RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTION, (void*)rwALPHATESTFUNCTIONGREATER);
	RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTIONREF, (void*)2);
	RwRenderStateSet(rwRENDERSTATETEXTUREFILTER, (void*)rwFILTERLINEAR);
	RwRenderStateSet(rwRENDERSTATETEXTUREADDRESS, (void*)rwTEXTUREADDRESSCLAMP);

}

void ImGuiWrapper::checkVertexBuffer(ImDrawData* draw_data)
{
	if (m_vertexBuffer == nullptr || m_vertexBufferSize < draw_data->TotalVtxCount)
	{
		Log::traceLastFunc("ImGuiWrapper::checkVertexBuffer");

		if (m_vertexBuffer)
		{
			delete m_vertexBuffer;
			m_vertexBuffer = nullptr;
		}

		m_vertexBufferSize = draw_data->TotalVtxCount + 10000;
		m_vertexBuffer = new RwIm2DVertex[m_vertexBufferSize];
		if (!m_vertexBuffer)
		{
			Log::addParameter("Buffer allocation error. Size", m_vertexBufferSize);
			return;
		}

		Log::addParameter("Vertex buffer reallocated. Size", m_vertexBufferSize);
	}

}

#include "gta-reversed/game_sa/Sprite2D.h"
void ImGuiWrapper::renderDrawData(ImDrawData* draw_data)
{
	const RwReal nearScreenZ = CSprite2d::NearScreenZ;
	const RwReal recipNearClip = CSprite2d::RecipNearClip;

	checkVertexBuffer(draw_data);
	setupRenderState(draw_data);

	RwIm2DVertex* vtx_dst = m_vertexBuffer;
	int vtx_offset = 0;

	for (int n = 0; n < draw_data->CmdListsCount; n++)
	{
		const ImDrawList* cmd_list = draw_data->CmdLists[n];
		const ImDrawVert* vtx_src = cmd_list->VtxBuffer.Data;
		const ImDrawIdx* idx_src = cmd_list->IdxBuffer.Data;

		for (int i = 0; i < cmd_list->VtxBuffer.Size; i++)
		{
			RwIm2DVertexSetScreenX(vtx_dst, vtx_src->pos.x);
			RwIm2DVertexSetScreenY(vtx_dst, vtx_src->pos.y);
			RwIm2DVertexSetScreenZ(vtx_dst, nearScreenZ);
			RwIm2DVertexSetRecipCameraZ(vtx_dst, recipNearClip);
			vtx_dst->emissiveColor = vtx_src->col;
			RwIm2DVertexSetU(vtx_dst, vtx_src->uv.x, recipCameraZ);
			RwIm2DVertexSetV(vtx_dst, vtx_src->uv.y, recipCameraZ);

			vtx_dst++;
			vtx_src++;
		}

		const ImDrawIdx* idx_buffer = cmd_list->IdxBuffer.Data;
		for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
		{
			const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
			if (pcmd->UserCallback)
			{
				pcmd->UserCallback(cmd_list, pcmd);
			}
			else
			{
				ImVec4 clip_rect;
				clip_rect.x = pcmd->ClipRect.x;
				clip_rect.y = pcmd->ClipRect.w;
				clip_rect.z = pcmd->ClipRect.z;
				clip_rect.w = pcmd->ClipRect.y;
				SetScissorRect((void*)&clip_rect);

				RwRenderStateSet(rwRENDERSTATETEXTURERASTER, (void*)pcmd->TextureId);
				RwIm2DRenderIndexedPrimitive(rwPRIMTYPETRILIST,
					&m_vertexBuffer[vtx_offset], (RwInt32)cmd_list->VtxBuffer.Size,
					(RwImVertexIndex*)idx_buffer, pcmd->ElemCount);
				RwRenderStateSet(rwRENDERSTATETEXTURERASTER, (void*)0);

				clip_rect = { 0.0f, 0.0f, 0.0f, 0.0f };
				SetScissorRect((void*)&clip_rect);
			}

			idx_buffer += pcmd->ElemCount;
		}
		vtx_offset += cmd_list->VtxBuffer.Size;
	}

}

bool ImGuiWrapper::createFontTexture()
{
	Log::traceLastFunc("ImGuiWrapper::createFontTexture");

	ImGuiIO& io = ImGui::GetIO();
	unsigned char* pxs;
	int width, height, bytes_per_pixel;
	io.Fonts->GetTexDataAsRGBA32(&pxs, &width, &height, &bytes_per_pixel);

	RwImage* font_img = RwImageCreate(width, height, bytes_per_pixel * 8);
	RwImageAllocatePixels(font_img);

	if (font_img == nullptr)
	{
		Log::addParameter("Font image pointer", font_img);
		return false;
	}

	RwUInt8* pixels = font_img->cpPixels;
	for (int y = 0; y < font_img->height; y++)
	{
		memcpy((void*)pixels, pxs + font_img->stride * y, font_img->stride);
		pixels += font_img->stride;
	}

	RwInt32 w, h, d, flags;
	RwImageFindRasterFormat(font_img, rwRASTERTYPETEXTURE, &w, &h, &d, &flags);
	m_fontRaster = RwRasterCreate(w, h, d, flags);
	if (m_fontRaster == nullptr)
	{
		Log::addParameter("font raster pointer", m_fontRaster);
		return false;
	}

	m_fontRaster = RwRasterSetFromImage(m_fontRaster, font_img);
	io.Fonts->TexID = (ImTextureID*)m_fontRaster;

	RwImageDestroy(font_img);

	Log::addParameter("Atlas width", width);
	Log::addParameter("Atlas height", height);
	return true;

}

void ImGuiWrapper::destroyFontTexture()
{
	if (m_fontRaster)
	{
		ImGuiIO& io = ImGui::GetIO();
		RwRasterDestroy(m_fontRaster);
		m_fontRaster = nullptr;
		io.Fonts->TexID = (ImTextureID)0;
	}

}
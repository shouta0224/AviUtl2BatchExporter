#include <windows.h>
#include <shlwapi.h>
#include <string>
#include <vector>
#include "output2.h"

static OUTPUT_PLUGIN_TABLE g_output_table = { 0 };

//===== バッチ登録・設定に関する変数 (後で実装) =====
struct ProjectInfo {
    std::wstring project_path;
    std::wstring output_path;
    std::wstring output_filename;
};
static std::vector<ProjectInfo> g_registered_projects;

//===== 設定に関する変数 (後で実装) =====
static std::wstring g_default_mp4_setting_text = L"Default MP4 Settings (H.264/AAC)";

// func_output のシグネチャ: bool (*func_output)(OUTPUT_INFO* oip);
static bool WINAPI DefineFuncOutput(OUTPUT_INFO* oip);

// func_config のシグネチャ: bool (*func_config)(HWND hwnd, HINSTANCE dll_hinst);
static bool WINAPI DefineFuncConfig(HWND hwnd, HINSTANCE dll_hinst);

// func_get_config_text のシグネチャ: LPCWSTR (*func_get_config_text)();
static LPCWSTR WINAPI DefineFuncGetConfigText();

extern "C" __declspec(dllexport) OUTPUT_PLUGIN_TABLE* __stdcall GetOutputPluginTable(void) {
    g_output_table.flag = OUTPUT_PLUGIN_TABLE::FLAG_VIDEO | OUTPUT_PLUGIN_TABLE::FLAG_AUDIO;
    g_output_table.name = L"Batch MP4 Exporter";
    g_output_table.filefilter = L"MP4 Files (*.mp4)\0*.mp4\0";
    g_output_table.information = L"Batch exports projects to MP4 format.";

    g_output_table.func_output = DefineFuncOutput;
    g_output_table.func_config = DefineFuncConfig;
    g_output_table.func_get_config_text = DefineFuncGetConfigText;

    return &g_output_table;
}

// func_output: 出力時に呼ばれる関数
bool WINAPI DefineFuncOutput(OUTPUT_INFO* oip) {
    if (g_registered_projects.empty()) {
        MessageBoxW(NULL,
            L"No projects registered for batch export.\nPlease register projects first.",
            L"Batch Export Error",
            MB_ICONERROR);
        return false;
    }

    std::wstring message;
    message += L"Batch Export Started (simulated).\n";
    message += L"Processing project: ";
    message += (oip->savefile ? std::wstring(oip->savefile) : L"NoSaveFile");
    message += L"\n";
    message += L"Format: ";
    if (oip->flag & OUTPUT_INFO::FLAG_VIDEO) message += L"Video ";
    if (oip->flag & OUTPUT_INFO::FLAG_AUDIO) message += L"Audio";
    if (message.length() == 24) message += L"None";

    MessageBoxW(NULL,
        message.c_str(),
        L"Batch Export - Simulating Processing",
        MB_OK);

    return true;
}

// func_config: 出力設定ダイアログ表示関数
bool WINAPI DefineFuncConfig(HWND hwnd, HINSTANCE dll_hinst) {
    MessageBoxW(hwnd,
        (L"Configuration dialog would be displayed here.\n" + g_default_mp4_setting_text).c_str(),
        L"Batch Export Configuration",
        MB_OK);
    return true;
}

// func_get_config_text: 出力設定テキスト取得関数
static LPCWSTR WINAPI DefineFuncGetConfigText() {
    return g_default_mp4_setting_text.c_str();
}

BOOL APIENTRY DllMain(HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        break;
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}
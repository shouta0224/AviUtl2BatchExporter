#include <windows.h>
#include <shlwapi.h>
#include <string>
#include <vector>
#include <commdlg.h> // For GetOpenFileName, GetSaveFileName
#include <ShlObj.h>  // For SHBrowseForFolder
#pragma comment(lib, "comdlg32.lib") // For GetOpenFileName etc.
#pragma comment(lib, "shell32.lib")  // For SHBrowseForFolder etc.

#include "output2.h"

// ★★★ OFN_ALLOWMULTIPLE の定義を追加 ★★★
// もし <commdlg.h> で定義されていない場合に備えて追加します。
#ifndef OFN_ALLOWMULTIPLE
#define OFN_ALLOWMULTIPLE 0x00000200L // Standard value for OFN_ALLOWMULTIPLE
#endif

//====================================================================
// Constants and Macros
//====================================================================
#define PLUGIN_NAME L"Batch MP4 Exporter"
#define PLUGIN_FILEFILTER L"MP4 Files (*.mp4)\0*.mp4\0"
#define PLUGIN_INFORMATION L"Batch exports projects to MP4 format."

#define IDD_BATCH_REGISTER_DIALOG 101
#define IDC_PROJECT_LIST 1001
#define IDC_BTN_ADD_PROJECT 1002
#define IDC_EDIT_OUTPUT_FOLDER 1003
#define IDC_BTN_BROWSE_FOLDER 1004
#define IDC_BTN_REMOVE_PROJECT 1005
#define IDC_BTN_CLEAR_PROJECTS 1006
#define IDOK 1
#define IDCANCEL 2

//====================================================================
// Plugin Global Variables
//====================================================================

static OUTPUT_PLUGIN_TABLE g_output_table = { 0 };

//===== バッチ登録・設定に関する変数 =====
struct ProjectInfo {
    std::wstring project_path;
    std::wstring output_path;
    std::wstring output_filename;
};
static std::vector<ProjectInfo> g_registered_projects;

static std::wstring g_default_mp4_setting_text = L"Default MP4 Settings";

//===== Global Handle for the Batch Register Dialog =====
static HWND g_hBatchDialog = NULL;

//====================================================================
// Forward Declarations of Plugin Functions
//====================================================================

static bool WINAPI DefineFuncOutput(OUTPUT_INFO* oip);
static bool WINAPI DefineFuncConfig(HWND hwnd, HINSTANCE dll_hinst);
static LPCWSTR WINAPI DefineFuncGetConfigText();

//====================================================================
// Dialog Functions
//====================================================================

static INT_PTR CALLBACK BatchRegisterDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_INITDIALOG: {
        g_hBatchDialog = hDlg;

        wchar_t default_output_path[MAX_PATH] = { 0 };
        wcscpy_s(default_output_path, MAX_PATH, L"C:\\Users\\Public\\Videos"); // Placeholder path
        SetDlgItemText(hDlg, IDC_EDIT_OUTPUT_FOLDER, default_output_path);

        SetWindowText(hDlg, L"Batch Project Registration");

        return TRUE;
    }

    case WM_COMMAND: {
        int wmId = LOWORD(wParam);
        int wmEvent = HIWORD(wParam);

        switch (wmId) {
        case IDC_BTN_ADD_PROJECT: {
            OPENFILENAMEW ofn = { 0 };
            wchar_t szFileMulti[MAX_PATH * 10] = { 0 }; // Buffer for multiple files

            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hDlg;
            ofn.lpstrFilter = L"AviUtl2 Project Files (*.aup2)\0*.aup2\0All Files (*.*)\0*.*\0";
            ofn.lpstrFile = szFileMulti;
            ofn.nMaxFile = MAX_PATH * 10;
            // OFN_ALLOWMULTIPLE フラグを使用
            ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_ALLOWMULTIPLE;
            ofn.lpstrTitle = L"Select AviUtl2 Project Files";

            if (GetOpenFileNameW(&ofn)) {
                std::wstring file_list = szFileMulti;
                size_t start = 0;
                size_t end = 0;
                while ((end = file_list.find(L'\0', start)) != std::wstring::npos) {
                    std::wstring current_file = file_list.substr(start, end - start);
                    if (!current_file.empty()) {
                        MessageBoxW(hDlg, std::wstring(L"Selected: " + current_file).c_str(), L"File Selection", MB_OK);
                        // TODO: Add the selected file to g_registered_projects and update list
                    }
                    start = end + 1;
                }
                if (start < file_list.length()) {
                    std::wstring current_file = file_list.substr(start);
                    if (!current_file.empty()) {
                        MessageBoxW(hDlg, std::wstring(L"Selected: " + current_file).c_str(), L"File Selection", MB_OK);
                        // TODO: Add the selected file to g_registered_projects and update list
                    }
                }
            }
            break;
        }
        case IDC_BTN_BROWSE_FOLDER: {
            BROWSEINFO bi = { 0 };
            wchar_t folder_path_buffer[MAX_PATH] = { 0 };

            bi.hwndOwner = hDlg;
            bi.pszDisplayName = folder_path_buffer;
            bi.lpszTitle = L"Select Output Folder";
            bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

            LPITEMIDLIST pidl = SHBrowseForFolder(&bi);
            if (pidl != NULL) {
                if (SHGetPathFromIDListW(pidl, folder_path_buffer)) {
                    SetDlgItemTextW(hDlg, IDC_EDIT_OUTPUT_FOLDER, folder_path_buffer);
                }
                IMalloc* pMalloc = NULL;
                if (SUCCEEDED(SHGetMalloc(&pMalloc))) {
                    pMalloc->Free(pidl);
                    pMalloc->Release();
                }
            }
            break;
        }
        case IDC_BTN_REMOVE_PROJECT: {
            MessageBoxW(hDlg, L"Remove selected project (not implemented).", L"Remove Project", MB_OK);
            break;
        }
        case IDC_BTN_CLEAR_PROJECTS: {
            MessageBoxW(hDlg, L"Clear all projects (not implemented).", L"Clear Projects", MB_OK);
            break;
        }
        case IDOK: {
            wchar_t output_folder[MAX_PATH];
            GetDlgItemTextW(hDlg, IDC_EDIT_OUTPUT_FOLDER, output_folder, MAX_PATH);
            // TODO: Process g_registered_projects here, using output_folder

            EndDialog(hDlg, TRUE);
            return TRUE;
        }
        case IDCANCEL: {
            EndDialog(hDlg, FALSE);
            return TRUE;
        }
        default:
            return FALSE;
        }
        break;
    }

    case WM_CLOSE: {
        EndDialog(hDlg, FALSE);
        return TRUE;
    }
    }
    return FALSE;
}

static void ShowBatchRegisterDialog() {
    INT_PTR result = DialogBoxParamW(
        GetModuleHandle(NULL),
        MAKEINTRESOURCE(IDD_BATCH_REGISTER_DIALOG),
        NULL,
        BatchRegisterDialogProc,
        NULL
    );

    if (result == IDOK) {
        MessageBoxW(NULL, L"Batch registration processed.", L"Success", MB_OK);
    }
    else {
        MessageBoxW(NULL, L"Batch registration cancelled.", L"Cancelled", MB_OK);
    }
}

//====================================================================
// AviUtl2 Plugin Exported Function
//====================================================================

extern "C" __declspec(dllexport) OUTPUT_PLUGIN_TABLE* __stdcall GetOutputPluginTable(void) {
    g_output_table.flag = OUTPUT_PLUGIN_TABLE::FLAG_VIDEO | OUTPUT_PLUGIN_TABLE::FLAG_AUDIO;
    g_output_table.name = L"Batch MP4 Exporter";
    g_output_table.filefilter = L"MP4 Files (*.mp4)\0*.mp4\0";
    g_output_table.information = L"Batch exports projects to MP4 format.";

    g_output_table.func_output = DefineFuncOutput;
    g_output_table.func_config = [](HWND hwnd, HINSTANCE dll_hinst) -> bool {
        ShowBatchRegisterDialog();
        return true;
        };
    g_output_table.func_get_config_text = DefineFuncGetConfigText;

    return &g_output_table;
}

//====================================================================
// Output Plugin Functions Implementation
//====================================================================

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

static LPCWSTR WINAPI DefineFuncGetConfigText() {
    return g_default_mp4_setting_text.c_str();
}

//====================================================================
// DLL Main Function
//====================================================================
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
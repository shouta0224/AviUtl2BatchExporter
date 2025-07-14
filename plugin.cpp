#include <windows.h>
#include <shlwapi.h>
#include <string>
#include <vector>
#include <commdlg.h> // For GetOpenFileName, GetSaveFileName
#include <ShlObj.h>  // For SHBrowseForFolder

#include "output2.h"

//====================================================================
// Constants and Macros
//====================================================================
// Plugin Meta Info (Replace with your actual info)
#define PLUGIN_NAME L"Batch MP4 Exporter"
#define PLUGIN_FILEFILTER L"MP4 Files (*.mp4)\0*.mp4\0"
#define PLUGIN_INFORMATION L"Batch exports projects to MP4 format."

// Dialog resource ID (for demonstration, you'd use a resource file)
#define IDD_BATCH_REGISTER_DIALOG 101
// Control IDs (for demonstration, these would be in a resource file)
#define IDC_PROJECT_LIST 1001
#define IDC_BTN_ADD_PROJECT 1002
#define IDC_EDIT_OUTPUT_FOLDER 1003
#define IDC_BTN_BROWSE_FOLDER 1004
#define IDC_BTN_REMOVE_PROJECT 1005
#define IDC_BTN_CLEAR_PROJECTS 1006
#define IDOK 1 // Standard OK button ID
#define IDCANCEL 2 // Standard Cancel button ID


//====================================================================
// Plugin Global Variables
//====================================================================

static OUTPUT_PLUGIN_TABLE g_output_table = { 0 };

//===== バッチ登録・設定に関する変数 =====
struct ProjectInfo {
    std::wstring project_path;
    std::wstring output_path;
    std::wstring output_filename;
    // TODO: Add output settings here later
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

// --- Callback for the Batch Register Dialog ---
INT_PTR CALLBACK BatchRegisterDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_INITDIALOG: {
        g_hBatchDialog = hDlg; // Store the dialog handle

        // --- Initialize Dialog Controls ---
        // Set the default output folder (e.g., from AviUtl's output directory)
        wchar_t default_output_path[MAX_PATH] = { 0 };
        // You might want to get AviUtl's output path here.
        // For now, let's use a placeholder.
        wcscpy_s(default_output_path, MAX_PATH, L"C:\\Users\\Public\\Videos");
        SetDlgItemText(hDlg, IDC_EDIT_OUTPUT_FOLDER, default_output_path);

        // Initialize the project list (currently empty)
        // HWND hList = GetDlgItem(hDlg, IDC_PROJECT_LIST);
        // You would typically add columns and items to a ListView here.

        // Set the dialog title
        SetWindowText(hDlg, L"Batch Project Registration");

        return TRUE; // Indicate that we've processed the message
    }

    case WM_COMMAND: {
        // Get the control ID that generated the command
        int wmId = LOWORD(wParam);
        int wmEvent = HIWORD(wParam);

        // Parse the menu selections:
        switch (wmId) {
        case IDC_BTN_ADD_PROJECT: {
            // --- Open File Dialog to select .aup2 files ---
            OPENFILENAMEW ofn = { 0 };
            wchar_t szFile[MAX_PATH] = { 0 };
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hDlg;
            ofn.lpstrFilter = L"AviUtl2 Project Files (*.aup2)\0*.aup2\0All Files (*.*)\0*.*\0";
            ofn.lpstrFile = szFile;
            ofn.nMaxFile = MAX_PATH;
            ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_ALLOWMULTIPLE; // Allow multiple selections
            ofn.lpstrTitle = L"Select AviUtl2 Project Files";

            if (GetOpenFileNameW(&ofn)) {
                // Process the selected files (szFile will contain the first file, use ofn.lpstrFile for the rest if multiple selected)
                // You'll need to parse the multi-selected files from szFile.
                // For now, let's just show the first selected file.
                MessageBoxW(hDlg, std::wstring(L"Selected files (first): " + std::wstring(szFile)).c_str(), L"File Selection", MB_OK);

                // TODO: Add the selected files to the g_registered_projects vector and update the list control.
                // For each selected file:
                // ProjectInfo pi;
                // pi.project_path = szFile; // Need to handle multiple files properly
                // Get output folder and filename from UI controls
                // wchar_t output_folder[MAX_PATH];
                // GetDlgItemTextW(hDlg, IDC_EDIT_OUTPUT_FOLDER, output_folder, MAX_PATH);
                // pi.output_path = output_folder;
                // // Set default output filename based on project_path
                // pi.output_filename = PathFindFileNameW(szFile); // Example
                // g_registered_projects.push_back(pi);
                // UpdateListView(hDlg, IDC_PROJECT_LIST, pi); // Function to add to list
            }
            break;
        }
        case IDC_BTN_BROWSE_FOLDER: {
            // --- Open Folder Browser Dialog ---
            BROWSEINFO bi = { 0 };
            bi.hwndOwner = hDlg;
            bi.pszDisplayName = default_output_path; // Use the existing buffer
            bi.lpszTitle = L"Select Output Folder";
            bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

            LPITEMIDLIST pidl = SHBrowseForFolder(&bi);
            if (pidl != NULL) {
                wchar_t folder_path[MAX_PATH] = { 0 };
                if (SHGetPathFromIDListW(pidl, folder_path)) {
                    SetDlgItemTextW(hDlg, IDC_EDIT_OUTPUT_FOLDER, folder_path);
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
            // TODO: Implement logic to remove selected project from list and vector
            MessageBoxW(hDlg, L"Remove selected project (not implemented).", L"Remove Project", MB_OK);
            break;
        }
        case IDC_BTN_CLEAR_PROJECTS: {
            // TODO: Implement logic to clear all projects from list and vector
            MessageBoxW(hDlg, L"Clear all projects (not implemented).", L"Clear Projects", MB_OK);
            break;
        }
        case IDOK: { // OK button clicked
            // --- Process Registration and Close Dialog ---
            // TODO: Get output folder and filename from UI
            // TODO: Finalize registration of projects (e.g., apply default output settings if not specified)
            // TODO: Store registered projects in g_registered_projects

            EndDialog(hDlg, TRUE); // Close the dialog
            return TRUE;
        }
        case IDCANCEL: { // Cancel button clicked
            // --- Cancel Registration and Close Dialog ---
            EndDialog(hDlg, FALSE); // Close the dialog without saving registration
            return TRUE;
        }
        default:
            return FALSE;
        }
        break;
    }

    case WM_CLOSE: {
        EndDialog(hDlg, FALSE); // Close the dialog on WM_CLOSE
        return TRUE;
    }
    }
    return FALSE;
}

// --- Function to display the Batch Register Dialog ---
static void ShowBatchRegisterDialog() {
    // Create the dialog box.
    // The dialog template needs to be defined in a resource file (.rc).
    // For now, we'll simulate it with DialogBoxParam, assuming the template exists.
    // If you are not using a resource file, you would need to create the dialog
    // programmatically using CreateDialogParam.

    // If using a resource file (recommended):
    INT_PTR result = DialogBoxParam(
        GetModuleHandle(NULL),     // hInstance
        MAKEINTRESOURCE(IDD_BATCH_REGISTER_DIALOG), // Dialog template resource ID
        NULL,                      // Parent window handle (NULL for top-level)
        BatchRegisterDialogProc,   // Dialog procedure callback
        NULL                       // lParam parameter to WM_INITDIALOG
    );

    if (result == IDOK) {
        // User clicked OK - registration might have been processed inside BatchRegisterDialogProc
        // Or you might need to retrieve the data here from g_registered_projects
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
    // --- func_config にバッチ登録ダイアログ表示処理を紐付けます ---
    g_output_table.func_config = [](HWND hwnd, HINSTANCE dll_hinst) -> bool {
        ShowBatchRegisterDialog(); // バッチ登録ダイアログを表示
        return true; // 常に true を返す (設定の変更があったとみなす)
        };
    g_output_table.func_get_config_text = DefineFuncGetConfigText;

    return &g_output_table;
}

//====================================================================
// Output Plugin Functions Implementation
//====================================================================

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

// func_config: 出力設定ダイアログ表示関数 (GetOutputPluginTable で ShowBatchRegisterDialog を呼ぶように変更)
// もし、設定変更のダイアログとバッチ登録のダイアログを分けたい場合は、
// output2.h の定義を見直すか、別のエントリポイントを検討する必要があります。
// 今は func_config からバッチ登録ダイアログを開く形にしています。
/*
bool WINAPI DefineFuncConfig(HWND hwnd, HINSTANCE dll_hinst) {
    MessageBoxW(hwnd,
                (L"Configuration dialog would be displayed here.\n" + g_default_mp4_setting_text).c_str(),
                L"Batch Export Configuration",
                MB_OK);
    return true;
}
*/

// func_get_config_text: 出力設定テキスト取得関数
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
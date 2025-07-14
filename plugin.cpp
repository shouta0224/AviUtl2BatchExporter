#include <windows.h>
#include <shlwapi.h>
#include <string>
#include <vector>
#include <commdlg.h>
#include <ShlObj.h>
#include <commctrl.h>
// #include <filesystem> // C++17が必要なため、一旦コメントアウトします。Windows APIで代替します。
#include <algorithm>

#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comctl32.lib")

#include "output2.h"

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

// --- OFN_ALLOWMULTIPLE の定義 ---
#ifndef OFN_ALLOWMULTIPLE
#define OFN_ALLOWMULTIPLE 0x00000200L
#endif

// --- LVIF_ALL の定義 ---
#ifndef LVIF_ALL
#define LVIF_ALL 0xFFFFFFFF
#endif

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
static HWND g_hListView = NULL;

//====================================================================
// Forward Declarations of Plugin Functions
//====================================================================

static bool WINAPI DefineFuncOutput(OUTPUT_INFO* oip);
static bool WINAPI DefineFuncConfig(HWND hwnd, HINSTANCE dll_hinst);
static LPCWSTR WINAPI DefineFuncGetConfigText();

//====================================================================
// Dialog Functions
//====================================================================

// --- Helper function to add an item to the ListView ---
static void AddProjectToListView(const ProjectInfo& pi) {
    if (g_hListView == NULL) return;

    // LVITEMW structure for inserting an item
    LVITEMW lvi = { 0 };
    lvi.mask = LVIF_TEXT;
    lvi.iItem = LVIF_ALL;
    lvi.iSubItem = 0;

    // --- Create temporary buffers for each string to be inserted ---
    // This is necessary because pszText needs a writable buffer, and we are passing const data.
    // The buffers are local to this function, so they are valid for the ListView_InsertItem/SetItem calls.

    // Project Path column
    std::vector<wchar_t> projectPathBuf(pi.project_path.begin(), pi.project_path.end());
    projectPathBuf.push_back(L'\0'); // Null-terminate
    lvi.pszText = projectPathBuf.data();
    int iItem = ListView_InsertItem(&lvi);

    if (iItem == -1) return; // Failed to insert item

    // Set text for subitems (columns)
    lvi.iItem = iItem;

    // Output Folder column
    lvi.iSubItem = 1;
    std::vector<wchar_t> outputPathBuf(pi.output_path.begin(), pi.output_path.end());
    outputPathBuf.push_back(L'\0');
    lvi.pszText = outputPathBuf.data();
    ListView_SetItem(&lvi);

    // Output Filename column
    lvi.iSubItem = 2;
    std::vector<wchar_t> outputFilenameBuf(pi.output_filename.begin(), pi.output_filename.end());
    outputFilenameBuf.push_back(L'\0');
    lvi.pszText = outputFilenameBuf.data();
    ListView_SetItem(&lvi);
}

// --- Dialog Procedure ---
static INT_PTR CALLBACK BatchRegisterDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_INITDIALOG: {
        g_hBatchDialog = hDlg;

        // --- Initialize ListView ---
        g_hListView = GetDlgItem(hDlg, IDC_PROJECT_LIST);
        if (g_hListView) {
            INITCOMMONCONTROLSEX icex;
            icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
            icex.dwICC = ICC_LISTVIEW_CLASSES;
            InitCommonControlsEx(&icex);

            ListView_SetExtendedListViewStyle(g_hListView, LVS_EX_GRIDLINES | LVS_EX_FULLROWSELECT);

            LVCOLUMNW lvc = { 0 };
            lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;

            lvc.pszText = L"Project Path";
            lvc.cx = 200;
            ListView_InsertColumn(g_hListView, 0, &lvc);

            lvc.pszText = L"Output Folder";
            lvc.cx = 150;
            ListView_InsertColumn(g_hListView, 1, &lvc);

            lvc.pszText = L"Output Filename";
            lvc.cx = 150;
            ListView_InsertColumn(g_hListView, 2, &lvc);
        }

        // Set the default output folder
        wchar_t default_output_path[MAX_PATH] = { 0 };
        wcscpy_s(default_output_path, MAX_PATH, L"C:\\Users\\Public\\Videos"); // Placeholder
        SetDlgItemTextW(hDlg, IDC_EDIT_OUTPUT_FOLDER, default_output_path);

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
            ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_ALLOWMULTIPLE;
            ofn.lpstrTitle = L"Select AviUtl2 Project Files";

            if (GetOpenFileNameW(&ofn)) {
                std::wstring file_list = szFileMulti;
                size_t start = 0;
                size_t end = 0;
                wchar_t current_output_folder[MAX_PATH] = { 0 };
                GetDlgItemTextW(hDlg, IDC_EDIT_OUTPUT_FOLDER, current_output_folder, MAX_PATH);

                while ((end = file_list.find(L'\0', start)) != std::wstring::npos) {
                    std::wstring current_file = file_list.substr(start, end - start);
                    if (!current_file.empty()) {
                        ProjectInfo pi;
                        pi.project_path = current_file;
                        pi.output_path = current_output_folder;

                        wchar_t filename_buffer[MAX_PATH] = { 0 };
                        // Get the file part from the path
                        const wchar_t* file_part = PathFindFileNameW(current_file.c_str());
                        if (file_part) {
                            // Copy the filename to a writable buffer
                            wcscpy_s(filename_buffer, MAX_PATH, file_part);

                            // Remove .aup2 extension and add .mp4
                            PathRemoveExtensionW(filename_buffer);
                            wcscat_s(filename_buffer, MAX_PATH, L".mp4");
                            pi.output_filename = filename_buffer;
                        }
                        else {
                            pi.output_filename = L"output.mp4"; // Fallback
                        }

                        g_registered_projects.push_back(pi);
                        AddProjectToListView(pi);
                    }
                    start = end + 1;
                }
                // Add the last file if it's not empty
                if (start < file_list.length()) {
                    std::wstring current_file = file_list.substr(start);
                    if (!current_file.empty()) {
                        ProjectInfo pi;
                        pi.project_path = current_file;
                        pi.output_path = current_output_folder;

                        wchar_t filename_buffer[MAX_PATH] = { 0 };
                        const wchar_t* file_part = PathFindFileNameW(current_file.c_str());
                        if (file_part) {
                            wcscpy_s(filename_buffer, MAX_PATH, file_part);
                            PathRemoveExtensionW(filename_buffer);
                            wcscat_s(filename_buffer, MAX_PATH, L".mp4");
                            pi.output_filename = filename_buffer;
                        }
                        else {
                            pi.output_filename = L"output.mp4";
                        }

                        g_registered_projects.push_back(pi);
                        AddProjectToListView(pi);
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
            // Get selected item index from ListView
            int selected_index = ListView_GetNextItem(g_hListView, -1, LVNI_SELECTED);
            if (selected_index != -1) {
                // Remove from vector
                if (selected_index < g_registered_projects.size()) {
                    g_registered_projects.erase(g_registered_projects.begin() + selected_index);
                }
                // Remove from ListView
                ListView_DeleteItem(g_hListView, selected_index);
            }
            break;
        }
        case IDC_BTN_CLEAR_PROJECTS: {
            ListView_DeleteAllItems(g_hListView);
            g_registered_projects.clear();
            break;
        }
        case IDOK: {
            wchar_t output_folder[MAX_PATH];
            GetDlgItemTextW(hDlg, IDC_EDIT_OUTPUT_FOLDER, output_folder, MAX_PATH);
            for (auto& pi : g_registered_projects) {
                pi.output_path = output_folder;
            }
            // TODO: Implement saving the registered projects if needed for persistence.

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
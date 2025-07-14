#include <windows.h>
#include <shlwapi.h>
#include <string>
#include <vector>
#include <commdlg.h>
#include <ShlObj.h>
#include <commctrl.h>
#include <algorithm>

#pragma comment(lib, "shlwapi.lib")
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

//====================================================================
// Plugin Global Variables
//====================================================================

static OUTPUT_PLUGIN_TABLE g_output_table = { 0 };

//===== バッチ登録・設定に関する変数 =====
struct ProjectInfo {
    std::wstring project_path;
    std::wstring output_path;
    std::wstring output_filename;
    // ListView に表示するためのバッファを保持
    std::vector<wchar_t> projectPathBuf;
    std::vector<wchar_t> outputPathBuf;
    std::vector<wchar_t> outputFilenameBuf;
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

    // --- Column 0: Project Path ---
    LVITEMW lvi0 = { 0 };
    lvi0.mask = LVIF_TEXT;
    lvi0.iItem = ListView_GetItemCount(g_hListView); // Insert at the end
    lvi0.iSubItem = 0;
    lvi0.pszText = const_cast<LPWSTR>(pi.projectPathBuf.data()); // Pass the buffer from ProjectInfo
    int iItem = ListView_InsertItem(g_hListView, &lvi0);

    if (iItem == -1) return;

    // --- Column 1: Output Folder ---
    LVITEMW lvi1 = { 0 };
    lvi1.mask = LVIF_TEXT;
    lvi1.iItem = iItem;
    lvi1.iSubItem = 1;
    lvi1.pszText = const_cast<LPWSTR>(pi.outputPathBuf.data());
    ListView_SetItem(g_hListView, &lvi1);

    // --- Column 2: Output Filename ---
    LVITEMW lvi2 = { 0 };
    lvi2.mask = LVIF_TEXT;
    lvi2.iItem = iItem;
    lvi2.iSubItem = 2;
    lvi2.pszText = const_cast<LPWSTR>(pi.outputFilenameBuf.data());
    ListView_SetItem(g_hListView, &lvi2);
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

            static const wchar_t col1[] = L"Project Path";
            static const wchar_t col2[] = L"Output Folder";
            static const wchar_t col3[] = L"Output Filename";

            lvc.pszText = (LPWSTR)col1;
            lvc.cx = 200;
            ListView_InsertColumn(g_hListView, 0, &lvc);

            lvc.pszText = (LPWSTR)col2;
            lvc.cx = 150;
            ListView_InsertColumn(g_hListView, 1, &lvc);

            lvc.pszText = (LPWSTR)col3;
            lvc.cx = 150;
            ListView_InsertColumn(g_hListView, 2, &lvc);
        }

        // Set the default output folder
        wchar_t default_output_path[MAX_PATH] = { 0 };
        wcscpy_s(default_output_path, MAX_PATH, L"C:\\Users\\Public\\Videos"); // Placeholder
        SetDlgItemTextW(hDlg, IDC_EDIT_OUTPUT_FOLDER, default_output_path);

        SetWindowText(hDlg, L"Batch Project Registration");

        // Load previously registered projects if persistence is implemented.
        // For now, g_registered_projects starts empty.

        return TRUE;
    }

    case WM_COMMAND: {
        int wmId = LOWORD(wParam);
        int wmEvent = HIWORD(wParam);

        switch (wmId) {
        case IDC_BTN_ADD_PROJECT: {
            OPENFILENAMEW ofn = { 0 };
            wchar_t szFileMulti[MAX_PATH * 10] = { 0 };

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

                        // Prepare buffers within ProjectInfo for ListView
                        pi.projectPathBuf.assign(pi.project_path.begin(), pi.project_path.end());
                        pi.projectPathBuf.push_back(L'\0');
                        pi.outputPathBuf.assign(pi.output_path.begin(), pi.output_path.end());
                        pi.outputPathBuf.push_back(L'\0');
                        pi.outputFilenameBuf.assign(pi.output_filename.begin(), pi.output_filename.end());
                        pi.outputFilenameBuf.push_back(L'\0');

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

                        // Prepare buffers within ProjectInfo for ListView
                        pi.projectPathBuf.assign(pi.project_path.begin(), pi.project_path.end());
                        pi.projectPathBuf.push_back(L'\0');
                        pi.outputPathBuf.assign(pi.output_path.begin(), pi.output_path.end());
                        pi.outputPathBuf.push_back(L'\0');
                        pi.outputFilenameBuf.assign(pi.output_filename.begin(), pi.output_filename.end());
                        pi.outputFilenameBuf.push_back(L'\0');

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
            int selected_index = ListView_GetNextItem(g_hListView, -1, LVNI_SELECTED);
            if (selected_index != -1) {
                if (selected_index < g_registered_projects.size()) {
                    g_registered_projects.erase(g_registered_projects.begin() + selected_index);
                }
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

    // --- バッチ出力処理のメインループ ---
    // ここで g_registered_projects に格納されているプロジェクトを順番に処理します。
    // 各プロジェクトに対して、エンコード処理を実行します。

    int total_projects = g_registered_projects.size();
    int completed_projects = 0;

    for (int i = 0; i < total_projects; ++i) {
        const ProjectInfo& pi = g_registered_projects[i];

        // --- エンコード処理開始の通知 ---
        std::wstring progress_msg = L"Processing: ";
        progress_msg += pi.project_path;
        // TODO: メッセージボックスではなく、ダイアログ内のステータスバーなどに表示するのが望ましい
        // MessageBoxW(g_hBatchDialog, progress_msg.c_str(), L"Batch Export - Starting", MB_OK);

        // --- AviUtl2 の出力機能を使ったエンコード処理 ---
        // ここで oip->func_get_video(), oip->func_get_audio(), oip->func_is_abort() などを
        // 使用して、実際のエンコード処理を行います。
        // これは AviUtl2 の具体的な出力プラグインAPIに依存します。
        // 現状ではダミーのエンコード処理を行います。

        // ダミーエンコード:AviUtl2の標準出力機能を呼び出す想定
        // oip->savefile は現在の出力ファイル名を示しているはずです。
        // しかし、ここで設定されているのは最初の登録プロジェクトの情報のみかもしれません。
        // バッチ処理では、各プロジェクトの出力ファイル名を個別に設定する必要があります。
        // output2.h には、個別のプロジェクトに対してsavefileを設定するAPIがないため、
        //AviUtl2本体にバッチ処理用の特別なAPIがあるか、
        // または、エンコーダープラグインにプロジェクト情報を渡す別の方法が必要になるかもしれません。

        // 今回は、oip->savefile をダミーとして使い、
        // oip->func_get_video や oip->func_get_audio を呼び出す想定で進めます。
        // 실제エンコード処理は、AviUtl2 本体や外部エンコーダーライブラリと連携する必要があります。

        // 仮のエンコード処理メッセージ
        std::wstring encode_sim_msg = L"Simulating encode for: ";
        encode_sim_msg += pi.project_path;
        // メッセージボックスは多すぎるので、デバッグ出力やListViewに表示するのが良いでしょう。
        // MessageBoxW(g_hBatchDialog, encode_sim_msg.c_str(), L"Encoding", MB_OK);

        // エンコード処理の進捗表示（例）
        // for (int frame = 0; frame < oip->n; ++frame) {
        //     if (oip->func_is_abort()) {
        //         MessageBoxW(g_hBatchDialog, L"Encoding aborted by user.", L"Aborted", MB_OK);
        //         return false; // Aborted
        //     }
        //     if (frame % 10 == 0) { // Progress update every 10 frames
        //         oip->func_rest_time_disp(frame, oip->n);
        //     }
        //     // void* video_frame = oip->func_get_video(frame, BI_RGB); // Get video frame
        //     // void* audio_data = oip->func_get_audio(frame * samples_per_frame, samples_per_frame, ...); // Get audio data
        //     // Send frame/audio data to encoder
        // }

        // ダミーのエンコード処理成功として進めます
        completed_projects++;
        oip->func_rest_time_disp(i + 1, total_projects); // 進捗表示を更新
    }

    // --- 全てのエンコードが完了したら ---
    // TODO: シャットダウン確認ダイアログを表示し、選択に応じて実行する。
    // ここは、バッチ出力全体が完了したタイミングで行うべき処理です。
    // 今はダミーメッセージを表示します。
    MessageBoxW(g_hBatchDialog,
        (L"Batch encoding finished.\n"
            L"Completed: " + std::to_wstring(completed_projects) + L"/" + std::to_wstring(total_projects) + L"\n"
            L"Would you like to shut down your PC?").c_str(),
        L"Batch Encoding Complete",
        MB_OK); // TODO: 実際の選択ダイアログに変更

    return true; // 全ての処理が正常に完了したと仮定
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
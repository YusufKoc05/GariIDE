// main.cpp — GariIDE UI (Dark Carbon & Crimson Pro Edition) — menu + toolbar + zoom + wrap + find + file open/save + splitter + accelerators + RichEdit + gutter line numbers + owner-draw tabs + status bar
// PATCH: Ctrl+Shift+P Komut Menüsü + RUN sonrası ilk hata için squiggle (WM_APP_SETERRMARK)

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <richedit.h>
#include <dwmapi.h>

#ifdef ERROR
#undef ERROR
#endif

#include <string>
#include <vector>
#include <cwctype>
#include <sstream>
#include <stdexcept>
#include <memory>
#include <unordered_map>
#include <iomanip>
#include <cstring>
#include <atomic>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <regex>

#include "core.h"

#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "Dwmapi.lib")

#ifndef EM_REDO
#define EM_REDO 0x0454
#endif
#ifndef EM_SETTARGETDEVICE
#define EM_SETTARGETDEVICE  (WM_USER + 72)
#endif
#ifndef EM_SETEVENTMASK
#define EM_SETEVENTMASK     (WM_USER + 69)
#endif
#ifndef ENM_CHANGE
#define ENM_CHANGE          0x00000001
#endif
#ifndef ENM_SELCHANGE
#define ENM_SELCHANGE        0x00080000
#endif
#ifndef EN_SELCHANGE
#define EN_SELCHANGE        0x0702
#endif
#ifndef MSFTEDIT_CLASS
#define MSFTEDIT_CLASS L"RICHEDIT50W"
#endif

// ===================== Design Tokens (DARK CARBON & CRIMSON PRO) =====================
static COLORREF TOK_BG = RGB(150, 40, 50);            // Ana İç Paneller: Tok ve asil bir koyu kiremit / Crimson kırmızısı
static COLORREF TOK_SURFACE = RGB(45, 48, 50);        // Üst Paneller / Toolbar: Derin mat antrasit / karbon grisi
static COLORREF TOK_SURFACE2 = RGB(65, 70, 72);       // Hover/Seçim Durumu: Belirgin açık karbon grisi
static COLORREF TOK_ACCENT = RGB(220, 50, 65);        // Odak/Vurgu Çizgileri: Parlak kor kırmızısı
static COLORREF TOK_GOOD = RGB(140, 220, 140);        // Çalıştır Sembolü: Gri-Kırmızı kontrastında parlayan soft yeşil
static COLORREF TOK_TEXT = RGB(235, 240, 242);        // Gri alanlardaki metinler: Temiz açık gümüş gri / beyaz
static COLORREF TOK_MUTED = RGB(130, 135, 138);       // İkincil / Sönük Metinler (Mat Karbon Gri)
static COLORREF TOK_BORDER = RGB(35, 38, 40);         // Panel Sınırları: Keskin ve elit koyu antrasit çizgiler
static COLORREF TOK_GUTTERBG = RGB(130, 32, 42);       // Satır Sayısı Alanı: Editörden bir ton daha koyu, asil mat kırmızı
static COLORREF TOK_GUTTERHI = RGB(175, 50, 62);       // Aktif Satır Vurgulama Alanı

// İç Editör Alanı Metin ve Arka Plan Senkronizasyonu (Profesyonel Yüksek Okunurluk)
static COLORREF TOK_CODE_TEXT = RGB(255, 252, 250);   // Kod Metni: Koyu Crimson üzerinde parlayan temiz krem beyazı
static COLORREF TOK_EDITOR_BG = RGB(150, 40, 50);      // Editör İç Arka Planı: Tok mat Crimson kırmızısı

// Find window: Industrial Slate Styles Popup
static COLORREF TOK_FIND_BG = RGB(150, 40, 50);
static COLORREF TOK_FIND_SURF = RGB(45, 48, 50);
static COLORREF TOK_FIND_EDIT = RGB(35, 38, 40);
static COLORREF TOK_FIND_BORDER = RGB(220, 50, 65);
static COLORREF TOK_FIND_TITLETX = RGB(235, 240, 242);
static COLORREF TOK_FIND_BT = RGB(65, 70, 72);
static COLORREF TOK_FIND_BT_H = RGB(85, 90, 92);
static COLORREF TOK_FIND_BT_D = RGB(220, 50, 65);

static HBRUSH gBrBg = nullptr, gBrSurface = nullptr, gBrSurface2 = nullptr;
static HBRUSH gBrTab = nullptr;
static HBRUSH gBrEdit = nullptr;
static HBRUSH gBrEditorWhite = nullptr;
static HBRUSH gBrFindBg = nullptr;
static HBRUSH gBrFindSurf = nullptr;
static HBRUSH gBrFindEdit = nullptr;

static HFONT gFontUI = nullptr;
static HFONT gFontCode = nullptr;

static HWND g_hMain = nullptr;

// ===================== IDs =====================
enum {
    IDC_EDIT_CODE = 101,
    IDC_GUTTER = 102,

    // Right inspector tabs + views
    IDC_TAB_RIGHT = 120,
    IDC_VIEW_TOK = 131,
    IDC_VIEW_AST = 132,
    IDC_VIEW_IR = 133,

    // Bottom panel (Console only)
    IDC_TAB_BOTTOM = 140,
    IDC_CONSOLE_OUT = 141,
    IDC_CONSOLE_IN = 142,

    IDC_STATUS = 150,

    // Command bar buttons
    IDC_BTN_RUN = 201,
    IDC_BTN_OPEN = 210,
    IDC_BTN_SAVE = 211,
    IDC_BTN_CLR = 205,
    IDC_BTN_MORE = 299,
};

enum {
    IDM_FILE_NEW = 40001,
    IDM_FILE_OPEN,
    IDM_FILE_SAVE,
    IDM_FILE_SAVEAS,
    IDM_FILE_EXIT,

    IDM_EDIT_UNDO,
    IDM_EDIT_REDO,
    IDM_EDIT_CUT,
    IDM_EDIT_COPY,
    IDM_EDIT_PASTE,
    IDM_EDIT_FIND,

    IDM_VIEW_ZOOMIN,
    IDM_VIEW_ZOOMOUT,
    IDM_VIEW_ZOOMRESET,
    IDM_VIEW_WRAP,

    IDM_VIEW_SHOW_TOKEN,
    IDM_VIEW_SHOW_AST,
    IDM_VIEW_SHOW_IR,

    IDM_TOOLS_COMMANDPALETTE,
    IDM_TOOLS_SETTINGS,

    IDM_HELP_ABOUT,
    IDM_HELP_LANGREF,

    IDM_RUN_F5,
    IDM_RUN_INTERP,

    IDM_EXAMPLE_1,
    IDM_EXAMPLE_2,
    IDM_EXAMPLE_3,
    IDM_EXAMPLE_4,
};

#define WM_APP_APPENDOUT    (WM_APP + 10)
#define WM_APP_RUNFINISH    (WM_APP + 11)
#define WM_APP_UPDSTATUS    (WM_APP + 21)

// (A) Squiggle: RUN sonrası ilk hatayı işaretle
#define WM_APP_SETERRMARK   (WM_APP + 23)

static int gTopH = 52;
static int gStatusH = 22;

static int gRightW = 460;
static const int gMinRightW = 320;
static const int gMaxRightW = 780;

static int gBottomH = 230;
static const int gMinBottomH = 140;
static const int gMaxBottomH = 360;

static const int gSplitterW = 4;
static const int gGutterW = 54;

static HWND g_hCode = nullptr;
static HWND g_hGutter = nullptr;

static HWND g_hTabRight = nullptr;
static HWND g_hTok = nullptr;
static HWND g_hAst = nullptr;
static HWND g_hIr = nullptr;

static HWND g_hTabBottom = nullptr;
static HWND g_hConsoleOut = nullptr;
static HWND g_hConsoleIn = nullptr;

static HWND g_hStatus = nullptr;

static int gActiveRight = 0;

static bool gWrapOn = false;
static int gUIFontPx = 14;
static int gCodeFontPx = 16;

static std::wstring gLastPath;

static bool gDraggingVSplit = false;
static int  gVSplitX = -1;

static bool gDraggingHSplit = false;
static int  gHSplitY = -1;

static HWND g_hTooltip = nullptr;

// Find UI (modern)
static HWND g_hFindWnd = nullptr;
static HWND g_hFindEdit = nullptr;
static HWND g_hFindBtnNext = nullptr;
static HWND g_hFindBtnClose = nullptr;
static std::wstring gFindNeedle;
static int gFindLastPos = 0;

static const int FIND_TOPBAR_H = 34;

static HACCEL g_hAccel = nullptr;

// ===================== Input Queue (IDE -> core input) =====================
static std::mutex gInMx;
static std::condition_variable gInCv;
static std::deque<std::wstring> gInQ;

static std::wstring IDE_ReadLineBlocking() {
    std::unique_lock<std::mutex> lk(gInMx);
    gInCv.wait(lk, [] { return !gInQ.empty(); });
    std::wstring s = std::move(gInQ.front());
    gInQ.pop_front();
    return s;
}

// ===================== Thread-safe output append =====================
static std::mutex gOutMx;
static std::wstring gPendingAppend;

static std::thread gRunThread;
static bool gRunActive = false;
static bool gRunUseVM = true;

// --------------------- Helpers ---------------------
static int ClampI(int v, int a, int b) { return v < a ? a : (v > b ? b : v); }
static void ApplyFont(HWND h, HFONT f) { SendMessageW(h, WM_SETFONT, (WPARAM)f, TRUE); }

static std::wstring GetTextW(HWND h) {
    int len = GetWindowTextLengthW(h);
    std::wstring s; s.resize((size_t)len + 1);
    GetWindowTextW(h, &s[0], len + 1);
    s.resize(wcslen(s.c_str()));
    return s;
}
static void SetTextW(HWND h, const std::wstring& s) { SetWindowTextW(h, s.c_str()); }

static COLORREF ClampRGB(int r, int g, int b) {
    auto c = [&](int x) { return x < 0 ? 0 : (x > 255 ? 255 : x); };
    return RGB(c(r), c(g), c(b));
}
static COLORREF Mix(COLORREF c, int add) {
    return ClampRGB((int)GetRValue(c) + add, (int)GetGValue(c) + add, (int)GetBValue(c) + add);
}

static void SetStatusTextLeft(const std::wstring& s) {
    if (g_hStatus) SendMessageW(g_hStatus, SB_SETTEXTW, 0, (LPARAM)s.c_str());
}
static void UI_AppendOutAsync(const std::wstring& s) {
    {
        std::lock_guard<std::mutex> lk(gOutMx);
        gPendingAppend += s;
    }
    if (g_hMain) PostMessageW(g_hMain, WM_APP_APPENDOUT, 0, 0);
}

// ===================== Fonts / Zoom =====================
static void RecreateFontsWithSize(int uiPx, int codePx) {
    if (gFontUI) { DeleteObject(gFontUI); gFontUI = nullptr; }
    if (gFontCode) { DeleteObject(gFontCode); gFontCode = nullptr; }

    LOGFONTW lf{};
    lf.lfHeight = -uiPx;
    wcscpy_s(lf.lfFaceName, L"Segoe UI");
    gFontUI = CreateFontIndirectW(&lf);

    ZeroMemory(&lf, sizeof(lf));
    lf.lfHeight = -codePx;
    wcscpy_s(lf.lfFaceName, L"Cascadia Code");
    gFontCode = CreateFontIndirectW(&lf);
    if (!gFontCode) {
        wcscpy_s(lf.lfFaceName, L"Consolas");
        gFontCode = CreateFontIndirectW(&lf);
    }

    for (HWND h : { g_hCode, g_hGutter, g_hTok, g_hAst, g_hIr, g_hConsoleOut, g_hConsoleIn }) {
        if (h) ApplyFont(h, gFontCode);
    }
    for (HWND h : { g_hTabRight, g_hTabBottom, g_hStatus }) {
        if (h) ApplyFont(h, gFontUI);
    }
    for (int id : { IDC_BTN_RUN, IDC_BTN_OPEN, IDC_BTN_SAVE, IDC_BTN_CLR, IDC_BTN_MORE }) {
        HWND b = GetDlgItem(g_hMain, id);
        if (b) ApplyFont(b, gFontUI);
    }
    if (g_hFindWnd && g_hFindEdit) ApplyFont(g_hFindEdit, gFontUI);
    if (g_hGutter) InvalidateRect(g_hGutter, nullptr, TRUE);
}

static int ZoomPercent() {
    int p = (int)((double)gCodeFontPx * 100.0 / 16.0 + 0.5);
    return ClampI(p, 60, 220);
}
static void ZoomReset() { gCodeFontPx = 16; RecreateFontsWithSize(gUIFontPx, gCodeFontPx); PostMessageW(g_hMain, WM_APP_UPDSTATUS, 0, 0); }
static void ZoomIn() { if (gCodeFontPx < 28) { gCodeFontPx++; RecreateFontsWithSize(gUIFontPx, gCodeFontPx); PostMessageW(g_hMain, WM_APP_UPDSTATUS, 0, 0); } }
static void ZoomOut() { if (gCodeFontPx > 10) { gCodeFontPx--; RecreateFontsWithSize(gUIFontPx, gCodeFontPx); PostMessageW(g_hMain, WM_APP_UPDSTATUS, 0, 0); } }

// ===================== Wrap toggle =====================
static void ApplyWrapState() {
    if (!g_hCode) return;
    SendMessageW(g_hCode, EM_SETTARGETDEVICE, 0, gWrapOn ? 0 : 1);

    LONG_PTR style = GetWindowLongPtrW(g_hCode, GWL_STYLE);
    if (gWrapOn) {
        style &= ~WS_HSCROLL;
        style &= ~ES_AUTOHSCROLL;
    }
    else {
        style |= WS_HSCROLL;
        style |= ES_AUTOHSCROLL;
    }
    SetWindowLongPtrW(g_hCode, GWL_STYLE, style);
    SetWindowPos(g_hCode, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

    PostMessageW(g_hMain, WM_APP_UPDSTATUS, 0, 0);
    if (g_hGutter) InvalidateRect(g_hGutter, nullptr, TRUE);
}

// ===================== Demo & Örnekler =====================
static std::wstring DemoCode() {
    return
        L"// === GariIDE: Ege Agzi Programlama Dili ===\r\n"
        L"sabit laf BOLGE = \"Ege Bolgesi\";\r\n\r\n"
        L"kalip_gari Zeybek {\r\n"
        L"    laf isim;\r\n"
        L"    tamsayi yas;\r\n"
        L"}\r\n\r\n"
        L"// Varsayilan parametreli fonksiyon (unvan verilmezse \"Efe\" alinir)\r\n"
        L"fonksiyon_gari bos_gari selamla_gari(laf kim, laf unvan = \"Efe\") {\r\n"
        L"    doktur(\"[\" + BOLGE + \"] Selam olsun sana, \" + buyut_gari(kim) + \" \" + unvan + \"!\");\r\n"
        L"}\r\n\r\n"
        L"fonksiyon_gari tamsayi faktoriyel(tamsayi n) {\r\n"
        L"    dondur_gari (n <= 1) ? 1 : (n * faktoriyel(n - 1));\r\n"
        L"}\r\n\r\n"
        L"basla_gari() {\r\n"
        L"    Zeybek efe;\r\n"
        L"    efe.isim = \"Yoruk Ali\";\r\n"
        L"    efe.yas = 28;\r\n"
        L"    selamla_gari(efe.isim);\r\n\r\n"
        L"    // Üçlü koşul operatörü (? :)\r\n"
        L"    laf durum = (efe.yas >= 18) ? \"Yetiskin Zeybek\" : \"Genc Kizan\";\r\n"
        L"    doktur(\"Durum: \" + durum);\r\n\r\n"
        L"    // Sozluk_gari (Map / Harita) kullanimi\r\n"
        L"    sozluk_gari tamsayi plakalar = [\"Aydin\": 9, \"Izmir\": 35, \"Mugla\": 48];\r\n"
        L"    plakalar[\"Denizli\"] = 20;\r\n"
        L"    doktur(\"Ege Plakalari: \" + lafa_cevir(plakalar));\r\n\r\n"
        L"    dizi_gari tamsayi sayilar = [30, 10, 40, 20];\r\n"
        L"    ekle_gari(sayilar, 50);\r\n"
        L"    sirala_gari(sayilar);\r\n\r\n"
        L"    tamsayi toplam = 0;\r\n"
        L"    gez_gari (tamsayi s : sayilar) {\r\n"
        L"        toplam += s;\r\n"
        L"    }\r\n"
        L"    doktur(\"Sirali Dizi Toplami: \" + lafa_cevir(toplam) + \" | 5! = \" + lafa_cevir(faktoriyel(5)));\r\n\r\n"
        L"    dene_gari {\r\n"
        L"        sayet (icinde_var_mi(plakalar, \"Aydin\") hem_de hakli) {\r\n"
        L"            firlat_gari(\"Aydin plakasi 0\" + lafa_cevir(plakalar[\"Aydin\"]) + \" bulundu gari!\");\r\n"
        L"        }\r\n"
        L"    } yakala_gari (laf bilgi) {\r\n"
        L"        doktur(\"Yakalanan: \" + bilgi);\r\n"
        L"    }\r\n\r\n"
        L"    dondur_gari 0;\r\n"
        L"}\r\n";
}

static std::wstring ExampleCode_Algorithms() {
    return
        L"// === Ornek 2: Ozyineleme (Fibonacci) ve Dizi siralama ===\r\n"
        L"fonksiyon_gari tamsayi fib(tamsayi n) {\r\n"
        L"    sayet (n <= 1) { dondur_gari n; }\r\n"
        L"    dondur_gari fib(n - 1) + fib(n - 2);\r\n"
        L"}\r\n\r\n"
        L"basla_gari() {\r\n"
        L"    dizi_gari tamsayi fibler = [];\r\n"
        L"    cevir_gari (tamsayi i = 0; i < 8; i++) {\r\n"
        L"        ekle_gari(fibler, fib(i));\r\n"
        L"    }\r\n"
        L"    doktur(\"Ilk 8 Fibonacci sayisi: \" + lafa_cevir(fibler));\r\n"
        L"    ters_cevir_gari(fibler);\r\n"
        L"    doktur(\"Ters cevrilmis hali: \" + lafa_cevir(fibler));\r\n"
        L"    dondur_gari 0;\r\n"
        L"}\r\n";
}

static std::wstring ExampleCode_StructsAndSwitch() {
    return
        L"// === Ornek 3: Kalip_gari (Struct), Gez_gari ve Bak_hele (Switch) ===\r\n"
        L"kalip_gari Zeytinlik {\r\n"
        L"    laf ilce;\r\n"
        L"    tamsayi agac_sayisi;\r\n"
        L"}\r\n\r\n"
        L"basla_gari() {\r\n"
        L"    Zeytinlik bahce;\r\n"
        L"    bahce.ilce = \"Nazilli\";\r\n"
        L"    bahce.agac_sayisi = 150;\r\n"
        L"    bahce.agac_sayisi += 50;\r\n\r\n"
        L"    doktur(\"Zeytinlik: \" + lafa_cevir(bahce));\r\n\r\n"
        L"    bak_hele (bahce.ilce) {\r\n"
        L"        olursa \"Milas\":\r\n"
        L"            doktur(\"Mugla zeytini gari!\");\r\n"
        L"            kir_gari;\r\n"
        L"        olursa \"Nazilli\":\r\n"
        L"            doktur(\"Aydin inciri ve zeytini bereketli olur gari!\");\r\n"
        L"            kir_gari;\r\n"
        L"        hicbiri:\r\n"
        L"            doktur(\"Baska bir Ege kasabasi.\");\r\n"
        L"    }\r\n\r\n"
        L"    dizi_gari laf kasabalar = [\"Tire\", \"Odemis\", \"Bodrum\", \"Fethiye\"];\r\n"
        L"    sirala_gari(kasabalar);\r\n"
        L"    gez_gari (laf k : kasabalar) {\r\n"
        L"        doktur(\"-> Gezilen Kasaba: \" + k);\r\n"
        L"    }\r\n"
        L"    dondur_gari 0;\r\n"
        L"}\r\n";
}

static std::wstring ExampleCode_TryCatchAndFiles() {
    return
        L"// === Ornek 4: Dene_gari / Yakala_gari (Hata Yakalama) ve Metin Islemleri ===\r\n"
        L"basla_gari() {\r\n"
        L"    laf cumle = \"   Ege denizi cok guzel gari!   \";\r\n"
        L"    cumle = kirp_gari(cumle);\r\n"
        L"    cumle = degistir_gari(cumle, \"guzel\", \"sahane\");\r\n"
        L"    doktur(\"Duzenlenen Laf: \" + cumle);\r\n\r\n"
        L"    dene_gari {\r\n"
        L"        doktur(\"Sifira bolme denemesi yapiliyor...\");\r\n"
        L"        tamsayi a = 42 / 0;\r\n"
        L"        doktur(a);\r\n"
        L"    } yakala_gari (laf hata) {\r\n"
        L"        doktur(\"Hata yakalandi, program cokmedi: \" + hata);\r\n"
        L"    }\r\n\r\n"
        L"    doktur(\"Rastgele sansli sayin: \" + lafa_cevir(rastgele_gari(1, 100)));\r\n"
        L"    dondur_gari 0;\r\n"
        L"}\r\n";
}

// ===================== File helpers =====================
static bool SaveTextToFile(const std::wstring& path, const std::wstring& text) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;

    DWORD w = 0;
    const WORD bom = 0xFEFF;
    WriteFile(h, &bom, sizeof(bom), &w, nullptr);
    WriteFile(h, text.data(), (DWORD)(text.size() * sizeof(wchar_t)), &w, nullptr);

    CloseHandle(h);
    return true;
}
static bool LoadTextFromFile(const std::wstring& path, std::wstring& outText) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;

    LARGE_INTEGER sz{};
    if (!GetFileSizeEx(h, &sz) || sz.QuadPart <= 0) { CloseHandle(h); outText.clear(); return true; }

    std::vector<BYTE> buf((size_t)sz.QuadPart);
    DWORD rd = 0;
    if (!ReadFile(h, buf.data(), (DWORD)buf.size(), &rd, nullptr)) { CloseHandle(h); return false; }
    CloseHandle(h);

    if (rd >= 2) {
        const WORD* pw = (const WORD*)buf.data();
        size_t wc = rd / 2;

        size_t start = 0;
        if (pw[0] == 0xFEFF) start = 1;
        outText.assign((const wchar_t*)(pw + start), (const wchar_t*)(pw + wc));
        return true;
    }
    outText.clear();
    return true;
}
static bool PromptOpenFile(HWND owner, std::wstring& outPath) {
    wchar_t file[MAX_PATH] = L"";
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = L"Gari Files (*.gari)\0*.gari\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (!GetOpenFileNameW(&ofn)) return false;
    outPath = file;
    return true;
}
static bool PromptSaveFile(HWND owner, std::wstring& outPath, bool saveAs) {
    wchar_t file[MAX_PATH] = L"";
    if (!saveAs && !gLastPath.empty()) {
        wcsncpy_s(file, gLastPath.c_str(), _TRUNCATE);
    }
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = L"Gari Files (*.gari)\0*.gari\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    ofn.lpstrDefExt = L"gari";
    if (!GetSaveFileNameW(&ofn)) return false;
    outPath = file;
    return true;
}

// ===================== Status bar parts =====================
static void UpdateStatusParts() {
    if (!g_hStatus) return;

    RECT rc{}; GetClientRect(g_hMain, &rc);
    int W = rc.right - rc.left;

    int wWrap = 110;
    int wZoom = 90;
    int wPos = 140;

    int parts[4];
    parts[3] = W;
    parts[2] = W - wWrap;
    parts[1] = W - wWrap - wZoom;
    parts[0] = W - wWrap - wZoom - wPos;

    SendMessageW(g_hStatus, SB_SETPARTS, 4, (LPARAM)parts);

    int line = 1, col = 1;
    if (g_hCode) {
        DWORD selStart = (DWORD)SendMessageW(g_hCode, EM_GETSEL, 0, 0);
        int ln0 = (int)SendMessageW(g_hCode, EM_LINEFROMCHAR, selStart, 0);
        int li = (int)SendMessageW(g_hCode, EM_LINEINDEX, ln0, 0);
        line = ln0 + 1;
        col = (int)selStart - li + 1;
        if (col < 1) col = 1;
    }

    std::wstringstream ssPos;
    ssPos << L"Ln " << line << L", Col " << col;

    std::wstringstream ssZoom;
    ssZoom << L"Zoom " << ZoomPercent() << L"%";

    std::wstring wrap = gWrapOn ? L"Wrap: ON" : L"Wrap: OFF";

    SendMessageW(g_hStatus, SB_SETTEXTW, 1, (LPARAM)ssPos.str().c_str());
    SendMessageW(g_hStatus, SB_SETTEXTW, 2, (LPARAM)ssZoom.str().c_str());
    SendMessageW(g_hStatus, SB_SETTEXTW, 3, (LPARAM)wrap.c_str());
}

// ===================== Gutter line numbers =====================
static int GetFirstVisibleLine() {
    if (!g_hCode) return 0;
    return (int)SendMessageW(g_hCode, EM_GETFIRSTVISIBLELINE, 0, 0);
}
static int GetLineCount() {
    if (!g_hCode) return 1;
    int c = (int)SendMessageW(g_hCode, EM_GETLINECOUNT, 0, 0);
    return (c <= 0) ? 1 : c;
}
static int GetCaretLine1Based() {
    if (!g_hCode) return 1;
    DWORD selStart = (DWORD)SendMessageW(g_hCode, EM_GETSEL, 0, 0);
    int ln0 = (int)SendMessageW(g_hCode, EM_LINEFROMCHAR, selStart, 0);
    return ln0 + 1;
}
static void GutterPaint(HWND h) {
    PAINTSTRUCT ps{};
    HDC dc = BeginPaint(h, &ps);

    RECT rc{}; GetClientRect(h, &rc);

    HBRUSH brBg = CreateSolidBrush(TOK_GUTTERBG);
    FillRect(dc, &rc, brBg);
    DeleteObject(brBg);

    SetBkMode(dc, TRANSPARENT);

    HFONT oldF = (HFONT)SelectObject(dc, gFontCode);

    TEXTMETRICW tm{};
    GetTextMetricsW(dc, &tm);
    int lineH = tm.tmHeight + tm.tmExternalLeading;
    if (lineH < 14) lineH = 14;

    int first = GetFirstVisibleLine();
    int caretLine = GetCaretLine1Based();
    int total = GetLineCount();

    int y = 6;
    int visibleLines = (rc.bottom - rc.top - 12) / lineH + 2;

    for (int i = 0; i < visibleLines; i++) {
        int ln0 = first + i;
        int ln1 = ln0 + 1;
        if (ln1 > total + 1) break;

        RECT rLine{ rc.left + 0, y - 1, rc.right - 6, y + lineH - 1 };

        if (ln1 == caretLine) {
            HBRUSH brHi = CreateSolidBrush(TOK_GUTTERHI);
            FillRect(dc, &rLine, brHi);
            DeleteObject(brHi);
            SetTextColor(dc, TOK_CODE_TEXT);
        }
        else {
            SetTextColor(dc, TOK_MUTED);
        }

        wchar_t buf[32];
        wsprintfW(buf, L"%d", ln1);

        RECT rText{ rc.left + 0, y, rc.right - 8, y + lineH };
        DrawTextW(dc, buf, -1, &rText, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

        y += lineH;
    }

    HPEN pen = CreatePen(PS_SOLID, 1, TOK_BORDER);
    HGDIOBJ oldP = SelectObject(dc, pen);
    MoveToEx(dc, rc.right - 1, rc.top, nullptr);
    LineTo(dc, rc.right - 1, rc.bottom);
    SelectObject(dc, oldP);
    DeleteObject(pen);

    SelectObject(dc, oldF);
    EndPaint(h, &ps);
}
static LRESULT CALLBACK GutterProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: GutterPaint(h); return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

// ===================== Tab helpers =====================
static void ShowRightView(int idx) {
    gActiveRight = idx;
    ShowWindow(g_hTok, idx == 0 ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hAst, idx == 1 ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hIr, idx == 2 ? SW_SHOW : SW_HIDE);
}

// TabControl erase fix
static LRESULT CALLBACK TabSubclassProc(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR) {
    switch (m) {
    case WM_ERASEBKGND: {
        HDC dc = (HDC)w;
        RECT rc; GetClientRect(h, &rc);
        FillRect(dc, &rc, gBrTab ? gBrTab : gBrSurface);
        return 1;
    }
    }
    return DefSubclassProc(h, m, w, l);
}

// ===================== Find (modern) =====================
static void FindNext() {
    if (!g_hCode) return;
    if (gFindNeedle.empty()) return;

    std::wstring hay = GetTextW(g_hCode);
    if (hay.empty()) return;

    size_t pos = hay.find(gFindNeedle, (size_t)gFindLastPos);
    if (pos == std::wstring::npos) {
        pos = hay.find(gFindNeedle, 0);
        if (pos == std::wstring::npos) {
            SetStatusTextLeft(L"Bulunamadı");
            MessageBeep(MB_ICONWARNING);
            return;
        }
    }

    int start = (int)pos;
    int end = (int)(pos + gFindNeedle.size());

    SendMessageW(g_hCode, EM_SETSEL, start, end);
    SendMessageW(g_hCode, EM_SCROLLCARET, 0, 0);
    SetFocus(g_hCode);

    gFindLastPos = end;
    SetStatusTextLeft(L"Bulundu");
    PostMessageW(g_hMain, WM_APP_UPDSTATUS, 0, 0);
}

static void Find_ApplyRounded(HWND h) {
    RECT rc{}; GetClientRect(h, &rc);
    int w = rc.right - rc.left;
    int hh = rc.bottom - rc.top;
    if (w <= 0 || hh <= 0) return;
    HRGN rgn = CreateRoundRectRgn(0, 0, w + 1, hh + 1, 14, 14);
    SetWindowRgn(h, rgn, TRUE);
}

static void Find_EnableDwmRounding(HWND) {
}

// Owner-draw buttons for Find (Next/Close)
static LRESULT DrawFindButton(DRAWITEMSTRUCT* dis) {
    HDC dc = dis->hDC;
    RECT r = dis->rcItem;

    bool down = (dis->itemState & ODS_SELECTED) != 0;
    bool hot = (dis->itemState & ODS_HOTLIGHT) != 0;
    bool disb = (dis->itemState & ODS_DISABLED) != 0;

    COLORREF bg = TOK_FIND_BT;
    if (disb) bg = RGB(80, 40, 45);
    else if (down) bg = TOK_FIND_BT_D;
    else if (hot)  bg = TOK_FIND_BT_H;

    HBRUSH br = CreateSolidBrush(bg);
    FillRect(dc, &r, br);
    DeleteObject(br);

    HPEN pen = CreatePen(PS_SOLID, 1, TOK_FIND_BORDER);
    HGDIOBJ oldP = SelectObject(dc, pen);
    HGDIOBJ oldB = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
    RoundRect(dc, r.left, r.top, r.right, r.bottom, 6, 6);
    SelectObject(dc, oldB);
    SelectObject(dc, oldP);
    DeleteObject(pen);

    wchar_t text[64] = L"";
    GetWindowTextW(dis->hwndItem, text, 63);

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, TOK_TEXT);
    HFONT oldF = (HFONT)SelectObject(dc, gFontUI);
    DrawTextW(dc, text, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(dc, oldF);

    return TRUE;
}

static void Find_Paint(HWND h) {
    PAINTSTRUCT ps{};
    HDC dc = BeginPaint(h, &ps);

    RECT rc{}; GetClientRect(h, &rc);

    FillRect(dc, &rc, gBrFindBg);

    RECT top = rc;
    top.bottom = top.top + FIND_TOPBAR_H;
    FillRect(dc, &top, gBrFindSurf);

    RECT acc = top;
    acc.top = acc.bottom - 2;
    HBRUSH brA = CreateSolidBrush(TOK_ACCENT);
    FillRect(dc, &acc, brA);
    DeleteObject(brA);

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, TOK_FIND_TITLETX);
    HFONT oldF = (HFONT)SelectObject(dc, gFontUI);

    RECT tr = top;
    tr.left += 12;
    tr.right -= 12;
    DrawTextW(dc, L"Bul", -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    SelectObject(dc, oldF);

    HPEN pen = CreatePen(PS_SOLID, 1, TOK_FIND_BORDER);
    HGDIOBJ oldP = SelectObject(dc, pen);
    HGDIOBJ oldB = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
    RoundRect(dc, rc.left, rc.top, rc.right, rc.bottom, 14, 14);
    SelectObject(dc, oldB);
    SelectObject(dc, oldP);
    DeleteObject(pen);

    EndPaint(h, &ps);
}

static void Find_Layout(HWND h) {
    RECT rc{}; GetClientRect(h, &rc);
    int W = rc.right - rc.left;
    int pad = 12;

    int y = FIND_TOPBAR_H + 12;
    int editH = 28;
    int btnH = 28;
    int gap = 10;

    int btnW = 88;
    int closeW = 78;

    int rightBtns = btnW + gap + closeW;
    int editW = W - pad * 2 - rightBtns - gap;
    if (editW < 140) editW = 140;

    int x = pad;

    if (g_hFindEdit) MoveWindow(g_hFindEdit, x, y, editW, editH, TRUE);
    x += editW + gap;

    if (g_hFindBtnNext)  MoveWindow(g_hFindBtnNext, x, y, btnW, btnH, TRUE);
    x += btnW + gap;

    if (g_hFindBtnClose) MoveWindow(g_hFindBtnClose, x, y, closeW, btnH, TRUE);
}

static LRESULT CALLBACK FindWndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_CREATE: {
        g_hFindWnd = h;

        if (!gBrFindBg)   gBrFindBg = CreateSolidBrush(TOK_FIND_BG);
        if (!gBrFindSurf) gBrFindSurf = CreateSolidBrush(TOK_FIND_SURF);
        if (!gBrFindEdit) gBrFindEdit = CreateSolidBrush(TOK_FIND_EDIT);

        g_hFindEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            0, 0, 10, 10, h, (HMENU)1, GetModuleHandleW(nullptr), nullptr);
        ApplyFont(g_hFindEdit, gFontUI);

        g_hFindBtnNext = CreateWindowExW(0, L"BUTTON", L"Sonraki",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            0, 0, 10, 10, h, (HMENU)2, GetModuleHandleW(nullptr), nullptr);
        ApplyFont(g_hFindBtnNext, gFontUI);

        g_hFindBtnClose = CreateWindowExW(0, L"BUTTON", L"Kapat",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            0, 0, 10, 10, h, (HMENU)3, GetModuleHandleW(nullptr), nullptr);
        ApplyFont(g_hFindBtnClose, gFontUI);

        Find_EnableDwmRounding(h);
        Find_ApplyRounded(h);
        Find_Layout(h);

        return 0;
    }

    case WM_SIZE:
        Find_ApplyRounded(h);
        Find_Layout(h);
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
        Find_Paint(h);
        return 0;

    case WM_NCHITTEST: {
        LRESULT ht = DefWindowProcW(h, m, w, l);
        if (ht == HTCLIENT) {
            POINT pt{ GET_X_LPARAM(l), GET_Y_LPARAM(l) };
            ScreenToClient(h, &pt);
            if (pt.y >= 0 && pt.y < FIND_TOPBAR_H) return HTCAPTION;
        }
        return ht;
    }

    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT: {
        HDC dc = (HDC)w;
        HWND hc = (HWND)l;

        SetBkMode(dc, OPAQUE);

        if (hc == g_hFindEdit) {
            SetBkColor(dc, TOK_FIND_EDIT);
            SetTextColor(dc, TOK_CODE_TEXT);
            return (INT_PTR)gBrFindEdit;
        }

        SetBkColor(dc, TOK_FIND_BG);
        SetTextColor(dc, TOK_CODE_TEXT);
        return (INT_PTR)gBrFindBg;
    }

    case WM_DRAWITEM: {
        DRAWITEMSTRUCT* dis = (DRAWITEMSTRUCT*)l;
        if (dis && dis->CtlType == ODT_BUTTON) {
            return DrawFindButton(dis);
        }
        break;
    }

    case WM_COMMAND: {
        int id = LOWORD(w);
        if (id == 2) {
            gFindNeedle = GetTextW(g_hFindEdit);
            gFindLastPos = 0;
            FindNext();
            return 0;
        }
        if (id == 3) { DestroyWindow(h); return 0; }
        return 0;
    }

    case WM_KEYDOWN:
        if (w == VK_ESCAPE) { DestroyWindow(h); return 0; }
        break;

    case WM_CLOSE:
        DestroyWindow(h);
        return 0;

    case WM_DESTROY:
        g_hFindWnd = nullptr;
        g_hFindEdit = nullptr;
        g_hFindBtnNext = nullptr;
        g_hFindBtnClose = nullptr;
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

static void ShowFindWindow(HWND owner) {
    if (g_hFindWnd) { ShowWindow(g_hFindWnd, SW_SHOW); SetForegroundWindow(g_hFindWnd); return; }

    WNDCLASSW wc{};
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"GariIDE_FindWnd";
    wc.lpfnWndProc = FindWndProc;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    RegisterClassW(&wc);

    RECT r{}; GetWindowRect(owner, &r);
    int x = r.left + 240, y = r.top + 220;

    HWND h = CreateWindowExW(
        WS_EX_TOOLWINDOW,
        wc.lpszClassName,
        L"",
        WS_POPUP | WS_VISIBLE,
        x, y, 520, 110,
        owner, nullptr, wc.hInstance, nullptr
    );

    ShowWindow(h, SW_SHOW);
    UpdateWindow(h);
    SetForegroundWindow(h);
}

// ===================== Errors: parse + underline =====================
struct ErrMark {
    int line = 0;
    int col = 0;
};

struct Problem {
    int line = 0;
    int col = 0;
};

static std::vector<Problem> ParseProblemsFromText(const std::wstring& text) {
    std::vector<Problem> out;
    std::wregex r1(
        LR"((TypeError|ParseError|RuntimeError|LexError)[^\n]*?(?:line|Ln)\s*[:=]?\s*(\d+)[^\n]*?(?:col|Col)\s*[:=]?\s*(\d+))",
        std::regex_constants::icase
    );

    std::wsmatch m;
    std::wstring::const_iterator it = text.begin();
    while (std::regex_search(it, text.end(), m, r1)) {
        Problem p;
        p.line = _wtoi(m[2].str().c_str());
        p.col = _wtoi(m[3].str().c_str());
        out.push_back(p);
        it = m.suffix().first;
    }
    return out;
}

static int Editor_PosFromLineCol(int line1, int col1) {
    if (!g_hCode) return 0;
    if (line1 < 1) line1 = 1;
    if (col1 < 1) col1 = 1;
    int ln0 = line1 - 1;
    int idx = (int)SendMessageW(g_hCode, EM_LINEINDEX, ln0, 0);
    if (idx < 0) idx = 0;
    return idx + (col1 - 1);
}

static int Editor_GuessTokenLenAt(int pos) {
    if (!g_hCode) return 1;
    std::wstring all = GetTextW(g_hCode);
    if (pos < 0 || (size_t)pos >= all.size()) return 1;
    if (iswspace(all[(size_t)pos])) return 1;

    int len = 0;
    while ((size_t)(pos + len) < all.size() && len < 40) {
        wchar_t ch = all[(size_t)(pos + len)];
        if (iswspace(ch) || ch == L';' || ch == L')' || ch == L'(' || ch == L'{' || ch == L'}' || ch == L',')
            break;
        len++;
    }
    return (len <= 0) ? 1 : len;
}

static bool gInSyntaxHighlight = false;

static void Editor_HighlightSyntax() {
    if (!g_hCode || gInSyntaxHighlight) return;
    gInSyntaxHighlight = true;

    DWORD selStart = 0, selEnd = 0;
    SendMessageW(g_hCode, EM_GETSEL, (WPARAM)&selStart, (LPARAM)&selEnd);
    POINT scrollPt{};
    SendMessageW(g_hCode, EM_GETSCROLLPOS, 0, (LPARAM)&scrollPt);

    LRESULT oldMask = SendMessageW(g_hCode, EM_SETEVENTMASK, 0, 0);
    SendMessageW(g_hCode, WM_SETREDRAW, FALSE, 0);

    // RichEdit50W içinde \r\n tek karakter (\r) olarak indekslenir. EM_GETTEXTEX veya \r\n -> \r dönüşümü ile tam indeks eşleşmesi sağlıyoruz:
    std::wstring raw = GetTextW(g_hCode);
    std::wstring text;
    text.reserve(raw.size());
    for (size_t k = 0; k < raw.size(); ++k) {
        if (raw[k] == L'\r' && k + 1 < raw.size() && raw[k + 1] == L'\n') {
            text.push_back(L'\n');
            k++;
        } else {
            text.push_back(raw[k]);
        }
    }

    auto setFormat = [&](int start, int end, COLORREF clr, DWORD effects = 0) {
        if (end <= start) return;
        SendMessageW(g_hCode, EM_SETSEL, start, end);
        CHARFORMAT2W cf{};
        cf.cbSize = sizeof(cf);
        cf.dwMask = CFM_COLOR | CFM_UNDERLINE | CFM_BOLD | CFM_ITALIC;
        cf.dwEffects = effects;
        cf.crTextColor = clr;
        SendMessageW(g_hCode, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
    };

    // 1. Tüm metni varsayılan krem beyaza sıfırla
    setFormat(0, (int)text.size(), TOK_CODE_TEXT, 0);

    // Renk paleti (Koyu Crimson arka plan üzerinde yüksek kontrastlı)
    const COLORREF CLR_KEYWORD = RGB(255, 210, 85);  // Parlak Altın Sarısı (Ege ağzı kontrol kelimeleri)
    const COLORREF CLR_TYPE    = RGB(110, 235, 255); // Parlak Turkuaz (Veri tipleri)
    const COLORREF CLR_BUILTIN = RGB(255, 175, 220); // Açık Pembe/Eflatun (Yerleşik fonksiyonlar)
    const COLORREF CLR_STRING  = RGB(165, 245, 155); // Yumuşak Açık Yeşil (Laf / String literalleri)
    const COLORREF CLR_NUMBER  = RGB(195, 180, 255); // Açık Lavanta (Sayılar ve hakli/haksiz)
    const COLORREF CLR_COMMENT = RGB(185, 145, 150); // Sönük Gül/Gri (Yorum satırları)

    size_t i = 0, n = text.size();
    while (i < n) {
        wchar_t c = text[i];

        // Tek satırlı yorum: //
        if (c == L'/' && i + 1 < n && text[i + 1] == L'/') {
            size_t start = i;
            while (i < n && text[i] != L'\n') i++;
            setFormat((int)start, (int)i, CLR_COMMENT, CFE_ITALIC);
            continue;
        }
        // Çok satırlı yorum: /* ... */
        if (c == L'/' && i + 1 < n && text[i + 1] == L'*') {
            size_t start = i;
            i += 2;
            while (i < n && !(text[i] == L'*' && i + 1 < n && text[i + 1] == L'/')) i++;
            if (i < n) i += 2;
            setFormat((int)start, (int)i, CLR_COMMENT, CFE_ITALIC);
            continue;
        }
        // String literal: "..."
        if (c == L'"') {
            size_t start = i++;
            while (i < n && text[i] != L'"') {
                if (text[i] == L'\\' && i + 1 < n) i += 2;
                else i++;
            }
            if (i < n) i++;
            setFormat((int)start, (int)i, CLR_STRING, 0);
            continue;
        }
        // Sayılar
        if (std::iswdigit(c)) {
            size_t start = i;
            while (i < n && (std::iswdigit(text[i]) || text[i] == L'.')) i++;
            setFormat((int)start, (int)i, CLR_NUMBER, 0);
            continue;
        }
        // Tanımlayıcılar ve Anahtar Kelimeler
        if (std::iswalpha(c) || c == L'_') {
            size_t start = i;
            while (i < n && (std::iswalnum(text[i]) || text[i] == L'_')) i++;
            std::wstring word = text.substr(start, i - start);

            if (word == L"basla_gari" || word == L"fonksiyon_gari" || word == L"kalip_gari" ||
                word == L"sayet" || word == L"yoksa" || word == L"oldukca" || word == L"bi_yap_hele" ||
                word == L"cevir_gari" || word == L"gez_gari" || word == L"dondur_gari" ||
                word == L"kir_gari" || word == L"pas_gec" || word == L"bak_hele" || word == L"olursa" ||
                word == L"hicbiri" || word == L"dene_gari" || word == L"yakala_gari" || word == L"firlat_gari" ||
                word == L"doktur" || word == L"hem_de" || word == L"ya_da" || word == L"degil_gari") {
                setFormat((int)start, (int)i, CLR_KEYWORD, CFE_BOLD);
            }
            else if (word == L"tamsayi" || word == L"hassas_sayi" || word == L"cift_hassas_sayi" ||
                     word == L"laf" || word == L"mantik" || word == L"dizi_gari" || word == L"sozluk_gari" ||
                     word == L"bos_gari" || word == L"sabit") {
                setFormat((int)start, (int)i, CLR_TYPE, CFE_BOLD);
            }
            else if (word == L"hakli" || word == L"haksiz") {
                setFormat((int)start, (int)i, CLR_NUMBER, CFE_BOLD);
            }
            else if (word == L"oku_gari" || word == L"oku_tamsayi" || word == L"oku_hassas_sayi" ||
                     word == L"boyut_gari" || word == L"ekle_gari" || word == L"cikar_gari" ||
                     word == L"sirala_gari" || word == L"ters_cevir_gari" || word == L"rastgele_gari" ||
                     word == L"lafa_cevir" || word == L"tamsayiya_cevir" || word == L"hassasa_cevir" ||
                     word == L"buyut_gari" || word == L"kucult_gari" || word == L"parcala_gari" ||
                     word == L"icinde_var_mi" || word == L"degistir_gari" || word == L"kirp_gari" ||
                     word == L"dosya_yaz_gari" || word == L"dosya_oku_gari" || word == L"dosya_var_mi" ||
                     word == L"bekle_gari" || word == L"vakit_gari" || word == L"anahtarlar_gari" ||
                     word == L"sil_gari" || word == L"kok" || word == L"us" ||
                     word == L"mutlak" || word == L"min_gari" || word == L"max_gari") {
                setFormat((int)start, (int)i, CLR_BUILTIN, 0);
            }
            continue;
        }
        i++;
    }

    SendMessageW(g_hCode, EM_SETSEL, selStart, selEnd);
    SendMessageW(g_hCode, EM_SETSCROLLPOS, 0, (LPARAM)&scrollPt);
    SendMessageW(g_hCode, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(g_hCode, nullptr, FALSE);
    SendMessageW(g_hCode, EM_SETEVENTMASK, 0, oldMask);

    gInSyntaxHighlight = false;
}

static void Editor_ClearAllMarks() {
    Editor_HighlightSyntax();
}

static void Editor_ApplyMark(int line1, int col1) {
    if (!g_hCode) return;
    int pos = Editor_PosFromLineCol(line1, col1);
    int len = Editor_GuessTokenLenAt(pos);

    SendMessageW(g_hCode, EM_SETSEL, pos, pos + len);

    CHARFORMAT2W cf{};
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_UNDERLINE | CFM_COLOR;
    cf.dwEffects = CFE_UNDERLINE;
    cf.crTextColor = RGB(255, 190, 60); // Kırmızı arka planda parlayan altın-sarı hata dalgası
    SendMessageW(g_hCode, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);

    SendMessageW(g_hCode, EM_SETSEL, pos, pos);
    SendMessageW(g_hCode, EM_SCROLLCARET, 0, 0);

    if (g_hGutter) InvalidateRect(g_hGutter, nullptr, TRUE);
    PostMessageW(g_hMain, WM_APP_UPDSTATUS, 0, 0);
}

static void Editor_ClearErrorMarks() { Editor_ClearAllMarks(); }
static void Editor_MarkError(int line1, int col1) { Editor_ApplyMark(line1, col1); }

// ===================== Enable/disable when running =====================
static void EnableRunUI(HWND hwnd, bool on) {
    EnableWindow(GetDlgItem(hwnd, IDC_BTN_RUN), on);
    EnableWindow(GetDlgItem(hwnd, IDC_BTN_OPEN), TRUE);
    EnableWindow(GetDlgItem(hwnd, IDC_BTN_SAVE), TRUE);
    EnableWindow(GetDlgItem(hwnd, IDC_BTN_CLR), TRUE);
    EnableWindow(GetDlgItem(hwnd, IDC_BTN_MORE), TRUE);

    if (g_hConsoleIn) EnableWindow(g_hConsoleIn, TRUE);
}

// ===================== Command bar buttons =====================
static const wchar_t* BtnGlyph(int id) {
    switch (id) {
    case IDC_BTN_RUN:  return L"▶";
    case IDC_BTN_OPEN: return L"📂";
    case IDC_BTN_SAVE: return L"💾";
    case IDC_BTN_CLR:  return L"🧹";
    case IDC_BTN_MORE: return L"…";
    default: return L"•";
    }
}
static const wchar_t* BtnTip(int id) {
    switch (id) {
    case IDC_BTN_RUN:  return L"Çalıştır (F5)";
    case IDC_BTN_OPEN: return L"Aç (Ctrl+O)";
    case IDC_BTN_SAVE: return L"Kaydet (Ctrl+S)";
    case IDC_BTN_CLR:  return L"Temizle";
    case IDC_BTN_MORE: return L"Diğer Komutlar";
    default: return L"";
    }
}

// ===================== Tooltips =====================
static void EnsureTooltips(HWND hwnd) {
    if (g_hTooltip) return;
    g_hTooltip = CreateWindowExW(0, TOOLTIPS_CLASSW, nullptr,
        WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);

    SetWindowPos(g_hTooltip, HWND_TOPMOST, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

    auto addTip = [&](HWND ctrl, int id) {
        if (!ctrl) return;
        TOOLINFOW ti{};
        ti.cbSize = sizeof(ti);
        ti.uFlags = TTF_SUBCLASS;
        ti.hwnd = hwnd;
        ti.uId = (UINT_PTR)ctrl;
        ti.lpszText = (LPWSTR)BtnTip(id);
        GetClientRect(ctrl, &ti.rect);
        SendMessageW(g_hTooltip, TTM_ADDTOOLW, 0, (LPARAM)&ti);
        };

    addTip(GetDlgItem(hwnd, IDC_BTN_RUN), IDC_BTN_RUN);
    addTip(GetDlgItem(hwnd, IDC_BTN_OPEN), IDC_BTN_OPEN);
    addTip(GetDlgItem(hwnd, IDC_BTN_SAVE), IDC_BTN_SAVE);
    addTip(GetDlgItem(hwnd, IDC_BTN_CLR), IDC_BTN_CLR);
    addTip(GetDlgItem(hwnd, IDC_BTN_MORE), IDC_BTN_MORE);
}

// ===================== Owner-draw tabs (MODERN FLAT TAB) =====================
static void DrawTabModern(HWND tab, DRAWITEMSTRUCT* dis) {
    HDC dc = dis->hDC;
    RECT r = dis->rcItem;
    bool sel = (dis->itemState & ODS_SELECTED) != 0;

    COLORREF bg = sel ? TOK_BG : TOK_SURFACE;
    COLORREF tx = sel ? TOK_CODE_TEXT : TOK_TEXT;

    HBRUSH br = CreateSolidBrush(bg);
    FillRect(dc, &r, br);
    DeleteObject(br);

    if (sel) {
        RECT ind = r;
        ind.top = ind.bottom - 2;
        HBRUSH brI = CreateSolidBrush(TOK_ACCENT);
        FillRect(dc, &ind, brI);
        DeleteObject(brI);
    }

    HPEN bPen = CreatePen(PS_SOLID, 1, TOK_BORDER);
    HGDIOBJ oldP = SelectObject(dc, bPen);
    MoveToEx(dc, r.left, r.top, nullptr);
    LineTo(dc, r.right, r.top);
    SelectObject(dc, oldP);
    DeleteObject(bPen);

    wchar_t text[64] = L"";
    TCITEMW ti{};
    ti.mask = TCIF_TEXT;
    ti.pszText = text;
    ti.cchTextMax = 63;
    TabCtrl_GetItem(tab, (int)dis->itemID, &ti);

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, tx);
    HFONT oldF = (HFONT)SelectObject(dc, gFontUI);

    RECT tr = r;
    tr.left += 6; tr.right -= 6;
    DrawTextW(dc, text, -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(dc, oldF);
}

// ===================== Command Palette =====================
static void ShowCommandMenu(HWND hwnd) {
    HMENU m = CreatePopupMenu();
    AppendMenuW(m, MF_STRING, IDM_RUN_F5, L"Çalıştır (VM)\tF5");
    AppendMenuW(m, MF_STRING, IDM_RUN_INTERP, L"Çalıştır (Interpreter)\tCtrl+F5");
    AppendMenuW(m, MF_STRING, IDM_FILE_OPEN, L"Aç...\tCtrl+O");
    AppendMenuW(m, MF_STRING, IDM_FILE_SAVE, L"Kaydet\tCtrl+S");
    AppendMenuW(m, MF_STRING, IDM_EDIT_FIND, L"Bul...\tCtrl+F");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, IDM_VIEW_WRAP, gWrapOn ? L"Wrap Kapat" : L"Wrap Aç");
    AppendMenuW(m, MF_STRING, IDM_VIEW_ZOOMIN, L"Zoom +\tCtrl++");
    AppendMenuW(m, MF_STRING, IDM_VIEW_ZOOMOUT, L"Zoom -\tCtrl+-");
    AppendMenuW(m, MF_STRING, IDM_VIEW_ZOOMRESET, L"Zoom Sıfırla\tCtrl+0");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, IDM_VIEW_SHOW_TOKEN, L"Token Göster (Sağ)");
    AppendMenuW(m, MF_STRING, IDM_VIEW_SHOW_AST, L"AST Göster (Sağ)");
    AppendMenuW(m, MF_STRING, IDM_VIEW_SHOW_IR, L"IR Göster (Sağ)");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, IDM_TOOLS_SETTINGS, L"Ayarlar");
    AppendMenuW(m, MF_STRING, IDM_HELP_ABOUT, L"Hakkında");

    POINT pt{};
    GetCursorPos(&pt);

    UINT cmd = TrackPopupMenu(
        m,
        TPM_TOPALIGN | TPM_RETURNCMD,
        pt.x, pt.y,
        0, hwnd, nullptr
    );
    DestroyMenu(m);

    if (cmd != 0) {
        SendMessageW(hwnd, WM_COMMAND, (WPARAM)cmd, 0);
    }
}

// ===================== Accelerators =====================
static void BuildAccelerators() {
    if (g_hAccel) { DeleteObject(g_hAccel); g_hAccel = nullptr; }

    std::vector<ACCEL> a;
    auto add = [&](BYTE fVirt, WORD key, WORD cmd) {
        ACCEL ac{};
        ac.fVirt = fVirt;
        ac.key = key;
        ac.cmd = cmd;
        a.push_back(ac);
        };

    add(FCONTROL | FVIRTKEY, 'S', IDM_FILE_SAVE);
    add(FCONTROL | FVIRTKEY, 'O', IDM_FILE_OPEN);
    add(FCONTROL | FVIRTKEY, 'F', IDM_EDIT_FIND);

    add(FCONTROL | FVIRTKEY, '0', IDM_VIEW_ZOOMRESET);
    add(FCONTROL | FVIRTKEY, VK_OEM_PLUS, IDM_VIEW_ZOOMIN);
    add(FCONTROL | FVIRTKEY, VK_ADD, IDM_VIEW_ZOOMIN);
    add(FCONTROL | FVIRTKEY, VK_OEM_MINUS, IDM_VIEW_ZOOMOUT);
    add(FCONTROL | FVIRTKEY, VK_SUBTRACT, IDM_VIEW_ZOOMOUT);

    add(FVIRTKEY, VK_F5, IDM_RUN_F5);
    add(FCONTROL | FVIRTKEY, VK_F5, IDM_RUN_INTERP);
    add(FCONTROL | FSHIFT | FVIRTKEY, 'P', IDM_TOOLS_COMMANDPALETTE);

    g_hAccel = CreateAcceleratorTableW(a.data(), (int)a.size());
}

// ===================== Menu =====================
static void BuildMenuBar(HWND hwnd) {
    HMENU hMenubar = CreateMenu();

    HMENU hFile = CreatePopupMenu();
    AppendMenuW(hFile, MF_STRING, IDM_FILE_NEW, L"Yeni\tCtrl+N");
    AppendMenuW(hFile, MF_STRING, IDM_FILE_OPEN, L"Aç...\tCtrl+O");
    AppendMenuW(hFile, MF_STRING, IDM_FILE_SAVE, L"Kaydet\tCtrl+S");
    AppendMenuW(hFile, MF_STRING, IDM_FILE_SAVEAS, L"Farklı Kaydet...");
    AppendMenuW(hFile, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hFile, MF_STRING, IDM_FILE_EXIT, L"Çıkış");
    AppendMenuW(hMenubar, MF_POPUP, (UINT_PTR)hFile, L"Dosya");

    HMENU hEdit = CreatePopupMenu();
    AppendMenuW(hEdit, MF_STRING, IDM_EDIT_UNDO, L"Geri Al\tCtrl+Z");
    AppendMenuW(hEdit, MF_STRING, IDM_EDIT_REDO, L"İleri Al\tCtrl+Y");
    AppendMenuW(hEdit, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hEdit, MF_STRING, IDM_EDIT_CUT, L"Kes\tCtrl+X");
    AppendMenuW(hEdit, MF_STRING, IDM_EDIT_COPY, L"Kopyala\tCtrl+C");
    AppendMenuW(hEdit, MF_STRING, IDM_EDIT_PASTE, L"Yapıştır\tCtrl+V");
    AppendMenuW(hEdit, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hEdit, MF_STRING, IDM_EDIT_FIND, L"Bul...\tCtrl+F");
    AppendMenuW(hMenubar, MF_POPUP, (UINT_PTR)hEdit, L"Düzenle");

    HMENU hView = CreatePopupMenu();
    AppendMenuW(hView, MF_STRING, IDM_VIEW_ZOOMIN, L"Zoom +\tCtrl++");
    AppendMenuW(hView, MF_STRING, IDM_VIEW_ZOOMOUT, L"Zoom -\tCtrl+-");
    AppendMenuW(hView, MF_STRING, IDM_VIEW_ZOOMRESET, L"Zoom Sıfırla\tCtrl+0");
    AppendMenuW(hView, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hView, MF_STRING, IDM_VIEW_WRAP, L"Satır Kaydırma (Wrap)");
    AppendMenuW(hMenubar, MF_POPUP, (UINT_PTR)hView, L"Görünüm");

    HMENU hExamples = CreatePopupMenu();
    AppendMenuW(hExamples, MF_STRING, IDM_EXAMPLE_1, L"1. Genel Ege Ağzı Turu (Varsayılan)");
    AppendMenuW(hExamples, MF_STRING, IDM_EXAMPLE_2, L"2. Özyineleme (Fibonacci) ve Dizi İşlemleri");
    AppendMenuW(hExamples, MF_STRING, IDM_EXAMPLE_3, L"3. Kalıp (Struct), Gez_gari ve Bak_hele (Switch)");
    AppendMenuW(hExamples, MF_STRING, IDM_EXAMPLE_4, L"4. Hata Yakalama (Dene/Yakala) ve Metin İşlemleri");
    AppendMenuW(hMenubar, MF_POPUP, (UINT_PTR)hExamples, L"Örnekler");

    HMENU hTools = CreatePopupMenu();
    AppendMenuW(hTools, MF_STRING, IDM_TOOLS_COMMANDPALETTE, L"Komut Menüsü\tCtrl+Shift+P");
    AppendMenuW(hTools, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hTools, MF_STRING, IDM_TOOLS_SETTINGS, L"Ayarlar");
    AppendMenuW(hMenubar, MF_POPUP, (UINT_PTR)hTools, L"Araçlar");

    HMENU hHelp = CreatePopupMenu();
    AppendMenuW(hHelp, MF_STRING, IDM_HELP_LANGREF, L"Ege Ağzı Dil Sözlüğü (Kılavuz)");
    AppendMenuW(hHelp, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hHelp, MF_STRING, IDM_HELP_ABOUT, L"Hakkında");
    AppendMenuW(hMenubar, MF_POPUP, (UINT_PTR)hHelp, L"Yardım");

    SetMenu(hwnd, hMenubar);
}

// ===================== Layout =====================
static void Layout(HWND hwnd) {
    RECT rc{}; GetClientRect(hwnd, &rc);
    int W = rc.right - rc.left;
    int H = rc.bottom - rc.top;

    int pad = 6;
    int contentTop = gTopH + pad;
    int contentBottom = H - gStatusH - pad;
    int contentH = contentBottom - contentTop;
    if (contentH < 260) contentH = 260;

    gRightW = ClampI(gRightW, gMinRightW, gMaxRightW);
    gBottomH = ClampI(gBottomH, gMinBottomH, gMaxBottomH);

    int mainAreaH = contentH - gBottomH - gSplitterW;
    if (mainAreaH < 180) {
        mainAreaH = 180;
        gBottomH = contentH - mainAreaH - gSplitterW;
        gBottomH = ClampI(gBottomH, gMinBottomH, gMaxBottomH);
    }

    int codeX = pad;
    int codeWTotal = W - pad * 3 - gRightW - gSplitterW;
    if (codeWTotal < 240) codeWTotal = 240;

    int vSplitX = codeX + codeWTotal + pad;
    gVSplitX = vSplitX;

    int rightX = vSplitX + gSplitterW;
    int rightW = W - rightX - pad;
    if (rightW < gMinRightW) rightW = gMinRightW;

    int hSplitY = contentTop + mainAreaH + 0;
    gHSplitY = hSplitY;

    int bx = pad, by = 10, bh = 32, gap = 6;
    auto mvb = [&](int id, int w = 36) {
        HWND b = GetDlgItem(hwnd, id);
        if (!b) return;
        MoveWindow(b, bx, by, w, bh, TRUE);
        bx += w + gap;
        };

    mvb(IDC_BTN_RUN, 42);
    mvb(IDC_BTN_OPEN, 38);
    mvb(IDC_BTN_SAVE, 38);
    mvb(IDC_BTN_CLR, 38);

    int moreW = 38;
    HWND bMore = GetDlgItem(hwnd, IDC_BTN_MORE);
    if (bMore) MoveWindow(bMore, W - pad - moreW, by, moreW, bh, TRUE);

    int gutterX = codeX;
    int editorX = codeX + gGutterW;
    int editorW = codeWTotal - gGutterW;
    if (editorW < 120) editorW = 120;

    if (g_hGutter) MoveWindow(g_hGutter, gutterX, contentTop, gGutterW, mainAreaH, TRUE);
    if (g_hCode)   MoveWindow(g_hCode, editorX, contentTop, editorW, mainAreaH, TRUE);

    int tabH = 30;
    MoveWindow(g_hTabRight, rightX, contentTop, rightW, tabH, TRUE);

    int viewY = contentTop + tabH;
    int viewH = mainAreaH - tabH;
    if (viewH < 80) viewH = 80;

    MoveWindow(g_hTok, rightX, viewY, rightW, viewH, TRUE);
    MoveWindow(g_hAst, rightX, viewY, rightW, viewH, TRUE);
    MoveWindow(g_hIr, rightX, viewY, rightW, viewH, TRUE);

    int bottomY = hSplitY + gSplitterW;
    int bottomH = gBottomH;

    MoveWindow(g_hTabBottom, pad, bottomY, W - pad * 2, tabH, TRUE);

    int bViewY = bottomY + tabH;
    int bViewH = bottomH - tabH;

    int inH = 28;
    int outH = bViewH - inH - 6;
    if (outH < 60) outH = 60;

    MoveWindow(g_hConsoleOut, pad, bViewY, W - pad * 2, outH, TRUE);
    MoveWindow(g_hConsoleIn, pad, bViewY + outH + 6, W - pad * 2, inH, TRUE);

    if (g_hStatus) SendMessageW(g_hStatus, WM_SIZE, 0, 0);

    ShowRightView(gActiveRight);
    UpdateStatusParts();
    InvalidateRect(hwnd, nullptr, TRUE);
}

// ===================== InEdit subclass (Console input Enter) =====================
static WNDPROC gOldConsoleInProc = nullptr;
static LRESULT CALLBACK ConsoleInProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_KEYDOWN && w == VK_RETURN) {
        std::wstring line = GetTextW(h);
        SetTextW(h, L"");

        UI_AppendOutAsync(L"> " + line + L"\r\n");

        {
            std::lock_guard<std::mutex> lk(gInMx);
            gInQ.push_back(line);
        }
        gInCv.notify_one();
        return 0;
    }
    return CallWindowProcW(gOldConsoleInProc, h, m, w, l);
}

// ===================== Editor subclass (Ctrl+Wheel zoom + Auto-Indent + Auto-Complete + status/gutter) =====================
static WNDPROC gOldCodeProc = nullptr;

static const wchar_t* const kAutoCompleteWords[] = {
    L"basla_gari", L"fonksiyon_gari", L"kalip_gari", L"dondur_gari", L"doktur",
    L"sayet", L"yoksa", L"oldukca", L"bi_yap_hele", L"cevir_gari", L"gez_gari",
    L"bak_hele", L"olursa", L"hicbiri", L"kir_gari", L"pas_gec",
    L"dene_gari", L"yakala_gari", L"firlat_gari",
    L"tamsayi", L"hassas_sayi", L"cift_hassas_sayi", L"laf", L"mantik", L"bos_gari", L"sabit",
    L"dizi_gari", L"sozluk_gari", L"hakli", L"haksiz", L"hem_de", L"ya_da", L"degil_gari",
    L"oku_gari", L"oku_tamsayi", L"oku_hassas_sayi",
    L"boyut_gari", L"ekle_gari", L"cikar_gari", L"sirala_gari", L"ters_cevir_gari",
    L"anahtarlar_gari", L"sil_gari", L"icinde_var_mi", L"rastgele_gari",
    L"lafa_cevir", L"tamsayiya_cevir", L"hassasa_cevir",
    L"buyut_gari", L"kucult_gari", L"kirp_gari", L"parcala_gari", L"degistir_gari",
    L"dosya_yaz_gari", L"dosya_oku_gari", L"dosya_var_mi", L"bekle_gari", L"vakit_gari"
};

static std::wstring GetRichEditNormalizedText(HWND h) {
    std::wstring raw = GetTextW(h);
    std::wstring text;
    text.reserve(raw.size());
    for (size_t k = 0; k < raw.size(); ++k) {
        if (raw[k] == L'\r' && k + 1 < raw.size() && raw[k + 1] == L'\n') {
            text.push_back(L'\n');
            k++;
        } else {
            text.push_back(raw[k]);
        }
    }
    return text;
}

static void TriggerAutoComplete(HWND h) {
    DWORD selStart = 0, selEnd = 0;
    SendMessageW(h, EM_GETSEL, (WPARAM)&selStart, (LPARAM)&selEnd);
    std::wstring text = GetRichEditNormalizedText(h);
    if (selStart > text.size()) selStart = (DWORD)text.size();

    int wordStart = (int)selStart;
    while (wordStart > 0 && (std::iswalnum(text[(size_t)(wordStart - 1)]) || text[(size_t)(wordStart - 1)] == L'_')) {
        wordStart--;
    }
    std::wstring prefix = text.substr((size_t)wordStart, (size_t)(selStart - wordStart));

    std::vector<std::wstring> matches;
    for (auto cand : kAutoCompleteWords) {
        std::wstring c = cand;
        if (prefix.empty() || (c.size() >= prefix.size() && c.compare(0, prefix.size(), prefix) == 0)) {
            matches.push_back(c);
        }
    }
    if (matches.empty()) return;

    if (matches.size() == 1) {
        SendMessageW(h, EM_SETSEL, wordStart, selStart);
        SendMessageW(h, EM_REPLACESEL, TRUE, (LPARAM)matches[0].c_str());
        return;
    }

    HMENU m = CreatePopupMenu();
    for (size_t idx = 0; idx < matches.size(); ++idx) {
        AppendMenuW(m, MF_STRING, 5000 + idx, matches[idx].c_str());
    }
    POINT pt{};
    GetCaretPos(&pt);
    ClientToScreen(h, &pt);
    UINT chosen = TrackPopupMenu(m, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD, pt.x, pt.y + 20, 0, h, nullptr);
    DestroyMenu(m);

    if (chosen >= 5000 && (chosen - 5000) < matches.size()) {
        const std::wstring& rep = matches[chosen - 5000];
        SendMessageW(h, EM_SETSEL, wordStart, selStart);
        SendMessageW(h, EM_REPLACESEL, TRUE, (LPARAM)rep.c_str());
    }
}

static LRESULT CALLBACK CodeEditProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_MOUSEWHEEL) {
        if (GetKeyState(VK_CONTROL) & 0x8000) {
            int delta = GET_WHEEL_DELTA_WPARAM(w);
            if (delta > 0) ZoomIn(); else ZoomOut();
            return 0;
        }
    }
    if (m == WM_KEYDOWN) {
        // Ctrl + Space: Satır içi otomatik tamamlama
        if (w == VK_SPACE && (GetKeyState(VK_CONTROL) & 0x8000)) {
            TriggerAutoComplete(h);
            return 0;
        }
        // Tab tuşu: 4 boşluk ekle (veya kelime başındaysa ve Ctrl basılı değilse 4 boşluk)
        if (w == VK_TAB) {
            SendMessageW(h, EM_REPLACESEL, TRUE, (LPARAM)L"    ");
            return 0;
        }
        // Enter tuşu: Otomatik girintileme (Auto-Indent)
        if (w == VK_RETURN) {
            DWORD selStart = 0, selEnd = 0;
            SendMessageW(h, EM_GETSEL, (WPARAM)&selStart, (LPARAM)&selEnd);
            std::wstring text = GetRichEditNormalizedText(h);
            if (selStart > text.size()) selStart = (DWORD)text.size();

            int lineStart = (int)selStart;
            while (lineStart > 0 && text[(size_t)(lineStart - 1)] != L'\n') lineStart--;

            std::wstring indent;
            int p = lineStart;
            while (p < (int)selStart && (text[(size_t)p] == L' ' || text[(size_t)p] == L'\t')) {
                indent.push_back(text[(size_t)p]);
                p++;
            }

            // Satırın son anlamlı karakteri '{' veya ':' ise 4 boşluk ekstra girinti ekle
            int lastNonWS = (int)selStart - 1;
            while (lastNonWS >= lineStart && (text[(size_t)lastNonWS] == L' ' || text[(size_t)lastNonWS] == L'\t')) {
                lastNonWS--;
            }
            if (lastNonWS >= lineStart && (text[(size_t)lastNonWS] == L'{' || text[(size_t)lastNonWS] == L':')) {
                indent += L"    ";
            }

            std::wstring ins = L"\r\n" + indent;
            SendMessageW(h, EM_REPLACESEL, TRUE, (LPARAM)ins.c_str());
            if (g_hGutter) InvalidateRect(g_hGutter, nullptr, TRUE);
            PostMessageW(g_hMain, WM_APP_UPDSTATUS, 0, 0);
            return 0;
        }
    }
    if (m == WM_CHAR) {
        if (w == L' ' && (GetKeyState(VK_CONTROL) & 0x8000)) {
            return 0; // Ctrl+Space karakter basmasını engelle
        }
        // '}' yazıldığında satır sadece boşluklardan oluşuyorsa otomatik 4 boşluk geri çek
        if (w == L'}') {
            DWORD selStart = 0, selEnd = 0;
            SendMessageW(h, EM_GETSEL, (WPARAM)&selStart, (LPARAM)&selEnd);
            std::wstring text = GetRichEditNormalizedText(h);
            if (selStart == selEnd && selStart <= text.size()) {
                int lineStart = (int)selStart;
                while (lineStart > 0 && text[(size_t)(lineStart - 1)] != L'\n') lineStart--;
                bool allSpaces = true;
                for (int k = lineStart; k < (int)selStart; ++k) {
                    if (text[(size_t)k] != L' ') { allSpaces = false; break; }
                }
                if (allSpaces && ((int)selStart - lineStart) >= 4) {
                    SendMessageW(h, EM_SETSEL, selStart - 4, selStart);
                    SendMessageW(h, EM_REPLACESEL, TRUE, (LPARAM)L"}");
                    return 0;
                }
            }
        }
    }
    if (m == WM_VSCROLL || m == WM_HSCROLL || m == WM_KEYUP || m == WM_LBUTTONUP || m == WM_CHAR) {
        if (g_hGutter) InvalidateRect(g_hGutter, nullptr, TRUE);
        PostMessageW(g_hMain, WM_APP_UPDSTATUS, 0, 0);
    }
    return CallWindowProcW(gOldCodeProc, h, m, w, l);
}

// ===================== Overflow menu =====================
static void ShowOverflowMenu(HWND hwnd, int xScreen, int yScreen) {
    HMENU m = CreatePopupMenu();
    AppendMenuW(m, MF_STRING, IDM_RUN_F5, L"Çalıştır (VM)\tF5");
    AppendMenuW(m, MF_STRING, IDM_RUN_INTERP, L"Çalıştır (Interpreter)\tCtrl+F5");
    AppendMenuW(m, MF_STRING, IDM_TOOLS_COMMANDPALETTE, L"Komut Menüsü\tCtrl+Shift+P");
    AppendMenuW(m, MF_STRING, IDM_EDIT_FIND, L"Bul...\tCtrl+F");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, IDM_VIEW_SHOW_TOKEN, L"Token Göster (Sağ)");
    AppendMenuW(m, MF_STRING, IDM_VIEW_SHOW_AST, L"AST Göster (Sağ)");
    AppendMenuW(m, MF_STRING, IDM_VIEW_SHOW_IR, L"IR Göster (Sağ)");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, IDM_VIEW_WRAP, gWrapOn ? L"Wrap Kapat" : L"Wrap Aç");
    AppendMenuW(m, MF_STRING, IDM_VIEW_ZOOMIN, L"Zoom +");
    AppendMenuW(m, MF_STRING, IDM_VIEW_ZOOMOUT, L"Zoom -");
    AppendMenuW(m, MF_STRING, IDM_VIEW_ZOOMRESET, L"Zoom Sıfırla");

    TrackPopupMenu(m, TPM_LEFTALIGN | TPM_TOPALIGN, xScreen, yScreen, 0, hwnd, nullptr);
    DestroyMenu(m);
}

// ===================== Paint command/top area + splitters =====================
static void PaintBackground(HWND hwnd) {
    PAINTSTRUCT ps{};
    HDC dc = BeginPaint(hwnd, &ps);

    RECT rc{}; GetClientRect(hwnd, &rc);

    RECT top{ rc.left, rc.top, rc.right, rc.top + gTopH };
    FillRect(dc, &top, gBrSurface);

    RECT bg{ rc.left, rc.top + gTopH, rc.right, rc.bottom };
    FillRect(dc, &bg, gBrBg);

    HPEN sPen = CreatePen(PS_SOLID, 1, TOK_BORDER);
    HGDIOBJ oldP = SelectObject(dc, sPen);

    if (gVSplitX >= 0) {
        RECT sp{ gVSplitX, gTopH, gVSplitX + gSplitterW, rc.bottom - gStatusH };
        FillRect(dc, &sp, gBrBg);
        MoveToEx(dc, gVSplitX, gTopH, nullptr);
        LineTo(dc, gVSplitX, rc.bottom - gStatusH);
        MoveToEx(dc, gVSplitX + gSplitterW - 1, gTopH, nullptr);
        LineTo(dc, gVSplitX + gSplitterW - 1, rc.bottom - gStatusH);
    }
    if (gHSplitY >= 0) {
        RECT sp{ 0, gHSplitY, rc.right, gHSplitY + gSplitterW };
        FillRect(dc, &sp, gBrBg);
        MoveToEx(dc, 0, gHSplitY, nullptr);
        LineTo(dc, rc.right, gHSplitY);
        MoveToEx(dc, 0, gHSplitY + gSplitterW - 1, nullptr);
        LineTo(dc, rc.right, gHSplitY + gSplitterW - 1);
    }

    MoveToEx(dc, rc.left, gTopH - 1, nullptr);
    LineTo(dc, rc.right, gTopH - 1);

    SelectObject(dc, oldP);
    DeleteObject(sPen);

    EndPaint(hwnd, &ps);
}

// ===================== Button owner-draw (MODERN SOFT PASTEL) =====================
static LRESULT DrawButton(DRAWITEMSTRUCT* dis) {
    int id = (int)dis->CtlID;
    HDC dc = dis->hDC;
    RECT r = dis->rcItem;

    bool down = (dis->itemState & ODS_SELECTED) != 0;
    bool hot = (dis->itemState & ODS_HOTLIGHT) != 0;
    bool disb = (dis->itemState & ODS_DISABLED) != 0;

    COLORREF bgClr = TOK_SURFACE;
    COLORREF glyphClr = TOK_TEXT;

    if (disb) {
        glyphClr = TOK_MUTED;
    }
    else if (down) {
        bgClr = TOK_SURFACE2;
        if (id == IDC_BTN_RUN) glyphClr = TOK_GOOD;
    }
    else if (hot) {
        bgClr = TOK_SURFACE2;
        if (id == IDC_BTN_RUN) glyphClr = TOK_GOOD;
        else glyphClr = RGB(255, 255, 255);
    }
    else {
        if (id == IDC_BTN_RUN) glyphClr = TOK_GOOD;
    }

    HBRUSH br = CreateSolidBrush(bgClr);
    FillRect(dc, &r, br);
    DeleteObject(br);

    if (hot && !disb) {
        HPEN bPen = CreatePen(PS_SOLID, 1, TOK_BORDER);
        HGDIOBJ oldP = SelectObject(dc, bPen);
        HGDIOBJ oldB = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
        RoundRect(dc, r.left, r.top, r.right, r.bottom, 4, 4);
        SelectObject(dc, oldB);
        SelectObject(dc, oldP);
        DeleteObject(bPen);
    }

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, glyphClr);
    HFONT oldF = (HFONT)SelectObject(dc, gFontUI);

    std::wstring g = BtnGlyph(id);
    DrawTextW(dc, g.c_str(), -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(dc, oldF);
    return TRUE;
}

// ===================== Window Proc =====================
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        g_hMain = hwnd;
        LoadLibraryW(L"Msftedit.dll");

        if (!gBrBg)           gBrBg = CreateSolidBrush(TOK_BG);
        if (!gBrSurface)      gBrSurface = CreateSolidBrush(TOK_SURFACE);
        if (!gBrSurface2)     gBrSurface2 = CreateSolidBrush(TOK_SURFACE2);
        if (!gBrTab)          gBrTab = CreateSolidBrush(TOK_SURFACE);
        if (!gBrEdit)         gBrEdit = CreateSolidBrush(TOK_BG);
        if (!gBrEditorWhite)  gBrEditorWhite = CreateSolidBrush(TOK_EDITOR_BG);

        RecreateFontsWithSize(gUIFontPx, gCodeFontPx);

        BuildMenuBar(hwnd);
        BuildAccelerators();

        Core_SetInputProvider(&IDE_ReadLineBlocking);

        auto mkbtn = [&](int id) {
            HWND b = CreateWindowExW(0, L"BUTTON", L"",
                WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                0, 0, 10, 10, hwnd, (HMENU)(INT_PTR)id, GetModuleHandleW(nullptr), nullptr);
            ApplyFont(b, gFontUI);
            };

        mkbtn(IDC_BTN_RUN);
        mkbtn(IDC_BTN_OPEN);
        mkbtn(IDC_BTN_SAVE);
        mkbtn(IDC_BTN_CLR);
        mkbtn(IDC_BTN_MORE);

        WNDCLASSW gwc{};
        gwc.hInstance = GetModuleHandleW(nullptr);
        gwc.lpszClassName = L"GariIDE_Gutter";
        gwc.lpfnWndProc = GutterProc;
        gwc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        gwc.hbrBackground = nullptr;
        RegisterClassW(&gwc);

        g_hGutter = CreateWindowExW(0, gwc.lpszClassName, L"",
            WS_CHILD | WS_VISIBLE,
            0, 0, 10, 10, hwnd, (HMENU)(INT_PTR)IDC_GUTTER, GetModuleHandleW(nullptr), nullptr);
        ApplyFont(g_hGutter, gFontCode);

        g_hCode = CreateWindowExW(0, MSFTEDIT_CLASS, L"",
            WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | WS_VSCROLL | WS_HSCROLL,
            0, 0, 10, 10, hwnd, (HMENU)(INT_PTR)IDC_EDIT_CODE, GetModuleHandleW(nullptr), nullptr);
        ApplyFont(g_hCode, gFontCode);

        SendMessageW(g_hCode, EM_SETBKGNDCOLOR, 0, (LPARAM)TOK_EDITOR_BG);
        {
            CHARFORMAT2W cf{};
            cf.cbSize = sizeof(cf);
            cf.dwMask = CFM_COLOR | CFM_UNDERLINE;
            cf.crTextColor = TOK_CODE_TEXT;
            cf.dwEffects = 0;
            SendMessageW(g_hCode, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);
        }

        SendMessageW(g_hCode, EM_SETEVENTMASK, 0, (LPARAM)(ENM_SELCHANGE | ENM_CHANGE));
        gOldCodeProc = (WNDPROC)SetWindowLongPtrW(g_hCode, GWLP_WNDPROC, (LONG_PTR)CodeEditProc);

        INITCOMMONCONTROLSEX icc{ sizeof(icc), ICC_TAB_CLASSES | ICC_BAR_CLASSES };
        InitCommonControlsEx(&icc);

        g_hTabRight = CreateWindowExW(0, WC_TABCONTROLW, L"",
            WS_CHILD | WS_VISIBLE | TCS_OWNERDRAWFIXED,
            0, 0, 10, 10, hwnd, (HMENU)(INT_PTR)IDC_TAB_RIGHT, GetModuleHandleW(nullptr), nullptr);
        ApplyFont(g_hTabRight, gFontUI);
        SetWindowSubclass(g_hTabRight, TabSubclassProc, 1, 0);

        TCITEMW ti{};
        ti.mask = TCIF_TEXT;
        ti.pszText = (LPWSTR)L"Token"; TabCtrl_InsertItem(g_hTabRight, 0, &ti);
        ti.pszText = (LPWSTR)L"AST";   TabCtrl_InsertItem(g_hTabRight, 1, &ti);
        ti.pszText = (LPWSTR)L"IR";    TabCtrl_InsertItem(g_hTabRight, 2, &ti);

        auto mkview = [&](int id, bool readonly = true) {
            DWORD style = WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL;
            if (readonly) style |= ES_READONLY;
            HWND e = CreateWindowExW(0, L"EDIT", L"",
                style, 0, 0, 10, 10, hwnd, (HMENU)(INT_PTR)id, GetModuleHandleW(nullptr), nullptr);
            ApplyFont(e, gFontCode);
            return e;
            };

        g_hTok = mkview(IDC_VIEW_TOK, true);
        g_hAst = mkview(IDC_VIEW_AST, true);
        g_hIr = mkview(IDC_VIEW_IR, true);

        g_hTabBottom = CreateWindowExW(0, WC_TABCONTROLW, L"",
            WS_CHILD | WS_VISIBLE | TCS_OWNERDRAWFIXED,
            0, 0, 10, 10, hwnd, (HMENU)(INT_PTR)IDC_TAB_BOTTOM, GetModuleHandleW(nullptr), nullptr);
        ApplyFont(g_hTabBottom, gFontUI);
        SetWindowSubclass(g_hTabBottom, TabSubclassProc, 2, 0);

        ti.pszText = (LPWSTR)L"Console"; TabCtrl_InsertItem(g_hTabBottom, 0, &ti);

        g_hConsoleOut = mkview(IDC_CONSOLE_OUT, true);
        g_hConsoleIn = CreateWindowExW(0, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            0, 0, 10, 10, hwnd, (HMENU)(INT_PTR)IDC_CONSOLE_IN, GetModuleHandleW(nullptr), nullptr);
        ApplyFont(g_hConsoleIn, gFontCode);
        gOldConsoleInProc = (WNDPROC)SetWindowLongPtrW(g_hConsoleIn, GWLP_WNDPROC, (LONG_PTR)ConsoleInProc);

        g_hStatus = CreateWindowExW(0, STATUSCLASSNAMEW, nullptr,
            WS_CHILD | WS_VISIBLE,
            0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)IDC_STATUS, GetModuleHandleW(nullptr), nullptr);
        ApplyFont(g_hStatus, gFontUI);

        SetStatusTextLeft(L"Hazır");

        SetTextW(g_hCode, DemoCode());
        Editor_HighlightSyntax();
        ApplyWrapState();

        SendMessageW(g_hTabRight, TCM_SETITEMSIZE, 0, MAKELPARAM(100, 26));
        SendMessageW(g_hTabBottom, TCM_SETITEMSIZE, 0, MAKELPARAM(100, 26));

        TabCtrl_SetCurSel(g_hTabRight, 0);
        TabCtrl_SetCurSel(g_hTabBottom, 0);
        ShowRightView(0);

        EnsureTooltips(hwnd);
        EnableRunUI(hwnd, true);

        Layout(hwnd);
        PostMessageW(hwnd, WM_APP_UPDSTATUS, 0, 0);
        return 0;
    }

    case WM_SIZE:
        Layout(hwnd);
        return 0;

    case WM_MEASUREITEM: {
        MEASUREITEMSTRUCT* mi = (MEASUREITEMSTRUCT*)lParam;
        if (mi && mi->CtlType == ODT_TAB) {
            mi->itemHeight = 26;
            mi->itemWidth = 90;
            return TRUE;
        }
        break;
    }

    case WM_SETCURSOR: {
        if (LOWORD(lParam) == HTCLIENT) {
            POINT pt; GetCursorPos(&pt); ScreenToClient(hwnd, &pt);

            if (gVSplitX >= 0 && pt.x >= gVSplitX - 1 && pt.x <= gVSplitX + gSplitterW + 1) {
                SetCursor(LoadCursor(nullptr, IDC_SIZEWE));
                return TRUE;
            }
            if (gHSplitY >= 0 && pt.y >= gHSplitY - 1 && pt.y <= gHSplitY + gSplitterW + 1) {
                SetCursor(LoadCursor(nullptr, IDC_SIZENS));
                return TRUE;
            }
        }
        break;
    }

    case WM_LBUTTONDOWN: {
        POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        if (gVSplitX >= 0 && pt.x >= gVSplitX - 1 && pt.x <= gVSplitX + gSplitterW + 1) {
            gDraggingVSplit = true;
            SetCapture(hwnd);
            return 0;
        }
        if (gHSplitY >= 0 && pt.y >= gHSplitY - 1 && pt.y <= gHSplitY + gSplitterW + 1) {
            gDraggingHSplit = true;
            SetCapture(hwnd);
            return 0;
        }
        break;
    }

    case WM_MOUSEMOVE: {
        if (gDraggingVSplit) {
            RECT rc{}; GetClientRect(hwnd, &rc);
            int W = rc.right - rc.left;
            int pad = 6;

            int x = GET_X_LPARAM(lParam);
            int rightX = x + gSplitterW;
            int newRightW = W - rightX - pad;
            newRightW = ClampI(newRightW, gMinRightW, gMaxRightW);
            gRightW = newRightW;
            Layout(hwnd);
            return 0;
        }
        if (gDraggingHSplit) {
            RECT rc{}; GetClientRect(hwnd, &rc);
            int H = rc.bottom - rc.top;
            int pad = 6;

            int y = GET_Y_LPARAM(lParam);
            int contentBottom = H - gStatusH - pad;

            int bottomY = y + gSplitterW;
            int newBottomH = contentBottom - bottomY;
            newBottomH = ClampI(newBottomH, gMinBottomH, gMaxBottomH);

            gBottomH = newBottomH;
            Layout(hwnd);
            return 0;
        }
        break;
    }

    case WM_LBUTTONUP:
        if (gDraggingVSplit || gDraggingHSplit) {
            gDraggingVSplit = false;
            gDraggingHSplit = false;
            ReleaseCapture();
            return 0;
        }
        break;

    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT: {
        HDC hdc = (HDC)wParam;
        HWND hctl = (HWND)lParam;

        SetBkMode(hdc, OPAQUE);

        if (hctl == g_hStatus) {
            SetBkColor(hdc, TOK_SURFACE);
            SetTextColor(hdc, TOK_TEXT);
            return (LRESULT)gBrSurface;
        }

        if (hctl == g_hCode) {
            SetBkColor(hdc, TOK_EDITOR_BG);
            SetTextColor(hdc, TOK_CODE_TEXT);
            return (LRESULT)gBrEditorWhite;
        }

        if (hctl == g_hTok || hctl == g_hAst || hctl == g_hIr ||
            hctl == g_hConsoleOut || hctl == g_hConsoleIn) {
            SetBkColor(hdc, TOK_BG);
            SetTextColor(hdc, TOK_CODE_TEXT);
            return (LRESULT)gBrEdit;
        }

        if (hctl == g_hTabRight || hctl == g_hTabBottom) {
            SetBkColor(hdc, TOK_SURFACE);
            SetTextColor(hdc, TOK_TEXT);
            return (LRESULT)gBrTab;
        }

        SetBkColor(hdc, TOK_SURFACE);
        SetTextColor(hdc, TOK_TEXT);
        return (LRESULT)gBrSurface;
    }

    case WM_DRAWITEM: {
        DRAWITEMSTRUCT* dis = (DRAWITEMSTRUCT*)lParam;
        if (dis->CtlType == ODT_TAB) {
            DrawTabModern(dis->hwndItem, dis);
            return TRUE;
        }
        if (dis->CtlType == ODT_BUTTON) {
            return DrawButton(dis);
        }
        break;
    }

    case WM_NOTIFY: {
        NMHDR* hdr = (NMHDR*)lParam;
        if (hdr->hwndFrom == g_hTabRight && hdr->code == TCN_SELCHANGE) {
            int idx = TabCtrl_GetCurSel(g_hTabRight);
            ShowRightView(idx);
            return 0;
        }
        break;
    }

    case WM_APP_APPENDOUT: {
        std::wstring add;
        {
            std::lock_guard<std::mutex> lk(gOutMx);
            add.swap(gPendingAppend);
        }
        if (!add.empty() && g_hConsoleOut) {
            TabCtrl_SetCurSel(g_hTabBottom, 0);
            SendMessageW(g_hConsoleOut, EM_SETSEL, (WPARAM)-1, (LPARAM)-1);
            SendMessageW(g_hConsoleOut, EM_REPLACESEL, TRUE, (LPARAM)add.c_str());
        }
        return 0;
    }

    case WM_APP_UPDSTATUS:
        UpdateStatusParts();
        return 0;

    case WM_APP_SETERRMARK: {
        auto* em = (ErrMark*)lParam;
        Editor_ClearErrorMarks();
        if (em && em->line > 0 && em->col > 0) {
            Editor_MarkError(em->line, em->col);
            SetStatusTextLeft(L"Hata işaretlendi");
        }
        delete em;
        return 0;
    }

    case WM_APP_RUNFINISH:
        gRunActive = false;
        EnableRunUI(hwnd, true);
        Editor_ClearErrorMarks();
        SetStatusTextLeft(L"RUN OK");
        PostMessageW(hwnd, WM_APP_UPDSTATUS, 0, 0);
        return 0;

    case WM_COMMAND: {
        int id = LOWORD(wParam);
        int code = HIWORD(wParam);
        HWND from = (HWND)lParam;

        if (from == g_hCode && code == EN_SELCHANGE) {
            if (g_hGutter) InvalidateRect(g_hGutter, nullptr, TRUE);
            PostMessageW(hwnd, WM_APP_UPDSTATUS, 0, 0);
            return 0;
        }
        if (from == g_hCode && code == EN_CHANGE) {
            Editor_HighlightSyntax();
            if (g_hGutter) InvalidateRect(g_hGutter, nullptr, TRUE);
            PostMessageW(hwnd, WM_APP_UPDSTATUS, 0, 0);
            return 0;
        }

        auto DoFileOpen = [&]() {
            std::wstring path;
            if (!PromptOpenFile(hwnd, path)) return;
            std::wstring text;
            if (!LoadTextFromFile(path, text)) {
                MessageBoxW(hwnd, L"Dosya okunamadı.", L"Hata", MB_OK | MB_ICONERROR);
                return;
            }
            gLastPath = path;
            SetTextW(g_hCode, text);
            SetStatusTextLeft(L"Açıldı");
            Editor_ClearErrorMarks();
            if (g_hGutter) InvalidateRect(g_hGutter, nullptr, TRUE);
            PostMessageW(hwnd, WM_APP_UPDSTATUS, 0, 0);
            };

        auto DoFileSave = [&](bool saveAs) {
            std::wstring path;
            if (!PromptSaveFile(hwnd, path, saveAs)) return;
            std::wstring text = GetTextW(g_hCode);
            if (!SaveTextToFile(path, text)) {
                MessageBoxW(hwnd, L"Dosya yazılamadı.", L"Hata", MB_OK | MB_ICONERROR);
                return;
            }
            gLastPath = path;
            SetStatusTextLeft(L"Kaydedildi");
            PostMessageW(hwnd, WM_APP_UPDSTATUS, 0, 0);
            };

        auto DoNew = [&]() {
            if (gRunActive) { SetStatusTextLeft(L"RUN aktifken yeni"); return; }
            gLastPath.clear();
            SetTextW(g_hCode, L"basla_gari() {\r\n  dondur_gari 0;\r\n}\r\n");
            SetStatusTextLeft(L"Yeni");
            Editor_ClearErrorMarks();
            if (g_hGutter) InvalidateRect(g_hGutter, nullptr, TRUE);
            PostMessageW(hwnd, WM_APP_UPDSTATUS, 0, 0);
            };

        auto DoClearAll = [&]() {
            SetTextW(g_hConsoleOut, L"");
            SetTextW(g_hTok, L"");
            SetTextW(g_hAst, L"");
            SetTextW(g_hIr, L"");
            SetStatusTextLeft(L"Temizlendi");
            Editor_ClearErrorMarks();
            PostMessageW(hwnd, WM_APP_UPDSTATUS, 0, 0);
            };

        auto DoBuildInspector = [&]() {
            std::wstring src = GetTextW(g_hCode);
            SetTextW(g_hTok, BuildTokensTextFromSource(src));
            SetTextW(g_hAst, BuildAstTextFromSource(src));
            SetTextW(g_hIr, BuildIRTextFromSource(src));
            };

        auto FocusRight = [&](int idx) {
            TabCtrl_SetCurSel(g_hTabRight, idx);
            ShowRightView(idx);
            };

        switch (id) {
        case IDM_FILE_NEW:    DoNew(); return 0;
        case IDM_FILE_OPEN:   DoFileOpen(); return 0;
        case IDM_FILE_SAVE:   DoFileSave(false); return 0;
        case IDM_FILE_SAVEAS: DoFileSave(true); return 0;
        case IDM_FILE_EXIT:   PostMessageW(hwnd, WM_CLOSE, 0, 0); return 0;

        case IDM_EDIT_UNDO:   SendMessageW(g_hCode, WM_UNDO, 0, 0); return 0;
        case IDM_EDIT_REDO:   SendMessageW(g_hCode, EM_REDO, 0, 0); return 0;
        case IDM_EDIT_CUT:    SendMessageW(g_hCode, WM_CUT, 0, 0); return 0;
        case IDM_EDIT_COPY:   SendMessageW(g_hCode, WM_COPY, 0, 0); return 0;
        case IDM_EDIT_PASTE:  SendMessageW(g_hCode, WM_PASTE, 0, 0); return 0;
        case IDM_EDIT_FIND:   ShowFindWindow(hwnd); return 0;

        case IDM_VIEW_ZOOMIN:    ZoomIn(); Layout(hwnd); return 0;
        case IDM_VIEW_ZOOMOUT:   ZoomOut(); Layout(hwnd); return 0;
        case IDM_VIEW_ZOOMRESET: ZoomReset(); Layout(hwnd); return 0;

        case IDM_VIEW_WRAP:
            gWrapOn = !gWrapOn;
            ApplyWrapState();
            SetStatusTextLeft(gWrapOn ? L"Wrap: ON" : L"Wrap: OFF");
            InvalidateRect(hwnd, nullptr, TRUE);
            return 0;

        case IDM_VIEW_SHOW_TOKEN: DoBuildInspector(); FocusRight(0); return 0;
        case IDM_VIEW_SHOW_AST:   DoBuildInspector(); FocusRight(1); return 0;
        case IDM_VIEW_SHOW_IR:    DoBuildInspector(); FocusRight(2); return 0;

        case IDM_TOOLS_COMMANDPALETTE:
            ShowCommandMenu(hwnd);
            return 0;

        case IDM_TOOLS_SETTINGS:
            MessageBoxW(hwnd,
                L"Ayarlar (final UI):\n"
                L"- Bottom: Console only\n"
                L"- Right: Token/AST/IR\n"
                L"- Find: modern slate popup\n"
                L"- Editor: Dark Carbon & Crimson theme\n"
                L"- Hata: parlak altın-sarı underline (RUN sonrası ilk hata)\n",
                L"Ayarlar",
                MB_OK | MB_ICONINFORMATION);
            return 0;

        case IDM_EXAMPLE_1:
            if (!gRunActive) { SetTextW(g_hCode, DemoCode()); Editor_ClearErrorMarks(); SetStatusTextLeft(L"Örnek 1 Yüklendi"); }
            return 0;
        case IDM_EXAMPLE_2:
            if (!gRunActive) { SetTextW(g_hCode, ExampleCode_Algorithms()); Editor_ClearErrorMarks(); SetStatusTextLeft(L"Örnek 2 Yüklendi"); }
            return 0;
        case IDM_EXAMPLE_3:
            if (!gRunActive) { SetTextW(g_hCode, ExampleCode_StructsAndSwitch()); Editor_ClearErrorMarks(); SetStatusTextLeft(L"Örnek 3 Yüklendi"); }
            return 0;
        case IDM_EXAMPLE_4:
            if (!gRunActive) { SetTextW(g_hCode, ExampleCode_TryCatchAndFiles()); Editor_ClearErrorMarks(); SetStatusTextLeft(L"Örnek 4 Yüklendi"); }
            return 0;

        case IDM_HELP_LANGREF:
            MessageBoxW(hwnd,
                L"=== GariIDE: Ege Ağzı Programlama Dili Sözlüğü ===\n\n"
                L"• basla_gari() { ... } : Ana program başlangıcı (main)\n"
                L"• fonksiyon_gari <tip> <ad>(...) : Fonksiyon tanımlama\n"
                L"• kalip_gari <Ad> { ... } : Yapı / Nesne kalıbı (struct)\n"
                L"• dondur_gari <ifade>; : Değer döndürme (return)\n"
                L"• doktur(<ifade>); : Konsola yazdırma (print)\n\n"
                L"--- Veri Tipleri ---\n"
                L"• tamsayi (int), hassas_sayi (float), cift_hassas_sayi (double)\n"
                L"• laf (string), mantik (bool: hakli / haksiz), bos_gari (void)\n"
                L"• sabit : Değiştirilemez sabit (const)\n"
                L"• dizi_gari <tip> d = [1, 2, 3]; : Dinamik / sabit dizi\n\n"
                L"--- Koşul ve Döngüler ---\n"
                L"• sayet (...) { ... } yoksa { ... } : if / else\n"
                L"• bak_hele (x) { olursa 1: ... kir_gari; hicbiri: ... } : switch / case\n"
                L"• oldukca (...) { ... } : while döngüsü\n"
                L"• bi_yap_hele { ... } oldukca (...); : do-while döngüsü\n"
                L"• cevir_gari (tamsayi i=0; i<10; i++) { ... } : for döngüsü\n"
                L"• gez_gari (<tip> x : dizi_veya_laf) { ... } : foreach döngüsü\n"
                L"• kir_gari; (break) / pas_gec; (continue)\n"
                L"• hem_de (&&), ya_da (||), degil_gari (!)\n\n"
                L"--- Hata Yakalama ---\n"
                L"• dene_gari { ... } yakala_gari (laf hata) { ... } : try / catch\n"
                L"• firlat_gari(\"hata mesaji\"); : throw\n\n"
                L"--- Yerleşik Fonksiyonlar ---\n"
                L"• Girdi: oku_gari(), oku_tamsayi(), oku_hassas_sayi()\n"
                L"• Dizi/Laf: boyut_gari(x), ekle_gari(d, x), cikar_gari(d), sirala_gari(d), ters_cevir_gari(x), icinde_var_mi(x, aranan)\n"
                L"• Laf: buyut_gari(s), kucult_gari(s), kirp_gari(s), parcala_gari(s, bas, uz), degistir_gari(s, eski, yeni)\n"
                L"• Dönüşüm: lafa_cevir(x), tamsayiya_cevir(x), hassasa_cevir(x)\n"
                L"• Dosya/Sistem: dosya_yaz_gari(yol, s), dosya_oku_gari(yol), dosya_var_mi(yol), rastgele_gari(min, max), bekle_gari(ms), vakit_gari()",
                L"Ege Ağzı Dil Sözlüğü",
                MB_OK | MB_ICONINFORMATION);
            return 0;

        case IDM_HELP_ABOUT:
            MessageBoxW(hwnd,
                L"GariIDE (UI Carbon Slate Edition)\n"
                L"- Slate & Solid Crimson VS UI + Core motor\n"
                L"- Command Menu: Ctrl+Shift+P\n"
                L"Bay Stark build ✅",
                L"Hakkında",
                MB_OK | MB_ICONINFORMATION);
            return 0;

        case IDM_RUN_F5:
            gRunUseVM = true;
            SendMessageW(hwnd, WM_COMMAND, IDC_BTN_RUN, 0);
            return 0;
        case IDM_RUN_INTERP:
            gRunUseVM = false;
            SendMessageW(hwnd, WM_COMMAND, IDC_BTN_RUN, 0);
            return 0;
        }

        if (id == IDC_BTN_OPEN) { DoFileOpen(); return 0; }
        if (id == IDC_BTN_SAVE) { DoFileSave(false); return 0; }
        if (id == IDC_BTN_CLR) { DoClearAll(); return 0; }

        if (id == IDC_BTN_MORE) {
            RECT r{}; GetWindowRect(GetDlgItem(hwnd, IDC_BTN_MORE), &r);
            ShowOverflowMenu(hwnd, r.left, r.bottom + 2);
            return 0;
        }

        if (id == IDC_BTN_RUN) {
            if (gRunActive) {
                SetStatusTextLeft(L"RUN aktif (input bekleyebilir)");
                PostMessageW(hwnd, WM_APP_UPDSTATUS, 0, 0);
                return 0;
            }

            Editor_ClearErrorMarks();

            {
                std::lock_guard<std::mutex> lk(gInMx);
                gInQ.clear();
            }

            SetTextW(g_hConsoleOut, L"");
            SetStatusTextLeft(L"RUN (input bekleyebilir)");
            TabCtrl_SetCurSel(g_hTabBottom, 0);
            SetFocus(g_hConsoleIn);

            UI_AppendOutAsync(L"[RUN] Başladı. input gerekiyorsa alttan yaz.\r\n");

            std::wstring src = GetTextW(g_hCode);

            gRunActive = true;
            EnableRunUI(hwnd, false);

            bool useVM = gRunUseVM;

            gRunThread = std::thread([src, useVM]() {
                std::wstring out = useVM ? RunCode_VM(src) : RunCode_Interpreter(src);
                UI_AppendOutAsync(out);

                auto probs = ParseProblemsFromText(out);
                if (!probs.empty()) {
                    auto* em = new ErrMark();
                    em->line = probs[0].line;
                    em->col = probs[0].col;
                    PostMessageW(g_hMain, WM_APP_SETERRMARK, 0, (LPARAM)em);
                }

                PostMessageW(g_hMain, WM_APP_RUNFINISH, 0, 0);
                });
            gRunThread.detach();
            return 0;
        }

        break;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
        PaintBackground(hwnd);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static bool SaveWindowPng(HWND hwnd, const std::wstring& path) {
    RECT rc{};
    GetWindowRect(hwnd, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) return false;

    HDC hdcScreen = GetDC(nullptr);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h; // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP hbm = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!hbm) {
        DeleteDC(hdcMem);
        ReleaseDC(nullptr, hdcScreen);
        return false;
    }
    HGDIOBJ oldBm = SelectObject(hdcMem, hbm);

    // PW_RENDERFULLCONTENT = 2 veya 0 ile pencereyi ve alt kontrollerini çiz
    if (!PrintWindow(hwnd, hdcMem, 2)) {
        PrintWindow(hwnd, hdcMem, 0);
    }

    // 32-bit BMP olarak kaydet
    BITMAPFILEHEADER bfh{};
    DWORD imgSize = (DWORD)(w * h * 4);
    bfh.bfType = 0x4D42; // 'BM'
    bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    bfh.bfSize = bfh.bfOffBits + imgSize;

    // Bottom-up BMP veya pozitif yükseklik yerine BMP okuyucuların tam uyumu için:
    HANDLE hf = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hf != INVALID_HANDLE_VALUE) {
        DWORD wr = 0;
        WriteFile(hf, &bfh, sizeof(bfh), &wr, nullptr);
        WriteFile(hf, &bmi.bmiHeader, sizeof(BITMAPINFOHEADER), &wr, nullptr);
        WriteFile(hf, bits, imgSize, &wr, nullptr);
        CloseHandle(hf);
    }

    SelectObject(hdcMem, oldBm);
    DeleteObject(hbm);
    DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdcScreen);
    return true;
}

// ===================== WinMain =====================
int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR lpCmdLine, int nCmdShow) {
    WNDCLASSW wc{};
    wc.hInstance = hInst;
    wc.lpszClassName = L"GariIDE_Main";
    wc.lpfnWndProc = WndProc;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"GariIDE",
        WS_OVERLAPPEDWINDOW,
        40, 40, 1280, 820,
        nullptr, nullptr, hInst, nullptr);

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // Eğer "--capture <klasor>" argümanı ile başlatıldıysa 4 örneği de çalıştırıp ekran görüntülerini kaydet ve çık
    std::wstring cmd = lpCmdLine ? lpCmdLine : L"";
    if (cmd.find(L"--capture") != std::wstring::npos) {
        std::wstring outDir = L"C:\\Users\\ykoc0\\.gemini\\antigravity\\brain\\7e9542a8-b638-4ac8-bb3a-4f4852db8eb3";
        auto pump = [&](int ms) {
            DWORD t0 = GetTickCount();
            MSG m{};
            while (GetTickCount() - t0 < (DWORD)ms) {
                while (PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE)) {
                    TranslateMessage(&m);
                    DispatchMessageW(&m);
                }
                Sleep(15);
            }
        };
        auto runSyncForShot = [&](int exCmd, int rightTabCmd, const wchar_t* fileName) {
            SendMessageW(hwnd, WM_COMMAND, exCmd, 0);
            pump(150);
            SendMessageW(hwnd, WM_COMMAND, rightTabCmd, 0);
            pump(150);
            std::wstring src = GetTextW(g_hCode);
            std::wstring out = L"[RUN] Başladı. (Bytecode VM Modu - F5)\r\n" + RunCode_VM(src);
            SetTextW(g_hConsoleOut, out);
            SetStatusTextLeft(L"RUN OK");
            UpdateStatusParts();
            Layout(hwnd);
            pump(250);
            RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_UPDATENOW | RDW_ALLCHILDREN);
            pump(250);
            SaveWindowPng(hwnd, outDir + L"\\" + fileName);
        };

        pump(400);
        runSyncForShot(IDM_EXAMPLE_1, IDM_VIEW_SHOW_TOKEN, L"shot1.bmp");
        runSyncForShot(IDM_EXAMPLE_2, IDM_VIEW_SHOW_AST,   L"shot2.bmp");
        runSyncForShot(IDM_EXAMPLE_3, IDM_VIEW_SHOW_IR,    L"shot3.bmp");
        runSyncForShot(IDM_EXAMPLE_4, IDM_VIEW_SHOW_AST,   L"shot4.bmp");
        return 0;
    }

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        if (g_hAccel && TranslateAcceleratorW(hwnd, g_hAccel, &msg)) continue;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}
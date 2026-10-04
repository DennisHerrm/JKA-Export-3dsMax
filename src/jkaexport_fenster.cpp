// ============================================================
//  jkaexport_fenster.cpp - das Exportfenster (Win32, ohne SDK)
//
//  Figur, Modellname, Basisordner (darunter models\players\<name>),
//  Massstab, Drehung, Bodenhoehe, NPC-Datei. Aussehen wie die anderen
//  Plugins (KOTOR/SWBF2 Import): Farben aus Max' Theme.
// ============================================================
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shobjidl.h>
#include <uxtheme.h>

#include "jkaexport_fenster.h"
#include "jkaexport_res.h"
#include "jkaexport_ui.h"

#include <cwchar>

namespace jkaexp {

namespace {
std::wstring Umgebung(const wchar_t* n) {
    const wchar_t* w = _wgetenv(n);
    return w ? std::wstring(w) : std::wstring();
}
std::wstring IniPfad() { return Ablage() + L"\\einstellungen.ini"; }
void LegeOrdnerAn(const std::wstring& p) {
    if (p.empty()) return;
    const DWORD a = GetFileAttributesW(p.c_str());
    if (a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY)) return;
    const size_t s = p.find_last_of(L"\\/");
    if (s != std::wstring::npos && s > 2) LegeOrdnerAn(p.substr(0, s));
    CreateDirectoryW(p.c_str(), nullptr);
}
} // namespace

std::wstring Ablage() {
    const std::wstring lad = Umgebung(L"LOCALAPPDATA");
    return (lad.empty() ? std::wstring(L".") : lad) + L"\\JkaExport";
}

std::wstring Protokollpfad() {
    const std::wstring heim = Umgebung(L"USERPROFILE");
    return (heim.empty() ? std::wstring(L".") : heim) + L"\\Downloads\\Export jka.log";
}

std::wstring IniLies(const wchar_t* abschnitt, const wchar_t* schluessel, const std::wstring& standard) {
    wchar_t puffer[2048] = {};
    GetPrivateProfileStringW(abschnitt, schluessel, standard.c_str(), puffer, 2048, IniPfad().c_str());
    return puffer;
}

void IniSchreib(const wchar_t* abschnitt, const wchar_t* schluessel, const std::wstring& wert) {
    LegeOrdnerAn(Ablage());
    const std::wstring pfad = IniPfad();
    if (GetFileAttributesW(pfad.c_str()) == INVALID_FILE_ATTRIBUTES) {
        HANDLE h = CreateFileW(pfad.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h != INVALID_HANDLE_VALUE) {
            const unsigned char bom[2] = { 0xFF, 0xFE };   // UTF-16: Pfade mit Umlauten
            DWORD n = 0;
            WriteFile(h, bom, 2, &n, nullptr);
            CloseHandle(h);
        }
    }
    WritePrivateProfileStringW(abschnitt, schluessel, wert.c_str(), pfad.c_str());
}

std::wstring Breit(const std::string& s) {
    if (s.empty()) return std::wstring();
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(static_cast<size_t>(n > 0 ? n : 0), L'\0');
    if (n > 0) MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), &w[0], n);
    return w;
}

std::string Utf8(const std::wstring& w) {
    if (w.empty()) return std::string();
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    std::string s(static_cast<size_t>(n > 0 ? n : 0), '\0');
    if (n > 0) WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), &s[0], n, nullptr, nullptr);
    return s;
}

const std::vector<std::pair<std::string, std::string>>& Klassen() {
    // Aus OpenJK code/game/AI_*.cpp: welche KI die Klasse bekommt.
    static const std::vector<std::pair<std::string, std::string>> k = {
        { "CLASS_HOWLER", "small creature: melee attacks, jumps (Howler AI)" },
        { "CLASS_WAMPA", "medium creature: grabs and throws (Wampa AI, uses *l_hand/*r_hand)" },
        { "CLASS_RANCOR", "large creature: grabs, eats, stomps (Rancor AI, uses *l_hand/*r_hand)" },
        { "CLASS_SAND_CREATURE", "burrowing worm (Sand creature AI)" },
        { "CLASS_GONK", "passive: wanders around, no attacks (droid AI)" },
    };
    return k;
}

namespace {

// Vorschlag fuer den Basisordner, solange die INI keinen hat: das base einer
// Steam-Installation von Jedi Academy, sonst leer.
std::wstring StandardBasis() {
    std::vector<std::wstring> k;
    const std::wstring pf86 = Umgebung(L"ProgramFiles(x86)");
    k.push_back((pf86.empty() ? std::wstring(L"C:\\Program Files (x86)") : pf86) +
                L"\\Steam\\steamapps\\common\\Jedi Academy\\GameData\\base");
    for (const std::wstring& o : k) {
        const DWORD a = GetFileAttributesW(o.c_str());
        if (a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY)) return o;
    }
    return std::wstring();
}

struct Zustand {
    const Bruecke* b = nullptr;
    HWND dlg = nullptr;
    jkaui::Palette pal;
    HFONT font = nullptr;
    std::vector<Figur> figuren;
    bool drehen = false, boden = true, npc = true, anOrt = true;
    int exportiert = 0;
};

std::wstring Text(HWND d, int id) {
    wchar_t p[1024] = {};
    GetDlgItemTextW(d, id, p, 1024);
    return p;
}

void ZeigeZiel(Zustand& z) {
    const std::wstring ziel = Text(z.dlg, IDC_BASIS) + L"\\models\\players\\" + Text(z.dlg, IDC_NAME);
    SetDlgItemTextW(z.dlg, IDC_ZIEL, (L"\u2192 " + ziel).c_str());
}

void ZeigeKlasse(Zustand& z) {
    const int i = static_cast<int>(SendDlgItemMessageW(z.dlg, IDC_KLASSE, CB_GETCURSEL, 0, 0));
    if (i >= 0 && static_cast<size_t>(i) < Klassen().size()) SetDlgItemTextW(z.dlg, IDC_KLASSE_INFO, Breit(Klassen()[static_cast<size_t>(i)].second).c_str());
    EnableWindow(GetDlgItem(z.dlg, IDC_KLASSE), z.npc);
    EnableWindow(GetDlgItem(z.dlg, IDC_GESUNDHEIT), z.npc);
}

std::string Vorschlag(const std::string& n) {
    std::string s;
    for (char c : n) {
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_') s += c;
        else if (c >= 'A' && c <= 'Z') s += static_cast<char>(c - 'A' + 'a');
        else if (c == ' ' || c == '-') s += '_';
    }
    if (s.size() > 24) s.resize(24);
    return s.empty() ? std::string("model") : s;
}

void FigurGewaehlt(Zustand& z, bool nameSetzen) {
    const int i = static_cast<int>(SendDlgItemMessageW(z.dlg, IDC_FIGUR, CB_GETCURSEL, 0, 0));
    if (i < 0 || static_cast<size_t>(i) >= z.figuren.size()) return;
    const Figur& f = z.figuren[static_cast<size_t>(i)];
    z.drehen = f.kotor;   // KOTOR-Figuren schauen nach +Y, JKA nach -Y
    if (nameSetzen) SetDlgItemTextW(z.dlg, IDC_NAME, Breit(Vorschlag(f.name)).c_str());
    InvalidateRect(GetDlgItem(z.dlg, IDC_DREHEN), nullptr, FALSE);
    ZeigeZiel(z);
}

void LadeFiguren(Zustand& z) {
    z.figuren = z.b->figuren ? z.b->figuren() : std::vector<Figur>();
    HWND c = GetDlgItem(z.dlg, IDC_FIGUR);
    SendMessageW(c, CB_RESETCONTENT, 0, 0);
    for (const Figur& f : z.figuren) {
        const std::wstring t = Breit(f.name) + L"   (" + std::to_wstring(f.netze) + L" meshes, " + std::to_wstring(f.knochen) + L" nodes" +
                               (f.kotor ? L", KOTOR" : L"") + L")";
        SendMessageW(c, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(t.c_str()));
    }
    if (!z.figuren.empty()) SendMessageW(c, CB_SETCURSEL, 0, 0);
    if (z.figuren.empty()) SetDlgItemTextW(z.dlg, IDC_STATUS, L"No character in the scene: needs a hierarchy with skinned meshes.");
    FigurGewaehlt(z, z.b->startName.empty());
}

std::wstring WaehleOrdner(HWND eltern) {
    std::wstring aus;
    IFileOpenDialog* d = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&d)))) return aus;
    DWORD opt = 0;
    d->GetOptions(&opt);
    d->SetOptions(opt | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
    d->SetTitle(L"JKA base folder (the one that contains models\\players)");
    if (SUCCEEDED(d->Show(eltern))) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(d->GetResult(&item))) {
            PWSTR p = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &p))) { aus = p; CoTaskMemFree(p); }
            item->Release();
        }
    }
    d->Release();
    return aus;
}

void Exportiere(Zustand& z) {
    const int i = static_cast<int>(SendDlgItemMessageW(z.dlg, IDC_FIGUR, CB_GETCURSEL, 0, 0));
    if (i < 0 || static_cast<size_t>(i) >= z.figuren.size()) { SetDlgItemTextW(z.dlg, IDC_STATUS, L"Pick a character first."); return; }
    Optionen o;
    o.wurzel = z.figuren[static_cast<size_t>(i)].handle;
    o.name = Vorschlag(Utf8(Text(z.dlg, IDC_NAME)));
    o.basis = Text(z.dlg, IDC_BASIS);
    while (!o.basis.empty() && (o.basis.back() == L'\\' || o.basis.back() == L'/')) o.basis.pop_back();
    o.skala = static_cast<float>(_wtof(Text(z.dlg, IDC_SKALA).c_str()));
    if (!(o.skala > 0.0f)) o.skala = 1.0f;
    o.drehen = z.drehen;
    o.boden = z.boden;
    o.anOrt = z.anOrt;
    o.npc = z.npc;
    const int k = static_cast<int>(SendDlgItemMessageW(z.dlg, IDC_KLASSE, CB_GETCURSEL, 0, 0));
    if (k >= 0 && static_cast<size_t>(k) < Klassen().size()) o.klasse = Klassen()[static_cast<size_t>(k)].first;
    o.gesundheit = _wtoi(Text(z.dlg, IDC_GESUNDHEIT).c_str());
    if (o.gesundheit <= 0) o.gesundheit = 200;
    if (o.basis.empty()) { SetDlgItemTextW(z.dlg, IDC_STATUS, L"Set the base folder (the one that contains models\\players)."); return; }
    IniSchreib(L"Export", L"Basis", o.basis);
    IniSchreib(L"Export", L"Skala", Text(z.dlg, IDC_SKALA));
    IniSchreib(L"Export", L"Boden", o.boden ? L"1" : L"0");
    IniSchreib(L"Export", L"AnOrt", o.anOrt ? L"1" : L"0");
    IniSchreib(L"Export", L"Npc", o.npc ? L"1" : L"0");
    IniSchreib(L"Export", L"Klasse", Breit(o.klasse));
    SetDlgItemTextW(z.dlg, IDC_NAME, Breit(o.name).c_str());
    SetDlgItemTextW(z.dlg, IDC_STATUS, L"Exporting ...");
    UpdateWindow(z.dlg);
    HCURSOR alt = SetCursor(LoadCursor(nullptr, IDC_WAIT));
    std::wstring bericht;
    const bool ok = z.b->exportiere && z.b->exportiere(o, bericht);
    SetCursor(alt);
    SetDlgItemTextW(z.dlg, IDC_STATUS, bericht.c_str());
    if (ok) ++z.exportiert;
}

INT_PTR CALLBACK Proc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    Zustand* z = reinterpret_cast<Zustand*>(GetWindowLongPtrW(h, DWLP_USER));
    switch (msg) {
    case WM_INITDIALOG: {
        z = reinterpret_cast<Zustand*>(lp);
        SetWindowLongPtrW(h, DWLP_USER, lp);
        z->dlg = h;
        z->pal.Baue(z->b->farbe);
        jkaui::DunkleTitelleiste(h, z->pal.dunkel);
        z->font = reinterpret_cast<HFONT>(SendMessageW(h, WM_GETFONT, 0, 0));
        if (z->pal.dunkel) {
            SetWindowTheme(GetDlgItem(h, IDC_FIGUR), L"DarkMode_CFD", nullptr);
            SetWindowTheme(GetDlgItem(h, IDC_KLASSE), L"DarkMode_CFD", nullptr);
        }
        SetWindowTextW(h, (L"JKA Export " + z->b->version).c_str());
        SetDlgItemTextW(h, IDC_BASIS, (z->b->startBasis.empty() ? IniLies(L"Export", L"Basis", StandardBasis()) : z->b->startBasis).c_str());
        SetDlgItemTextW(h, IDC_SKALA, IniLies(L"Export", L"Skala", L"1.0").c_str());
        if (!z->b->startName.empty()) SetDlgItemTextW(h, IDC_NAME, Breit(z->b->startName).c_str());
        z->boden = IniLies(L"Export", L"Boden", L"1") != L"0";
        z->anOrt = IniLies(L"Export", L"AnOrt", L"1") != L"0";
        z->npc = IniLies(L"Export", L"Npc", L"1") != L"0";
        SetDlgItemTextW(h, IDC_GESUNDHEIT, L"200");
        const std::wstring klasse = IniLies(L"Export", L"Klasse", L"CLASS_HOWLER");
        for (size_t i = 0; i < Klassen().size(); ++i) {
            SendDlgItemMessageW(h, IDC_KLASSE, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Breit(Klassen()[i].first).c_str()));
            if (Breit(Klassen()[i].first) == klasse) SendDlgItemMessageW(h, IDC_KLASSE, CB_SETCURSEL, i, 0);
        }
        if (SendDlgItemMessageW(h, IDC_KLASSE, CB_GETCURSEL, 0, 0) < 0) SendDlgItemMessageW(h, IDC_KLASSE, CB_SETCURSEL, 0, 0);
        SetDlgItemTextW(h, IDC_STATUS,
                        L"Animations come from the note track \"jka\" on the scene root (start and end key per sequence, "
                        L"e.g. from KOTOR Animations > Select JKA set > Selected to timeline), otherwise from \"animations\". "
                        L"Footsteps come from the note track \"events\".");
        LadeFiguren(*z);
        ZeigeKlasse(*z);
        return TRUE;
    }
    case WM_DRAWITEM: {
        if (z == nullptr) break;
        const DRAWITEMSTRUCT* d = reinterpret_cast<const DRAWITEMSTRUCT*>(lp);
        bool betont = d->CtlID == IDOK;
        if (d->CtlID == IDC_DREHEN) betont = z->drehen;
        if (d->CtlID == IDC_BODEN) betont = z->boden;
        if (d->CtlID == IDC_ANORT) betont = z->anOrt;
        if (d->CtlID == IDC_NPC) betont = z->npc;
        jkaui::ZeichneKnopf(z->pal, z->font, *d, betont);
        return TRUE;
    }
    case WM_CTLCOLORDLG:
        return z ? reinterpret_cast<INT_PTR>(z->pal.pinselGrund) : FALSE;
    case WM_CTLCOLORSTATIC:
        if (z == nullptr) break;
        SetBkColor(reinterpret_cast<HDC>(wp), z->pal.grund);
        {
            const int id = GetDlgCtrlID(reinterpret_cast<HWND>(lp));
            SetTextColor(reinterpret_cast<HDC>(wp), (id == IDC_ZIEL || id == IDC_KLASSE_INFO || id == IDC_STATUS) ? z->pal.dim : z->pal.text);
        }
        return reinterpret_cast<INT_PTR>(z->pal.pinselGrund);
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
        if (z == nullptr) break;
        SetBkColor(reinterpret_cast<HDC>(wp), z->pal.feld);
        SetTextColor(reinterpret_cast<HDC>(wp), z->pal.feldText);
        return reinterpret_cast<INT_PTR>(z->pal.pinselFeld);
    case WM_PAINT: {
        if (z == nullptr) break;
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        for (int id : { IDC_NAME, IDC_BASIS, IDC_SKALA, IDC_GESUNDHEIT }) jkaui::Kante(h, dc, id, z->pal.linie);
        EndPaint(h, &ps);
        return TRUE;
    }
    case WM_COMMAND: {
        if (z == nullptr) break;
        const int id = LOWORD(wp), code = HIWORD(wp);
        switch (id) {
        case IDC_FIGUR:
            if (code == CBN_SELCHANGE) FigurGewaehlt(*z, true);
            return TRUE;
        case IDC_KLASSE:
            if (code == CBN_SELCHANGE) ZeigeKlasse(*z);
            return TRUE;
        case IDC_NAME:
        case IDC_BASIS:
            if (code == EN_CHANGE) ZeigeZiel(*z);
            return TRUE;
        case IDC_AKTUALISIEREN:
            LadeFiguren(*z);
            return TRUE;
        case IDC_DURCHSUCHEN: {
            std::wstring o = WaehleOrdner(h);
            if (o.empty()) return TRUE;
            // models\players oder tiefer gewaehlt? Auf den Basisordner kuerzen.
            std::wstring k = o;
            for (wchar_t& c : k) c = static_cast<wchar_t>(towlower(c));
            const size_t p = k.find(L"\\models\\players");
            if (p != std::wstring::npos) o = o.substr(0, p);
            SetDlgItemTextW(h, IDC_BASIS, o.c_str());
            return TRUE;
        }
        case IDC_DREHEN: z->drehen = !z->drehen; InvalidateRect(GetDlgItem(h, id), nullptr, FALSE); return TRUE;
        case IDC_BODEN: z->boden = !z->boden; InvalidateRect(GetDlgItem(h, id), nullptr, FALSE); return TRUE;
        case IDC_ANORT: z->anOrt = !z->anOrt; InvalidateRect(GetDlgItem(h, id), nullptr, FALSE); return TRUE;
        case IDC_NPC: z->npc = !z->npc; InvalidateRect(GetDlgItem(h, id), nullptr, FALSE); ZeigeKlasse(*z); return TRUE;
        case IDOK: Exportiere(*z); return TRUE;
        case IDCANCEL: EndDialog(h, z->exportiert > 0 ? 1 : 0); return TRUE;
        default: break;
        }
        break;
    }
    case WM_DESTROY:
        if (z) z->pal.Frei();
        break;
    default:
        break;
    }
    return FALSE;
}

} // namespace

int ZeigeExportFenster(const Bruecke& b) {
    Zustand z;
    z.b = &b;
    const HRESULT co = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const INT_PTR r = DialogBoxParamW(static_cast<HINSTANCE>(b.hInstance), MAKEINTRESOURCEW(IDD_EXPORT), static_cast<HWND>(b.eltern), Proc,
                                      reinterpret_cast<LPARAM>(&z));
    if (SUCCEEDED(co)) CoUninitialize();
    if (r == -1) return -1;
    return z.exportiert > 0 ? 1 : 0;
}

} // namespace jkaexp

// ============================================================
//  JKA Export - DLL-Einstieg (Exporte ueber jkaexport.def)
// ============================================================
#include "jkaexport.h"

#include <iFnPub.h>

HINSTANCE hInstance = nullptr;

BOOL WINAPI DllMain(HINSTANCE h, ULONG grund, LPVOID) {
    if (grund == DLL_PROCESS_ATTACH) {
        hInstance = h;
        DisableThreadLibraryCalls(h);
    }
    return TRUE;
}

namespace {
std::string U8(const MCHAR* s) { return s ? jkaexp::Utf8(s) : std::string(); }
}

// ------------------------------------------------------------
//  Datei -> Exportieren -> "Jedi Academy Ghoul2 (*.glm)"
//  Liegt der gewaehlte Pfad unter ...\models\players\<name>\, werden Basis
//  und Name daraus genommen, sonst Ordner und Dateiname. Dann das Fenster.
// ------------------------------------------------------------
class JkaSceneExport : public SceneExport {
public:
    int ExtCount() override { return 1; }
    const MCHAR* Ext(int) override { return _T("glm"); }
    const MCHAR* LongDesc() override { return _T("Star Wars Jedi Academy Ghoul2 model (GLM + GLA + animation.cfg)"); }
    const MCHAR* ShortDesc() override { return _T("Jedi Academy Ghoul2"); }
    const MCHAR* AuthorName() override { return _T("DennisH"); }
    const MCHAR* CopyrightMessage() override { return _T(""); }
    const MCHAR* OtherMessage1() override { return _T(""); }
    const MCHAR* OtherMessage2() override { return _T(""); }
    unsigned int Version() override { return JKAEXPORT_VERSION; }
    void ShowAbout(HWND h) override {
        MessageBox(h, _T("JKA Export ") JKAEXPORT_VERSION_STR _T("\n\nGhoul2 models (GLM/GLA) for Jedi Academy straight from 3ds Max.\n")
                      _T("Writers from g2c. Not affiliated with Raven, Activision, Disney or Autodesk."),
                   _T("JKA Export"), MB_ICONINFORMATION);
    }
    BOOL SupportsOptions(int, DWORD) override { return FALSE; }
    int DoExport(const MCHAR* name, ExpInterface*, Interface*, BOOL, DWORD) override {
        std::wstring p = name ? name : L"";
        std::wstring k = p;
        for (wchar_t& c : k) c = static_cast<wchar_t>(towlower(c));
        std::wstring basis;
        std::string modell;
        const size_t m = k.find(L"\\models\\players\\");
        if (m != std::wstring::npos) {
            basis = p.substr(0, m);
            const size_t a = m + 16;
            const size_t e = p.find(L'\\', a);
            modell = jkaexp::Utf8(p.substr(a, e == std::wstring::npos ? std::wstring::npos : e - a));
        } else {
            const size_t sl = p.find_last_of(L"\\/");
            basis = sl == std::wstring::npos ? std::wstring() : p.substr(0, sl);
            const size_t pk = p.find_last_of(L'.');
            modell = jkaexp::Utf8(p.substr(sl + 1, (pk == std::wstring::npos || pk < sl) ? std::wstring::npos : pk - sl - 1));
        }
        return jkaexp::OeffneFenster(modell, basis) >= 0 ? IMPEXP_SUCCESS : IMPEXP_FAIL;
    }
};

class JkaSceneExportClassDesc : public ClassDesc2 {
public:
    int IsPublic() override { return TRUE; }
    void* Create(BOOL) override { return new JkaSceneExport(); }
    const MCHAR* ClassName() override { return _T("JKA Export"); }
#if defined(MAX_RELEASE) && (MAX_RELEASE >= 24000)
    const MCHAR* NonLocalizedClassName() override { return _T("JKA Export"); }
#endif
    SClass_ID SuperClassID() override { return SCENE_EXPORT_CLASS_ID; }
    Class_ID ClassID() override { return JKAEXPORT_CLASS_ID; }
    const MCHAR* Category() override { return _T("Export"); }
    const MCHAR* InternalName() override { return _T("JkaSceneExport"); }
    HINSTANCE HInstance() override { return hInstance; }
};

static JkaSceneExportClassDesc theClassDesc;

// ------------------------------------------------------------
//  MAXScript "JkaExportCpp"
//      JkaExportCpp.showDialog()
//      JkaExportCpp.version()
//      JkaExportCpp.exportModel <knoten> <name> <basis> <skala> <drehen: -1 auto, 0, 1> <npcKlasse oder "">
//      JkaExportCpp.lastReport()
// ------------------------------------------------------------
class JkaExportFP : public FPStaticInterface {
public:
    enum { fn_showDialog, fn_version, fn_exportModel, fn_lastReport };
    BOOL showDialog() { return jkaexp::OeffneFenster() > 0 ? TRUE : FALSE; }
    const MCHAR* version() { return JKAEXPORT_VERSION_STR; }
    BOOL exportModel(const MCHAR* knoten, const MCHAR* name, const MCHAR* basis, float skala, int drehen, const MCHAR* klasse) {
        return jkaexp::SkriptExport(knoten ? knoten : L"", U8(name), basis ? basis : L"", skala, drehen, U8(klasse)) ? TRUE : FALSE;
    }
    const MCHAR* lastReport() { return jkaexp::LetzterBericht().c_str(); }

    DECLARE_DESCRIPTOR(JkaExportFP)
    BEGIN_FUNCTION_MAP
        FN_0(fn_showDialog, TYPE_BOOL, showDialog)
        FN_0(fn_version, TYPE_STRING, version)
        FN_6(fn_exportModel, TYPE_BOOL, exportModel, TYPE_STRING, TYPE_STRING, TYPE_STRING, TYPE_FLOAT, TYPE_INT, TYPE_STRING)
        FN_0(fn_lastReport, TYPE_STRING, lastReport)
    END_FUNCTION_MAP
};

static JkaExportFP theFP(
    JKAEXPORT_FP_ID, _T("JkaExportCpp"), 0, &theClassDesc, FP_CORE,
    JkaExportFP::fn_showDialog, _T("showDialog"), 0, TYPE_BOOL, 0, 0,
    JkaExportFP::fn_version, _T("version"), 0, TYPE_STRING, 0, 0,
    JkaExportFP::fn_exportModel, _T("exportModel"), 0, TYPE_BOOL, 0, 6,
        _T("node"), 0, TYPE_STRING,
        _T("name"), 0, TYPE_STRING,
        _T("baseFolder"), 0, TYPE_STRING,
        _T("scale"), 0, TYPE_FLOAT,
        _T("turn"), 0, TYPE_INT,
        _T("npcClass"), 0, TYPE_STRING,
    JkaExportFP::fn_lastReport, _T("lastReport"), 0, TYPE_STRING, 0, 0,
    p_end);

__declspec(dllexport) const TCHAR* LibDescription() { return _T("JKA Export ") JKAEXPORT_VERSION_STR _T(" - Ghoul2 GLM/GLA exporter"); }
__declspec(dllexport) int LibNumberClasses() { return 1; }
__declspec(dllexport) ClassDesc* LibClassDesc(int i) { return i == 0 ? &theClassDesc : nullptr; }
__declspec(dllexport) ULONG LibVersion() { return VERSION_3DSMAX; }
__declspec(dllexport) int LibInitialize() {
    // Wie beim SWBF2/KOTOR Import: Kerninterface nachmelden, falls Max es beim Laden nicht tat.
    if (GetCOREInterface(JKAEXPORT_FP_ID) == nullptr) RegisterCOREInterface(&theFP);
    return TRUE;
}
__declspec(dllexport) int CanAutoDefer() { return FALSE; }

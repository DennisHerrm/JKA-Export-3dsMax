// ============================================================
//  JKA Export - die Max-Schicht: Szene lesen, Auftrag fuellen.
//
//  KNOCHEN: alle Knoten der gewaehlten Hierarchie, die kein GEHAEUTETES
//  Netz sind (Bones, Helfer, Dummies, starre Netze), dazu alle Bones, die
//  ein Skin benutzt, samt Vorfahren. Ganz oben ein eigener "model_root"
//  (Ravens Modelle haben ihn alle; in jedem Frame steht er bei z = -24).
//
//  BINDEPOSE: fuer Skin-Bones ISkin::GetBoneInitTM (exakt die Bindung),
//  fuer alle anderen die Lage bei Bild 0 relativ zum Elternknochen.
//
//  NETZE: gehaeutet -> Skin kurz abgeschaltet, Netz vor dem Skin auswerten,
//  Bindelage aus ISkin::GetSkinInitTM, Gewichte aus ISkinContextData.
//  Starr -> zu 100 % an den eigenen Knochen.
//
//  RAUM: Max (Zeilenvektoren) -> Spiel (Spaltenvektoren), Massstab, wahlweise
//  180 Grad um Z (KOTOR schaut nach +Y, JKA nach -Y), Frames um -24 gesenkt.
//
//  SEQUENZEN: Notizspur "jka" (sonst "animations") an der Szenenwurzel, je
//  Sequenz ein Key am Anfang und einer am Ende mit dem Namen. Schritte aus
//  der Spur "events" (Text mit "footstep").
// ============================================================
#include "jkaexport.h"
#include "g2export.h"

#include <icolorman.h>
#include <iskin.h>
#include <modstack.h>
#include <MeshNormalSpec.h>
#include <stdmat.h>
#include <maxscript/maxscript.h>

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <functional>
#include <tuple>
#include <map>
#include <set>
#include <unordered_map>

#ifndef BONE_OBJ_CLASSID
#define BONE_OBJ_CLASSID Class_ID(BONE_OBJ_CLASS_ID, 0)
#endif

extern HINSTANCE hInstance;

namespace jkaexp {

namespace {

// ---- Protokoll: Downloads\Export jka.log ----
std::FILE* g_log = nullptr;
std::wstring g_bericht;

void Log(const char* format, ...) {
    if (g_log == nullptr) return;
    char p[2048];
    va_list a;
    va_start(a, format);
    std::vsnprintf(p, sizeof p, format, a);
    va_end(a);
    std::fprintf(g_log, "%s\n", p);
    std::fflush(g_log);
}

struct LogBlock {
    LogBlock() { if (g_log == nullptr) g_log = _wfopen(Protokollpfad().c_str(), L"ab"); }
    ~LogBlock() { if (g_log) { std::fclose(g_log); g_log = nullptr; } }
};

std::string N(INode* n) { return (n && n->GetName()) ? Utf8(n->GetName()) : std::string(); }

std::string Klein(std::string s) {
    for (char& c : s) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return s;
}

#if defined(MAX_RELEASE) && (MAX_RELEASE >= 24000)
bool FuehreSkript(const std::wstring& s) { return ExecuteMAXScriptScript(s.c_str(), MAXScript::ScriptSource::NonEmbedded, TRUE) != FALSE; }
#else
bool FuehreSkript(const std::wstring& s) { return ExecuteMAXScriptScript(s.c_str(), TRUE) != FALSE; }
#endif

std::string UserProp(INode* n, const MCHAR* k) {
    MSTR w;
    if (n == nullptr || !n->GetUserPropString(MSTR(k), w) || w.data() == nullptr) return std::string();
    return Utf8(w.data());
}

// ---- Klassifizierung ----
Modifier* FindeSkin(INode* n) {
    Object* o = n ? n->GetObjectRef() : nullptr;
    while (o != nullptr && o->SuperClassID() == GEN_DERIVOB_CLASS_ID) {
        IDerivedObject* d = static_cast<IDerivedObject*>(o);
        for (int i = 0; i < d->NumModifiers(); ++i) {
            Modifier* m = d->GetModifier(i);
            if (m != nullptr && m->ClassID() == SKIN_CLASSID) return m;
        }
        o = d->GetObjRef();
    }
    return nullptr;
}

bool IstGeometrie(INode* n) {
    if (n == nullptr || !n->Renderable()) return false;
    ObjectState os = n->EvalWorldState(0);
    if (os.obj == nullptr || os.obj->SuperClassID() != GEOMOBJECT_CLASS_ID) return false;
    if (os.obj->ClassID() == BONE_OBJ_CLASSID) return false;
    return os.obj->CanConvertToType(triObjectClassID) != 0;
}

void Sammle(INode* n, std::vector<INode*>& aus) {
    if (n == nullptr) return;
    aus.push_back(n);
    for (int i = 0; i < n->NumberOfChildren(); ++i) Sammle(n->GetChildNode(i), aus);
}

std::vector<Figur> Figuren() {
    std::vector<Figur> aus;
    Interface* ip = GetCOREInterface();
    INode* w = ip->GetRootNode();
    INode* gewaehlt = ip->GetSelNodeCount() > 0 ? ip->GetSelNode(0) : nullptr;
    while (gewaehlt && gewaehlt->GetParentNode() && !gewaehlt->GetParentNode()->IsRootNode()) gewaehlt = gewaehlt->GetParentNode();
    for (int i = 0; i < w->NumberOfChildren(); ++i) {
        INode* top = w->GetChildNode(i);
        std::vector<INode*> alle;
        Sammle(top, alle);
        Figur f;
        f.handle = top->GetHandle();
        f.name = N(top);
        // KOTOR-Rigs ab KOTOR Import 0.4.0 schauen schon nach -Y (kotor_blick).
        f.kotor = !UserProp(top, _T("kotor_rig")).empty() && UserProp(top, _T("kotor_blick")) != "-Y";
        f.knochen = static_cast<int>(alle.size());
        for (INode* n : alle) if (FindeSkin(n) != nullptr || IstGeometrie(n)) ++f.netze;
        if (f.netze == 0) continue;
        if (top == gewaehlt) aus.insert(aus.begin(), f);
        else aus.push_back(f);
    }
    return aus;
}

// ---- Raum ----
struct Raum {
    float s = 1.0f;
    bool drehen = false;
    float boden = 0.0f;           // Frames: -24
};

g2export::Mat Spiel(const Matrix3& m, const Raum& r, bool frame) {
    g2export::Mat out;
    for (int j = 0; j < 3; ++j) {
        Point3 achse = m.GetRow(j);                      // Spalte j der Drehung
        const float l = Length(achse);
        if (l > 1e-8f) achse /= l;
        out.m[0][j] = r.drehen ? -achse.x : achse.x;
        out.m[1][j] = r.drehen ? -achse.y : achse.y;
        out.m[2][j] = achse.z;
    }
    const Point3 t = m.GetRow(3);
    out.m[0][3] = r.s * (r.drehen ? -t.x : t.x);
    out.m[1][3] = r.s * (r.drehen ? -t.y : t.y);
    out.m[2][3] = r.s * t.z + (frame ? r.boden : 0.0f);
    return out;
}

Point3 SpielPunkt(const Point3& p, const Raum& r) {
    return Point3(r.s * (r.drehen ? -p.x : p.x), r.s * (r.drehen ? -p.y : p.y), r.s * p.z);
}

Point3 SpielRichtung(const Point3& v, const Raum& r) {
    Point3 d(r.drehen ? -v.x : v.x, r.drehen ? -v.y : v.y, v.z);
    const float l = Length(d);
    return l > 1e-8f ? d / l : Point3(0, 0, 1);
}

// ---- Notizspuren ueber MAXScript lesen: "spur<TAB>bild<TAB>text" ----
struct NotizKey { std::string spur; int bild; std::string text; };

std::vector<NotizKey> LiesNotizen() {
    std::vector<NotizKey> aus;
    const std::wstring datei = Ablage() + L"\\notizen.txt";
    CreateDirectoryW(Ablage().c_str(), nullptr);
    _wremove(datei.c_str());
    std::wstring pfad;
    for (wchar_t c : datei) { if (c == L'\\') pfad += L"\\\\"; else pfad += c; }
    FuehreSkript(L"(local f = createFile \"" + pfad + L"\"\n"
                 L" for i = 1 to numNoteTracks rootNode do (local t = getNoteTrack rootNode i\n"
                 L"  for k in t.keys do format \"%\\t%\\t%\\n\" t.name (k.time.frame as integer) k.value to:f)\n"
                 L" close f\n OK)\n");
    std::FILE* f = _wfopen(datei.c_str(), L"rb");
    if (f == nullptr) return aus;
    char zeile[1024];
    while (std::fgets(zeile, sizeof zeile, f)) {
        std::string z = zeile;
        while (!z.empty() && (z.back() == '\n' || z.back() == '\r')) z.pop_back();
        const size_t a = z.find('\t'), b = (a == std::string::npos) ? a : z.find('\t', a + 1);
        if (b == std::string::npos) continue;
        aus.push_back({ z.substr(0, a), std::atoi(z.substr(a + 1, b - a - 1).c_str()), z.substr(b + 1) });
    }
    std::fclose(f);
    return aus;
}

struct Abschnitt { std::string name, kommentar; int von, bis; };

std::vector<Abschnitt> Abschnitte(const std::vector<NotizKey>& keys, const std::string& spur) {
    std::vector<NotizKey> k;
    for (const NotizKey& x : keys) if (x.spur == spur) k.push_back(x);
    std::sort(k.begin(), k.end(), [](const NotizKey& a, const NotizKey& b) { return a.bild < b.bild; });
    std::vector<Abschnitt> aus;
    for (size_t i = 0; i < k.size(); ++i) {
        if (i + 1 < k.size() && k[i + 1].text == k[i].text) {
            aus.push_back({ k[i].text, std::string(), k[i].bild, k[i + 1].bild });
            ++i;
        } else {
            const int bis = (i + 1 < k.size()) ? k[i + 1].bild - 1 : k[i].bild;
            aus.push_back({ k[i].text, std::string(), k[i].bild, std::max(bis, k[i].bild) });
        }
    }
    return aus;
}

bool IstSchleife(const std::string& name) {
    static const char* const s[] = { "BOTH_STAND1", "BOTH_STAND2", "BOTH_STAND6", "BOTH_GUARD_IDLE1", "BOTH_WALK1", "BOTH_WALK2",
                                     "BOTH_WALK5", "BOTH_RUN1", "BOTH_RUN2", "BOTH_WALKBACK1", "BOTH_WALKBACK2", "BOTH_RUNBACK1",
                                     "BOTH_RUNBACK2", "BOTH_SLEEP1", "BOTH_CHOKE1", "BOTH_VT_IDLE", "BOTH_VT_WALK_FWD", "BOTH_VT_RUN_FWD",
                                     "BOTH_SWIM_IDLE1", "BOTH_HOLD_IDLE" };
    for (const char* x : s) if (name == x) return true;
    return Klein(name).find("loop") != std::string::npos;
}

// ---- Textur eines Materials ----
std::wstring TexturVon(Mtl* m) {
    if (m == nullptr) return std::wstring();
    std::wstring erste;
    for (int i = 0; i < m->NumSubTexmaps(); ++i) {
        Texmap* t = m->GetSubTexmap(i);
        if (t == nullptr || t->ClassID() != Class_ID(BMTEX_CLASS_ID, 0)) continue;
        const MCHAR* datei = static_cast<BitmapTex*>(t)->GetMapName();
        if (datei == nullptr || datei[0] == 0) continue;
#if defined(MAX_RELEASE) && (MAX_RELEASE >= 24000)
        const std::string slot = Klein(Utf8(m->GetSubTexmapSlotName(i, false).data()));
#else
        const std::string slot = Klein(Utf8(m->GetSubTexmapSlotName(i).data()));
#endif
        if (slot.find("diffuse") != std::string::npos || slot.find("base") != std::string::npos) return datei;
        if (erste.empty()) erste = datei;
    }
    return erste;
}

// ---- Ein Netz einsammeln ----
struct NetzQuelle {
    INode* node = nullptr;
    Modifier* skin = nullptr;     // nullptr = starr
};

// korrektur: je GLA-Knochen die Umrechnung von der Bindung DIESES Skins auf die
// eine Bindepose der GLA (leer = gleich). Noetig, wenn ein Bone in zwei Skins
// verschieden gebunden ist - die GLA kennt je Bone nur eine Bindepose.
bool LiesNetz(Interface* ip, const NetzQuelle& q, const Matrix3& objZuWelt, const std::map<INode*, int>& knochenIndex, int eigenerKnochen,
              const std::map<int, Matrix3>& korrektur, const Raum& raum, std::map<Mtl*, g2export::Flaeche>& proMaterial,
              std::vector<Point3>& alleSpielpunkte, std::string& fehler) {
    const TimeValue t0 = 0;
    if (q.skin != nullptr) q.skin->DisableMod();
    ObjectState os = q.node->EvalWorldState(t0);
    TriObject* tri = (os.obj != nullptr && os.obj->CanConvertToType(triObjectClassID))
                         ? static_cast<TriObject*>(os.obj->ConvertToType(t0, triObjectClassID)) : nullptr;
    if (tri == nullptr) {
        if (q.skin) q.skin->EnableMod();
        fehler = "no mesh";
        return false;
    }
    Mesh mesh = tri->GetMesh();                 // Kopie: das Objekt darf danach weg
    if (tri != os.obj) tri->MaybeAutoDelete();
    if (q.skin != nullptr) { q.skin->EnableMod(); q.node->EvalWorldState(t0); }

    const int nv = mesh.getNumVerts(), nf = mesh.getNumFaces();
    // Gewichte je Geometrievertex
    std::vector<std::vector<std::pair<int, float>>> gew(static_cast<size_t>(nv));
    if (q.skin != nullptr) {
        ISkin* is = static_cast<ISkin*>(q.skin->GetInterface(I_SKIN));
        ISkinContextData* ctx = is ? is->GetContextInterface(q.node) : nullptr;
        if (ctx == nullptr || ctx->GetNumPoints() != nv) { fehler = "skin data does not match the mesh"; return false; }
        for (int v = 0; v < nv; ++v) {
            for (int j = 0; j < ctx->GetNumAssignedBones(v); ++j) {
                INode* bn = is->GetBone(ctx->GetAssignedBone(v, j));
                const auto it = knochenIndex.find(bn);
                const float w = ctx->GetBoneWeight(v, j);
                if (it != knochenIndex.end() && w > 0.0f) gew[static_cast<size_t>(v)].push_back({ it->second, w });
            }
            auto& g = gew[static_cast<size_t>(v)];
            std::sort(g.begin(), g.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
            if (g.size() > 4) g.resize(4);
            float s = 0;
            for (auto& x : g) s += x.second;
            if (s > 0) for (auto& x : g) x.second /= s;
        }
    } else {
        for (auto& g : gew) g.push_back({ eigenerKnochen, 1.0f });
    }

    // Normalen: festgelegte (Edit Normals) oder aus den Glaettungsgruppen.
    mesh.buildNormals();
    MeshNormalSpec* spec = mesh.GetSpecifiedNormals();
    const bool mitSpec = spec != nullptr && spec->GetNumFaces() == nf && spec->GetNumNormals() > 0;
    std::vector<Point3> flaechenNormale(static_cast<size_t>(nf));
    std::vector<std::vector<int>> flaechenAnVertex(static_cast<size_t>(nv));
    for (int f = 0; f < nf; ++f) {
        const Point3 a = mesh.verts[mesh.faces[f].v[0]], b = mesh.verts[mesh.faces[f].v[1]], c = mesh.verts[mesh.faces[f].v[2]];
        flaechenNormale[static_cast<size_t>(f)] = Normalize((b - a) ^ (c - a));
        for (int k = 0; k < 3; ++k) flaechenAnVertex[mesh.faces[f].v[k]].push_back(f);
    }
    auto eckNormale = [&](int f, int k) -> Point3 {
        if (mitSpec) return spec->GetNormal(f, k);
        const int v = static_cast<int>(mesh.faces[f].v[k]);
        const DWORD sg = mesh.faces[f].smGroup;
        if (sg == 0) return flaechenNormale[static_cast<size_t>(f)];
        Point3 s(0, 0, 0);
        for (int g : flaechenAnVertex[static_cast<size_t>(v)]) if ((mesh.faces[g].smGroup & sg) != 0) s += flaechenNormale[static_cast<size_t>(g)];
        return Normalize(s);
    };

    Mtl* mtl = q.node->GetMtl();
    const bool mitUv = mesh.getNumTVerts() > 0 && mesh.tvFace != nullptr;
    std::map<Mtl*, std::map<std::tuple<int, int, int, int, int>, int>> eckIndex;
    for (int f = 0; f < nf; ++f) {
        Mtl* sub = mtl;
        if (mtl != nullptr && mtl->IsMultiMtl() && mtl->NumSubMtls() > 0) sub = mtl->GetSubMtl(mesh.faces[f].getMatID() % mtl->NumSubMtls());
        g2export::Flaeche& fl = proMaterial[sub];
        auto& idx = eckIndex[sub];
        for (int k = 0; k < 3; ++k) {
            const int v = static_cast<int>(mesh.faces[f].v[k]);
            const int tv = mitUv ? static_cast<int>(mesh.tvFace[f].t[k]) : -1;
            Point3 nObj = eckNormale(f, k);
            const auto qn = [](float x) { return static_cast<int>(std::lround(x * 1000.0f)); };
            const auto schl = std::make_tuple(v, tv, qn(nObj.x), qn(nObj.y), qn(nObj.z));
            auto it = idx.find(schl);
            if (it == idx.end()) {
                g2export::Ecke e;
                Point3 weltP = mesh.verts[v] * objZuWelt;
                Point3 weltN = objZuWelt.VectorTransform(nObj);
                if (!korrektur.empty()) {
                    Point3 sp(0, 0, 0), sn(0, 0, 0);
                    float sw = 0;
                    for (const auto& g : gew[static_cast<size_t>(v)]) {
                        const auto ki = korrektur.find(g.first);
                        if (ki == korrektur.end()) { sp += weltP * g.second; sn += weltN * g.second; }
                        else { sp += (weltP * ki->second) * g.second; sn += ki->second.VectorTransform(weltN) * g.second; }
                        sw += g.second;
                    }
                    if (sw > 0) { weltP = sp / sw; weltN = sn; }
                }
                const Point3 pw = SpielPunkt(weltP, raum);
                e.pos[0] = pw.x; e.pos[1] = pw.y; e.pos[2] = pw.z;
                alleSpielpunkte.push_back(pw);
                const Point3 nw = SpielRichtung(weltN, raum);
                e.normale[0] = nw.x; e.normale[1] = nw.y; e.normale[2] = nw.z;
                if (tv >= 0 && tv < mesh.getNumTVerts()) { e.uv[0] = mesh.tVerts[tv].x; e.uv[1] = 1.0f - mesh.tVerts[tv].y; }
                const auto& g = gew[static_cast<size_t>(v)];
                for (size_t j = 0; j < g.size() && j < 4; ++j) { e.knochen[j] = g[j].first; e.gewicht[j] = g[j].second; }
                it = idx.emplace(schl, static_cast<int>(fl.ecken.size())).first;
                fl.ecken.push_back(e);
            }
            fl.dreiecke.push_back(it->second);
        }
    }
    (void)ip;
    return true;
}

// ---- Der Export ----
bool Exportiere(const Optionen& o, std::wstring& bericht) {
    LogBlock lb;
    Interface* ip = GetCOREInterface();
    INode* top = ip->GetINodeByHandle(o.wurzel);
    if (top == nullptr) { bericht = L"The character is no longer in the scene."; return false; }
    if (o.name.empty()) { bericht = L"Enter a model name."; return false; }
    Log("=== JKA Export %s: %s -> %s\\models\\players\\%s (Skala %.3f, Drehen %d, Boden %d)", Utf8(JKAEXPORT_VERSION_STR).c_str(),
        N(top).c_str(), Utf8(o.basis).c_str(), o.name.c_str(), static_cast<double>(o.skala), o.drehen ? 1 : 0, o.boden ? 1 : 0);
    Raum raum;
    raum.s = o.skala;
    raum.drehen = o.drehen;
    raum.boden = o.boden ? -24.0f : 0.0f;
    const TimeValue t0 = 0;

    // 1. Knoten der Figur, Netze, Skins
    std::vector<INode*> alle;
    Sammle(top, alle);
    std::vector<NetzQuelle> netze;
    std::set<INode*> gehaeutet, knochenMenge;
    std::vector<Modifier*> skins;
    for (INode* n : alle) {
        Modifier* sk = FindeSkin(n);
        if (sk != nullptr) { netze.push_back({ n, sk }); gehaeutet.insert(n); skins.push_back(sk); }
        else if (IstGeometrie(n)) netze.push_back({ n, nullptr });
    }
    if (netze.empty()) { bericht = L"No meshes under this character."; return false; }
    for (INode* n : alle) if (!gehaeutet.count(n)) knochenMenge.insert(n);
    std::map<INode*, Matrix3> bindMax;
    for (size_t i = 0; i < skins.size(); ++i) {
        ISkin* is = static_cast<ISkin*>(skins[i]->GetInterface(I_SKIN));
        if (is == nullptr) continue;
        for (int b = 0; b < is->GetNumBones(); ++b) {
            INode* bn = is->GetBone(b);
            if (bn == nullptr) continue;
            for (INode* a = bn; a != nullptr && !a->IsRootNode(); a = a->GetParentNode()) knochenMenge.insert(a);
            Matrix3 tm;
            tm.IdentityMatrix();
            if (!bindMax.count(bn) && is->GetBoneInitTM(bn, tm, false) == SKIN_OK) bindMax[bn] = tm;
        }
    }

    // 2. Reihenfolge: Szene von oben, Elternteil vor Kind. [0] = model_root.
    std::vector<INode*> folge;
    std::function<void(INode*)> lauf = [&](INode* n) {
        if (knochenMenge.count(n)) folge.push_back(n);
        for (int i = 0; i < n->NumberOfChildren(); ++i) lauf(n->GetChildNode(i));
    };
    lauf(ip->GetRootNode());
    std::map<INode*, int> index;
    g2export::Auftrag a;
    a.name = o.name;
    a.basis = o.basis;
    a.herkunft = N(top);
    {
        g2export::Knochen w;
        w.name = "model_root";
        a.knochen.push_back(w);
    }
    std::set<std::string> namen = { "model_root" };
    std::vector<INode*> knochenNode = { nullptr };
    for (INode* n : folge) {
        g2export::Knochen k;
        std::string nm = N(n);
        if (nm.size() > 60) nm.resize(60);
        std::string e = nm;
        for (int i = 2; !namen.insert(Klein(e)).second; ++i) e = nm + "_" + std::to_string(i);
        k.name = e;
        k.eltern = 0;
        for (INode* p = n->GetParentNode(); p != nullptr && !p->IsRootNode(); p = p->GetParentNode()) {
            const auto it = index.find(p);
            if (it != index.end()) { k.eltern = it->second; break; }
        }
        index[n] = static_cast<int>(a.knochen.size());
        a.knochen.push_back(k);
        knochenNode.push_back(n);
    }
    if (a.knochen.size() > 256) { bericht = L"Too many bones (" + std::to_wstring(a.knochen.size()) + L", the engine allows 256)."; return false; }

    // 3. Bindepose in Max-Welt: Skin-Bones aus GetBoneInitTM, sonst relativ bei Bild 0.
    std::vector<Matrix3> bind(a.knochen.size());
    bind[0].IdentityMatrix();
    size_t ausSkin = 0;
    for (size_t i = 1; i < a.knochen.size(); ++i) {
        INode* n = knochenNode[i];
        const auto it = bindMax.find(n);
        if (it != bindMax.end()) { bind[i] = it->second; ++ausSkin; continue; }
        const int p = a.knochen[i].eltern;
        if (p <= 0) { bind[i] = n->GetNodeTM(t0); continue; }
        const Matrix3 lokal = n->GetNodeTM(t0) * Inverse(knochenNode[static_cast<size_t>(p)]->GetNodeTM(t0));
        bind[i] = lokal * bind[static_cast<size_t>(p)];
    }
    for (size_t i = 0; i < a.knochen.size(); ++i) a.knochen[i].bind = Spiel(bind[i], raum, false);
    Log("Knochen: %zu (davon %zu Bindelagen aus dem Skin), %zu Netze", a.knochen.size(), ausSkin, netze.size());

    // 4. Netze
    std::map<std::wstring, std::string> texturZiel;
    std::vector<Point3> punkte;
    std::set<std::string> flaechenNamen;
    for (const NetzQuelle& q : netze) {
        Matrix3 objZuWelt;
        if (q.skin != nullptr) {
            ISkin* is = static_cast<ISkin*>(q.skin->GetInterface(I_SKIN));
            if (is == nullptr || is->GetSkinInitTM(q.node, objZuWelt, true) != SKIN_OK) objZuWelt = q.node->GetObjectTM(t0);
        } else {
            const Matrix3 versatz = q.node->GetObjectTM(t0) * Inverse(q.node->GetNodeTM(t0));
            objZuWelt = versatz * bind[static_cast<size_t>(index[q.node])];
        }
        std::map<int, Matrix3> korrektur;
        if (q.skin != nullptr) {
            ISkin* is = static_cast<ISkin*>(q.skin->GetInterface(I_SKIN));
            for (int b = 0; is != nullptr && b < is->GetNumBones(); ++b) {
                INode* bn = is->GetBone(b);
                const auto ii = index.find(bn);
                Matrix3 tm;
                tm.IdentityMatrix();
                if (bn == nullptr || ii == index.end() || is->GetBoneInitTM(bn, tm, false) != SKIN_OK) continue;
                const Matrix3& glaBind = bind[static_cast<size_t>(ii->second)];
                float abw = 0;
                for (int r = 0; r < 4; ++r) {
                    const Point3 d = tm.GetRow(r) - glaBind.GetRow(r);
                    abw = std::max(abw, std::max({ std::fabs(d.x), std::fabs(d.y), std::fabs(d.z) }));
                }
                if (abw > 1e-3f) korrektur[ii->second] = Inverse(tm) * glaBind;
            }
            if (!korrektur.empty()) Log("Netz %s: %zu Bones anders gebunden als in der GLA - Vertices umgerechnet", N(q.node).c_str(), korrektur.size());
        }
        std::map<Mtl*, g2export::Flaeche> proMaterial;
        std::string fehler;
        if (!LiesNetz(ip, q, objZuWelt, index, q.skin ? -1 : index[q.node], korrektur, raum, proMaterial, punkte, fehler)) {
            Log("Netz %s uebersprungen: %s", N(q.node).c_str(), fehler.c_str());
            continue;
        }
        std::string basisName = Klein(N(q.node));
        if (basisName.size() > 5 && basisName.compare(basisName.size() - 5, 5, "_mesh") == 0) basisName.resize(basisName.size() - 5);
        for (auto& kv : proMaterial) {
            g2export::Flaeche& f = kv.second;
            f.name = basisName;
            if (proMaterial.size() > 1 && kv.first != nullptr) f.name += "_" + Klein(Utf8(kv.first->GetName().data()));
            const std::wstring quelle = TexturVon(kv.first);
            if (!quelle.empty()) {
                auto it = texturZiel.find(quelle);
                if (it == texturZiel.end()) {
                    const size_t sl = quelle.find_last_of(L"\\/");
                    std::string ziel = Klein(Utf8(quelle.substr(sl == std::wstring::npos ? 0 : sl + 1)));
                    for (char& c : ziel) if (c == ' ') c = '_';
                    it = texturZiel.emplace(quelle, ziel).first;
                    a.texturen.emplace_back(quelle, ziel);
                    const size_t pk = ziel.find_last_of('.');
                    const std::string end = pk == std::string::npos ? std::string() : ziel.substr(pk);
                    if (end != ".tga" && end != ".jpg" && end != ".png") Log("Textur %s: JKA laedt nur .tga/.jpg/.png", ziel.c_str());
                }
                f.textur = it->second;
            }
            Log("Netz %-20s %-20s %5zu Ecken %5zu Dreiecke %s", N(q.node).c_str(), f.name.c_str(), f.ecken.size(), f.dreiecke.size() / 3,
                f.textur.c_str());
            a.flaechen.push_back(std::move(f));
        }
    }

    // 5. Sequenzen aus den Notizspuren
    const std::vector<NotizKey> notizen = LiesNotizen();
    std::vector<Abschnitt> abschnitte = Abschnitte(notizen, "jka");
    std::string spur = "jka";
    if (abschnitte.empty()) { abschnitte = Abschnitte(notizen, "animations"); spur = "animations"; }
    const std::vector<Abschnitt> kotor = Abschnitte(notizen, "animations");
    for (Abschnitt& ab : abschnitte) {
        for (const Abschnitt& k : kotor) if (k.von == ab.von && k.name != ab.name) ab.kommentar = k.name;
        std::string gross;
        for (char c : ab.name) gross += static_cast<char>((c >= 'a' && c <= 'z') ? c - 'a' + 'A' : c);
        ab.name = gross;
    }
    const Interval bereich = ip->GetAnimRange();
    const TimeValue proBild = GetTicksPerFrame();
    if (abschnitte.empty()) {
        abschnitte.push_back({ "BOTH_STAND1", "whole time range", bereich.Start() / proBild, bereich.End() / proBild });
        spur = "(none - whole time range as BOTH_STAND1)";
    }
    const int fps = GetFrameRate();
    float hoehe = 0, minX = 1e9f, maxX = -1e9f, minY = 1e9f, maxY = -1e9f;
    for (const Point3& p : punkte) {
        hoehe = std::max(hoehe, p.z);
        minX = std::min(minX, p.x); maxX = std::max(maxX, p.x);
        minY = std::min(minY, p.y); maxY = std::max(maxY, p.y);
    }
    const bool schwer = hoehe > 96.0f;

    // Frame 0 = ROOT (Bindepose, gesenkt), danach die Sequenzen.
    const size_t nb = a.knochen.size();
    auto nimmFrame = [&](TimeValue t, bool bindepose) {
        for (size_t i = 0; i < nb; ++i) {
            if (i == 0) {
                g2export::Mat m;
                m.m[2][3] = raum.boden;
                a.welt.push_back(m);
            } else {
                const Matrix3 tm = bindepose ? bind[i] : knochenNode[i]->GetNodeTM(t);
                a.welt.push_back(Spiel(tm, raum, true));
            }
        }
        ++a.frames;
    };
    nimmFrame(0, true);
    {
        g2export::Sequenz root;
        root.name = "ROOT";
        root.erstes = 0;
        root.anzahl = 1;
        root.fps = fps;
        a.sequenzen.push_back(root);
    }
    std::set<std::string> schonSeq;
    for (const Abschnitt& ab : abschnitte) {
        if (!schonSeq.insert(ab.name).second) { Log("Sequenz %s doppelt - nur die erste", ab.name.c_str()); continue; }
        g2export::Sequenz s;
        s.name = ab.name;
        s.kommentar = ab.kommentar;
        s.schleife = IstSchleife(ab.name);
        s.fps = fps;
        s.erstes = a.frames;
        const int bis = (s.schleife && ab.bis > ab.von) ? ab.bis - 1 : ab.bis;
        for (int b = ab.von; b <= bis; ++b) nimmFrame(b * proBild, false);
        s.anzahl = a.frames - s.erstes;
        bool rechts = true;
        for (const NotizKey& k : notizen) {
            if (k.spur != "events" || k.bild < ab.von || k.bild > bis || Klein(k.text).find("footstep") == std::string::npos) continue;
            g2export::Schritt sch;
            sch.bild = k.bild - ab.von;
            sch.art = std::string(schwer ? "FOOTSTEP_HEAVY_" : "FOOTSTEP_") + (rechts ? "R" : "L");
            rechts = !rechts;
            s.schritte.push_back(sch);
        }
        a.sequenzen.push_back(s);
        Log("Sequenz %-22s Bilder %5d..%-5d -> GLA %5d +%-4d %s %zu Schritte%s%s", s.name.c_str(), ab.von, bis, s.erstes, s.anzahl,
            s.schleife ? "Schleife" : "einmal  ", s.schritte.size(), s.kommentar.empty() ? "" : "  // ", s.kommentar.c_str());
    }

    // 5b. Auf der Stelle (4.10.2026, DH: "Tiere bewegen sich vor statt auf der
    //     Stelle"). Gemessen: KOTOR-Angriffe schieben den Koerper fast eine ganze
    //     Koerperlaenge vor (Kinrath/Tukata/Kath-Hund 47-58 Einheiten), Tod und
    //     Umfallen enden versetzt, viele Clips beginnen 9-30 Einheiten neben der
    //     Ruhelage. In JKA bewegt der Code das NPC; der Clip muss stehen bleiben.
    //     Bewegungsknochen = der oberste Knochen, dessen Lage sich waagerecht
    //     aendert (KOTOR: rootdummy). In jedem Frame wird sein XY auf die Ruhelage
    //     gelegt und das ganze Skelett mitverschoben - Hoehe und Glieder bleiben.
    std::string bewegung;
    double entfernt = 0.0;
    if (o.anOrt && a.frames > 1) {
        // Wahl (zweiter Anlauf): "rootdummy" (KOTOR), sonst der bewegte Knochen mit
        // den meisten Nachfahren - mindestens ein Viertel des Skeletts, also der
        // Koerper. Die erste Regel "oberster bewegter Knochen" nahm beim Krayt-
        // Drachen eine Pfote (KDB_B_Paw_L), die direkt unter der Wurzel haengt.
        std::vector<int> nachfahren(nb, 0);
        for (size_t i = nb - 1; i >= 1; --i) {
            const int p = a.knochen[i].eltern;
            if (p >= 0) nachfahren[static_cast<size_t>(p)] += nachfahren[i] + 1;
        }
        int bester = -1;
        for (size_t i = 1; i < nb; ++i) {
            float lo[2] = { 1e9f, 1e9f }, hi[2] = { -1e9f, -1e9f };
            for (int f = 0; f < a.frames; ++f) {
                const g2export::Mat& m = a.welt[static_cast<size_t>(f) * nb + i];
                for (int k = 0; k < 2; ++k) { lo[k] = std::min(lo[k], m.m[k][3]); hi[k] = std::max(hi[k], m.m[k][3]); }
            }
            if (std::max(hi[0] - lo[0], hi[1] - lo[1]) < 1.0f) continue;
            if (Klein(a.knochen[i].name) == "rootdummy") { bester = static_cast<int>(i); break; }
            if (nachfahren[i] * 4 < static_cast<int>(nb)) continue;
            if (bester < 0 || nachfahren[i] > nachfahren[static_cast<size_t>(bester)]) bester = static_cast<int>(i);
        }
        if (bester > 0) {
            bewegung = a.knochen[static_cast<size_t>(bester)].name;
            const float rx = a.knochen[static_cast<size_t>(bester)].bind.m[0][3], ry = a.knochen[static_cast<size_t>(bester)].bind.m[1][3];
            for (int f = 0; f < a.frames; ++f) {
                const g2export::Mat& m = a.welt[static_cast<size_t>(f) * nb + static_cast<size_t>(bester)];
                const float dx = m.m[0][3] - rx, dy = m.m[1][3] - ry;
                entfernt = std::max(entfernt, std::sqrt(static_cast<double>(dx) * dx + static_cast<double>(dy) * dy));
                for (size_t i = 0; i < nb; ++i) {
                    g2export::Mat& w = a.welt[static_cast<size_t>(f) * nb + i];
                    w.m[0][3] -= dx;
                    w.m[1][3] -= dy;
                }
            }
            Log("Auf der Stelle: Bewegungsknochen %s, groesste waagerechte Verschiebung entfernt %.1f Einheiten", bewegung.c_str(), entfernt);
        }
    }

    // 6. NPC
    a.npc.schreiben = o.npc;
    a.npc.klasse = o.klasse;
    a.npc.gesundheit = o.gesundheit;
    a.npc.hoehe = std::max(16, static_cast<int>(std::lround(hoehe)));
    a.npc.breite = std::min(120, std::max(8, static_cast<int>(std::lround(std::max(maxX - minX, maxY - minY) * 0.5f))));

    const g2export::Ergebnis e = g2export::Schreibe(a);
    if (!e.protokoll.empty()) Log("%s", e.protokoll.c_str());
    bericht = Breit(e.bericht);
    if (e.ok) {
        if (!bewegung.empty())
            bericht += L"\nIn place: horizontal motion of " + Breit(bewegung) + L" removed (up to " +
                       std::to_wstring(static_cast<int>(entfernt + 0.5)) + L" units).";
        bericht += L"\n\nSequences from note track \"" + Breit(spur) + L"\". Height " + std::to_wstring(a.npc.hoehe) + L", width " +
                   std::to_wstring(a.npc.breite) + L".\nFolder: " + e.ordner;
        if (o.npc) bericht += L"\nSpawn in game:  npc spawn " + Breit(o.name);
    }
    Log("%s", Utf8(bericht).c_str());
    g_bericht = bericht;
    return e.ok;
}

uint32_t MaxFarbe(int welche) { return static_cast<uint32_t>(GetCustSysColor(welche)); }

} // namespace

int OeffneFenster(const std::string& startName, const std::wstring& startBasis) {
    Bruecke b;
    b.hInstance = hInstance;
    b.eltern = GetCOREInterface()->GetMAXHWnd();
    b.farbe = &MaxFarbe;
    b.figuren = &Figuren;
    b.exportiere = &Exportiere;
    b.version = JKAEXPORT_VERSION_STR;
    b.startName = startName;
    b.startBasis = startBasis;
    return ZeigeExportFenster(b);
}

bool SkriptExport(const std::wstring& wurzel, const std::string& name, const std::wstring& basis, float skala, int drehen,
                  const std::string& klasse) {
    Interface* ip = GetCOREInterface();
    INode* n = ip->GetINodeByName(wurzel.c_str());
    if (n == nullptr) { g_bericht = L"Node not found: " + wurzel; return false; }
    while (n->GetParentNode() && !n->GetParentNode()->IsRootNode()) n = n->GetParentNode();
    Optionen o;
    o.wurzel = n->GetHandle();
    o.name = name;
    o.basis = basis;
    o.skala = skala > 0 ? skala : 1.0f;
    o.drehen = drehen < 0 ? (!UserProp(n, _T("kotor_rig")).empty() && UserProp(n, _T("kotor_blick")) != "-Y") : drehen != 0;
    o.boden = true;
    o.anOrt = IniLies(L"Export", L"AnOrt", L"1") != L"0";
    o.npc = !klasse.empty();
    if (!klasse.empty()) o.klasse = klasse;
    std::wstring bericht;
    return Exportiere(o, bericht);
}

const std::wstring& LetzterBericht() { return g_bericht; }

} // namespace jkaexp

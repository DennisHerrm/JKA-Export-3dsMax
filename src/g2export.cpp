// ============================================================
//  g2export.cpp - aus den einfachen Strukturen (g2export.h) die
//  Ghoul2-Dateien schreiben. Benutzt die g2c-Schreiber (vendor/g2c).
// ============================================================
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "g2export.h"

#include "g2/mdxa.h"
#include "g2/mdxm.h"
#include "g2/model.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <exception>
#include <map>
#include <set>
#include <sstream>

namespace g2export {

namespace {

std::wstring Breit(const std::string& s) {
    if (s.empty()) return std::wstring();
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(static_cast<size_t>(n > 0 ? n : 0), L'\0');
    if (n > 0) MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), &w[0], n);
    return w;
}

void LegeOrdnerAn(const std::wstring& p) {
    if (p.empty()) return;
    const DWORD a = GetFileAttributesW(p.c_str());
    if (a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY)) return;
    const size_t s = p.find_last_of(L"\\/");
    if (s != std::wstring::npos && s > 2) LegeOrdnerAn(p.substr(0, s));
    CreateDirectoryW(p.c_str(), nullptr);
}

bool Schreib(const std::wstring& pfad, const void* d, size_t n) {
    std::FILE* f = _wfopen(pfad.c_str(), L"wb");
    if (f == nullptr) return false;
    const size_t w = n ? std::fwrite(d, 1, n, f) : 0;
    const bool zu = std::fclose(f) == 0;
    return zu && w == n;
}

bool Schreib(const std::wstring& pfad, const std::string& text) { return Schreib(pfad, text.data(), text.size()); }

g2::Mat3x4 AlsG2(const Mat& m) {
    g2::Mat3x4 r;
    for (int i = 0; i < 3; ++i) for (int j = 0; j < 4; ++j) r.m[i][j] = m.m[i][j];
    return r;
}

std::string Klein(std::string s) {
    for (char& c : s) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return s;
}

// Surfaces ueber 1000 Vertices oder 32 Bones in Stuecke teilen. Dreieck fuer
// Dreieck: passt es nicht mehr ins laufende Stueck, beginnt ein neues.
std::vector<Flaeche> Teile(const Flaeche& f, int& geteilt) {
    std::vector<Flaeche> aus;
    Flaeche akt;
    std::map<int, int> neu;
    std::set<int> bones;
    auto beginne = [&]() {
        akt = Flaeche();
        akt.name = f.name;
        akt.textur = f.textur;
        neu.clear();
        bones.clear();
    };
    beginne();
    for (size_t t = 0; t + 2 < f.dreiecke.size(); t += 3) {
        int neueVerts = 0;
        std::set<int> neueBones;
        for (int k = 0; k < 3; ++k) {
            const int e = f.dreiecke[t + static_cast<size_t>(k)];
            if (neu.count(e)) continue;
            ++neueVerts;
            for (int b = 0; b < 4; ++b) {
                const Ecke& ec = f.ecken[static_cast<size_t>(e)];
                if (ec.knochen[b] >= 0 && ec.gewicht[b] > 0 && !bones.count(ec.knochen[b])) neueBones.insert(ec.knochen[b]);
            }
        }
        if (!akt.dreiecke.empty() && (static_cast<int>(akt.ecken.size()) + neueVerts > kMaxVertsJeFlaeche ||
                                      static_cast<int>(bones.size() + neueBones.size()) > kMaxKnochenJeFlaeche)) {
            aus.push_back(akt);
            beginne();
        }
        for (int k = 0; k < 3; ++k) {
            const int e = f.dreiecke[t + static_cast<size_t>(k)];
            auto it = neu.find(e);
            if (it == neu.end()) {
                it = neu.emplace(e, static_cast<int>(akt.ecken.size())).first;
                const Ecke& ec = f.ecken[static_cast<size_t>(e)];
                akt.ecken.push_back(ec);
                for (int b = 0; b < 4; ++b) if (ec.knochen[b] >= 0 && ec.gewicht[b] > 0) bones.insert(ec.knochen[b]);
            }
            akt.dreiecke.push_back(it->second);
        }
    }
    if (!akt.dreiecke.empty()) aus.push_back(akt);
    if (aus.size() > 1) {
        geteilt += static_cast<int>(aus.size()) - 1;
        for (size_t i = 1; i < aus.size(); ++i) aus[i].name = f.name + "_" + std::to_string(i + 1);
    }
    return aus;
}

bool Passiv(const std::string& klasse) {
    for (const char* k : { "CLASS_GONK", "CLASS_R2D2", "CLASS_R5D2", "CLASS_MOUSE", "CLASS_PROTOCOL", "CLASS_CIVILIAN" })
        if (klasse == k) return true;
    return false;
}

} // namespace

Ergebnis Schreibe(const Auftrag& a) {
    Ergebnis e;
    std::ostringstream log;
    try {
        const size_t nb = a.knochen.size();
        if (a.name.empty()) throw std::runtime_error("no model name");
        if (nb == 0) throw std::runtime_error("no bones");
        if (a.frames <= 0 || a.welt.size() != static_cast<size_t>(a.frames) * nb) throw std::runtime_error("frame data incomplete");
        const std::string pfadSpiel = "models/players/" + a.name + "/";
        e.ordner = a.basis + L"\\models\\players\\" + Breit(a.name);
        LegeOrdnerAn(e.ordner);

        // ---- GLA ----
        g2::Skeleton skel;
        skel.name = pfadSpiel + a.name;
        skel.scale = 1.0f;
        for (const Knochen& k : a.knochen) {
            g2::Bone b;
            b.name = k.name;
            b.parent = k.eltern;
            b.basePose = AlsG2(k.bind);
            skel.bones.push_back(b);
        }
        const auto fehlerSkel = skel.validate();
        if (!fehlerSkel.empty()) throw std::runtime_error("skeleton: " + fehlerSkel.front());
        std::vector<g2::Mat3x4> bInv(nb);
        for (size_t b = 0; b < nb; ++b) bInv[b] = g2::affineInverse(skel.bones[b].basePose);
        g2::AnimationFrames frames;
        frames.resize(a.frames, static_cast<int>(nb));
        for (int f = 0; f < a.frames; ++f) {
            for (size_t b = 0; b < nb; ++b) {
                const g2::Mat3x4 X = AlsG2(a.welt[static_cast<size_t>(f) * nb + b]);
                const int p = a.knochen[b].eltern;
                g2::Mat3x4 A;
                if (p < 0) {
                    A = g2::mul(X, bInv[b]);
                } else {
                    const g2::Mat3x4 Xp = AlsG2(a.welt[static_cast<size_t>(f) * nb + static_cast<size_t>(p)]);
                    A = g2::mul(g2::mul(skel.bones[static_cast<size_t>(p)].basePose, g2::affineInverse(Xp)), g2::mul(X, bInv[b]));
                }
                frames.at(f, static_cast<int>(b)) = A;
            }
        }
        // Das Format kennt je Bone und Frame nur Verschiebungen bis +-511 Einheiten
        // (relativ zum Elternknochen). Wer darueber liegt, wird hier benannt -
        // meist hilft ein kleinerer Massstab.
        size_t zuWeit = 0;
        std::string zuWeitBeispiel;
        for (int f = 0; f < a.frames; ++f) {
            for (size_t b = 0; b < nb; ++b) {
                const g2::Mat3x4& A = frames.at(f, static_cast<int>(b));
                const float m = std::max({ std::fabs(A.m[0][3]), std::fabs(A.m[1][3]), std::fabs(A.m[2][3]) });
                if (m <= 511.0f) continue;
                if (zuWeit++ == 0) {
                    std::string seq = "?";
                    for (const Sequenz& s : a.sequenzen) if (f >= s.erstes && f < s.erstes + s.anzahl) seq = s.name;
                    zuWeitBeispiel = a.knochen[b].name + " in " + seq + " (" + std::to_string(static_cast<int>(m)) + " units)";
                }
            }
        }
        if (zuWeit) log << "Ausserhalb +-511: " << zuWeit << " Werte, zuerst " << zuWeitBeispiel << "\n";
        const g2::MdxaWriteResult gla = g2::writeMdxa(skel, frames);
        if (!Schreib(e.ordner + L"\\" + Breit(a.name) + L".gla", gla.data.data(), gla.data.size()))
            throw std::runtime_error("cannot write the .gla");
        log << "GLA: " << nb << " Bones, " << a.frames << " Frames, " << gla.data.size() << " Byte, Pool " << gla.poolEntries
            << " (Dedupe " << static_cast<int>(gla.dedupeRatio() * 100) << " %), Kompression: " << gla.stats.summary() << "\n";

        // ---- GLM ----
        g2::Mesh mesh;
        mesh.name = pfadSpiel + "model.glm";
        mesh.animName = pfadSpiel + a.name;
        mesh.numBones = static_cast<int>(nb);
        g2::LOD lod;
        std::set<std::string> namen;
        int geteilt = 0;
        size_t verts = 0, tris = 0;
        std::vector<std::pair<std::string, std::string>> skin;
        for (const Flaeche& f0 : a.flaechen) {
            if (f0.dreiecke.empty()) continue;
            for (Flaeche& f : Teile(f0, geteilt)) {
                std::string n = Klein(f.name);
                while (!n.empty() && n[0] == '*') n.erase(0, 1);
                if (n.empty()) n = "surface";
                if (n.size() > 60) n.resize(60);
                std::string eindeutig = n;
                for (int i = 2; !namen.insert(eindeutig).second; ++i) eindeutig = n + "_" + std::to_string(i);
                g2::Surface s;
                s.name = eindeutig;
                s.shader = f.textur.empty() ? std::string() : pfadSpiel + f.textur;
                s.parentIndex = lod.surfaces.empty() ? -1 : 0;
                for (const Ecke& ec : f.ecken) {
                    g2::Vertex v;
                    for (int i = 0; i < 3; ++i) { v.position[i] = ec.pos[i]; v.normal[i] = ec.normale[i]; }
                    v.uv[0] = ec.uv[0];
                    v.uv[1] = ec.uv[1];
                    for (int b = 0; b < 4; ++b)
                        if (ec.knochen[b] >= 0 && ec.gewicht[b] > 0) v.weights.push_back(g2::VertexWeight{ ec.knochen[b], ec.gewicht[b] });
                    s.vertices.push_back(std::move(v));
                }
                // WINDUNG: Ghoul2 (Quake 3) wertet Dreiecke IM Uhrzeigersinn als vorn,
                // Max gegen den Uhrzeigersinn. Gemessen an Ravens rancor/wampa/howler
                // (test/windung.py: 99,6 % gegen die Vertexnormale) - DH sah im ModView
                // die Innenseiten (4.10.2026). Deshalb a, c, b.
                for (size_t t = 0; t + 2 < f.dreiecke.size(); t += 3) {
                    g2::Triangle tr;
                    tr.indexes[0] = f.dreiecke[t];
                    tr.indexes[1] = f.dreiecke[t + 2];
                    tr.indexes[2] = f.dreiecke[t + 1];
                    s.triangles.push_back(tr);
                }
                verts += s.vertices.size();
                tris += s.triangles.size();
                if (!s.shader.empty()) skin.emplace_back(s.name, s.shader);
                lod.surfaces.push_back(std::move(s));
            }
        }
        if (lod.surfaces.empty()) throw std::runtime_error("no meshes");
        mesh.lods.push_back(std::move(lod));
        const auto fehlerMesh = mesh.validate();
        if (!fehlerMesh.empty()) throw std::runtime_error("mesh: " + fehlerMesh.front());
        const g2::MdxmWriteResult glm = g2::writeMdxm(mesh);
        if (!Schreib(e.ordner + L"\\model.glm", glm.data.data(), glm.data.size())) throw std::runtime_error("cannot write model.glm");
        log << "GLM: " << mesh.lods[0].surfaces.size() << " Surfaces (" << geteilt << " durch Grenzen geteilt), " << verts << " Vertices, "
            << tris << " Dreiecke, bis " << glm.stats.maxBoneRefsUsed << " Bones je Surface, " << glm.stats.weightsDropped
            << " Gewichte verworfen, " << glm.data.size() << " Byte\n";

        // ---- animation.cfg ----
        std::ostringstream cfg;
        cfg << "// models/players/" << a.name << "/animation.cfg - " << a.frames << " frames; " << a.sequenzen.size()
            << " sequences; written by JKA Export" << (a.herkunft.empty() ? "" : " from " + a.herkunft) << "\r\n//\r\n"
            << "// Format:  enum, targetFrame, frameCount, loopFrame, frameSpeed\r\n//\r\n";
        for (const Sequenz& s : a.sequenzen) {
            char z[256];
            std::snprintf(z, sizeof z, "%-24s\t%d\t%d\t%d\t%d", s.name.c_str(), s.erstes, s.anzahl, s.schleife ? 0 : -1, s.fps);
            cfg << z;
            if (!s.kommentar.empty()) cfg << "\t// " << s.kommentar;
            cfg << "\r\n";
        }
        if (!Schreib(e.ordner + L"\\animation.cfg", cfg.str())) throw std::runtime_error("cannot write animation.cfg");

        // ---- animevents.cfg ----
        std::ostringstream ev;
        size_t schritte = 0;
        ev << "// animevents.cfg written by JKA Export\r\n// FOOTSTEP format: animID AEV_FOOTSTEP framenum footstepType chancetoplay (0 = always)\r\n\r\n"
              "UPPEREVENTS\r\n{\r\n}\r\n\r\nLOWEREVENTS\r\n{\r\n";
        for (const Sequenz& s : a.sequenzen) {
            for (const Schritt& sch : s.schritte) {
                char z[256];
                std::snprintf(z, sizeof z, "\t%-24s AEV_FOOTSTEP %4d %s 0\r\n", s.name.c_str(), sch.bild, sch.art.c_str());
                ev << z;
                ++schritte;
            }
        }
        ev << "}\r\n";
        if (!Schreib(e.ordner + L"\\animevents.cfg", ev.str())) throw std::runtime_error("cannot write animevents.cfg");

        // ---- model_default.skin ----
        std::string sk;
        for (const auto& kv : skin) sk += kv.first + "," + kv.second + "\r\n";
        Schreib(e.ordner + L"\\model_default.skin", sk);

        // ---- Texturen ----
        size_t kopiert = 0;
        for (const auto& t : a.texturen) {
            if (CopyFileW(t.first.c_str(), (e.ordner + L"\\" + Breit(t.second)).c_str(), FALSE)) ++kopiert;
            else log << "Textur nicht kopiert: " << t.second << "\n";
        }

        // ---- NPC ----
        std::wstring npcDatei;
        if (a.npc.schreiben) {
            const bool passiv = Passiv(a.npc.klasse);
            std::ostringstream n;
            n << "// ext_data/npcs/" << a.name << ".npc - written by JKA Export\r\n\r\n" << a.name << "\r\n{\r\n"
              << "\tplayerModel\t" << a.name << "\r\n"
              << "\tweapon\t\t" << (passiv ? "WP_NONE" : "WP_MELEE") << "\r\n"
              << "\thealth\t\t" << a.npc.gesundheit << "\r\n"
              << "\treactions\t3\r\n\taim\t\t1\r\n\tmove\t\t3\r\n\taggression\t3\r\n\tevasion\t\t1\r\n\tintelligence\t5\r\n"
              << "\tplayerTeam\t" << (passiv ? "TEAM_NEUTRAL" : "TEAM_FREE") << "\r\n"
              << "\tenemyTeam\t" << (passiv ? "TEAM_NEUTRAL" : "TEAM_FREE") << "\r\n"
              << "\tclass\t\t" << a.npc.klasse << "\r\n"
              << "\theight\t\t" << a.npc.hoehe << "\r\n\tcrouchheight\t" << a.npc.hoehe << "\r\n"
              << "\twidth\t\t" << a.npc.breite << "\r\n"
              << "\tyawSpeed\t60\r\n\twalkSpeed\t" << a.npc.gehen << "\r\n\trunSpeed\t" << a.npc.rennen << "\r\n"
              << "\thFOV\t\t120\r\n\tvfov\t\t45\r\n}\r\n";
            const std::wstring ordner = a.basis + L"\\ext_data\\npcs";
            LegeOrdnerAn(ordner);
            npcDatei = ordner + L"\\" + Breit(a.name) + L".npc";
            if (!Schreib(npcDatei, n.str())) { log << "NPC-Datei nicht geschrieben\n"; npcDatei.clear(); }
        }

        std::ostringstream b;
        b << "Exported " << a.name << ": " << nb << " bones, " << a.frames << " frames in " << a.sequenzen.size() << " sequences, "
          << mesh.lods[0].surfaces.size() << " surfaces (" << verts << " vertices";
        if (geteilt) b << ", " << geteilt << " extra from splitting at 1000 verts / 32 bones";
        b << "), " << kopiert << " textures, " << schritte << " footstep events";
        if (!npcDatei.empty()) b << ", NPC file";
        if (!gla.stats.clean() || zuWeit)
            b << ". WARNING: " << zuWeit << " bone offsets exceed the format limit of 511 units (first: " << zuWeitBeispiel
              << ") - they were clamped; export with a smaller scale";
        b << ".";
        e.bericht = b.str();
        e.protokoll = log.str();
        e.ok = true;
    } catch (const std::exception& x) {
        e.ok = false;
        e.bericht = std::string("Export failed: ") + x.what();
        e.protokoll = log.str() + "FEHLER: " + x.what() + "\n";
    }
    return e;
}

} // namespace g2export

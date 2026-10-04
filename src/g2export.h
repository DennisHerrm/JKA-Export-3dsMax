// ============================================================
//  g2export.h - Export nach Jedi Academy (Ghoul2: GLA + GLM)
//
//  Bewusst ohne Max-SDK und ohne g2c-Typen: die Max-Schicht fuellt diese
//  einfachen Strukturen, g2export.cpp (C++20, mit den g2c-Schreibern) macht
//  daraus die Dateien:
//
//    <basis>\models\players\<name>\<name>.gla        Skelett + Animation
//    <basis>\models\players\<name>\model.glm         Meshes
//    <basis>\models\players\<name>\animation.cfg     Sequenzen
//    <basis>\models\players\<name>\animevents.cfg    Schritte
//    <basis>\models\players\<name>\model_default.skin
//    <basis>\models\players\<name>\*.tga             Texturen
//    <basis>\ext_data\npcs\<name>.npc                (wahlweise)
//
//  RAUM: alles hier ist schon im Spielraum (Einheiten, JKA-Ausrichtung:
//  Blick nach -Y, links +X, Z oben). Matrizen: Spaltenvektoren, Drehung in
//  den Spalten 0..2, Verschiebung in Spalte 3 (wie g2::Mat3x4).
//
//  FORMEL (aus g2c, gegen Ravens GLA geprueft): die Engine wertet
//  W(b) = W(Eltern) * A(b) aus, Skinning-Matrix W = X * B^-1. Gespeichert wird
//      Wurzel:  A = X * B^-1
//      sonst:   A = B(p) * X(p)^-1 * X(b) * B(b)^-1
//  mit X = Weltlage im Frame, B = Bindepose.
// ============================================================
#pragma once

#include <string>
#include <utility>
#include <vector>

namespace g2export {

struct Mat {
    float m[3][4] = { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 } };
};

struct Knochen {
    std::string name;
    int eltern = -1;          // Index in Auftrag::knochen, muss davor stehen
    Mat bind;                 // Bindepose, Welt
};

struct Ecke {
    float pos[3] = { 0, 0, 0 };
    float normale[3] = { 0, 0, 1 };
    float uv[2] = { 0, 0 };   // schon im JKA-Sinn (v = 1 - v_max)
    int knochen[4] = { -1, -1, -1, -1 };
    float gewicht[4] = { 0, 0, 0, 0 };
};

struct Flaeche {
    std::string name;         // Surface-Name (klein, ohne '*')
    std::string textur;       // Dateiname der Textur im Modellordner, leer = keine
    std::vector<Ecke> ecken;
    std::vector<int> dreiecke;  // 3 je Dreieck
};

struct Schritt {
    int bild = 0;             // relativ zum Sequenzanfang
    std::string art;          // FOOTSTEP_HEAVY_R ...
};

struct Sequenz {
    std::string name;         // BOTH_...
    int erstes = 0, anzahl = 0;   // in der GLA
    bool schleife = false;
    int fps = 30;
    std::vector<Schritt> schritte;
    std::string kommentar;    // z.B. KOTOR-Name
};

struct Npc {
    bool schreiben = true;
    std::string klasse = "CLASS_HOWLER";
    int hoehe = 64, breite = 16, gesundheit = 200;
    int gehen = 60, rennen = 200;
};

struct Auftrag {
    std::string name;                 // Ordner- und GLA-Name (klein)
    std::wstring basis;               // ...\base
    std::vector<Knochen> knochen;     // [0] = model_root
    int frames = 0;
    std::vector<Mat> welt;            // frames * knochen.size(): X(b) je Frame
    std::vector<Flaeche> flaechen;
    std::vector<Sequenz> sequenzen;
    std::vector<std::pair<std::wstring, std::string>> texturen;   // Quelldatei -> Zielname
    Npc npc;
    std::string herkunft;             // fuer die Kopfzeilen
};

struct Ergebnis {
    bool ok = false;
    std::wstring ordner;
    std::string bericht;              // englisch, fuer den Anwender
    std::string protokoll;            // deutsch, Einzelheiten
};

Ergebnis Schreibe(const Auftrag& a);

// Grenzen des Formats (OpenJK mdx_format.h / qfiles.h).
constexpr int kMaxVertsJeFlaeche = 1000;
constexpr int kMaxKnochenJeFlaeche = 32;

} // namespace g2export

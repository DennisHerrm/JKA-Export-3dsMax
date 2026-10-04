// ============================================================
//  jkaexport_fenster.h - Exportfenster, von Max aus gesehen.
//  Ohne Windows- und SDK-Typen (Fenster = Win32, Max-Schicht = SDK).
// ============================================================
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace jkaexp {

struct Optionen {
    unsigned long wurzel = 0;        // Handle des obersten Knotens der Figur
    std::string name;                // Ordnername (klein)
    std::wstring basis;              // ...\base (darunter models\players\<name>)
    float skala = 1.0f;              // Max-Einheiten -> Spieleinheiten
    bool drehen = false;             // Figur schaut in Max nach +Y (KOTOR) -> um 180 Grad drehen
    bool boden = true;               // Frames um -24 senken (NPC-Unterkante, wie Ravens Modelle)
    bool anOrt = true;               // waagerechte Bewegung des Bewegungsknochens entfernen
    bool npc = true;                 // ext_data\npcs\<name>.npc schreiben
    std::string klasse = "CLASS_HOWLER";
    int gesundheit = 200;
};

struct Figur {                       // eine exportierbare Hierarchie der Szene
    unsigned long handle = 0;
    std::string name;
    bool kotor = false;              // vom KOTOR Import (Benutzereigenschaft kotor_rig)
    int netze = 0, knochen = 0;
};

struct Bruecke {
    void* hInstance = nullptr;
    void* eltern = nullptr;
    uint32_t (*farbe)(int sysIndex) = nullptr;
    std::vector<Figur> (*figuren)() = nullptr;
    // Liest die Szene, schreibt die Dateien. bericht = Text fuer den Anwender.
    bool (*exportiere)(const Optionen& o, std::wstring& bericht) = nullptr;
    std::wstring version;
    std::string startName;           // Vorbelegung (Datei -> Exportieren)
    std::wstring startBasis;
};

// Modal. 1 = exportiert, 0 = ohne, -1 = Fenster ging nicht auf.
int ZeigeExportFenster(const Bruecke& b);

std::wstring Ablage();                                    // %LOCALAPPDATA%\JkaExport
std::wstring Protokollpfad();                             // Downloads\Export jka.log
std::wstring IniLies(const wchar_t* abschnitt, const wchar_t* schluessel, const std::wstring& standard);
void IniSchreib(const wchar_t* abschnitt, const wchar_t* schluessel, const std::wstring& wert);
std::wstring Breit(const std::string& s);
std::string Utf8(const std::wstring& w);

// Die NPC-Klassen, die das Fenster anbietet (Name, kurze Beschreibung).
const std::vector<std::pair<std::string, std::string>>& Klassen();

} // namespace jkaexp

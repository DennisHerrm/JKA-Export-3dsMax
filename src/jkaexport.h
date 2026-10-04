// ============================================================
//  JKA Export - Plugin-Kopf
//
//  Exportiert eine Figur aus der Max-Szene direkt als Ghoul2-Modell fuer
//  Star Wars Jedi Knight: Jedi Academy (GLA + GLM + animation.cfg +
//  animevents.cfg + .skin + Texturen + wahlweise .npc). Die Schreiber
//  stammen aus g2c (vendor/g2c), der Max-Teil liest Skelett, Skin,
//  Materialien und die Sequenzen aus den Notizspuren der Szenenwurzel.
// ============================================================
#pragma once

#include "jkaexport_fenster.h"

#include <string>

#include <max.h>
#include <iparamb2.h>
#include <impexp.h>
#include <istdplug.h>

#define JKAEXPORT_VERSION      20
#define JKAEXPORT_VERSION_STR  _T("0.2.0")

#define JKAEXPORT_CLASS_ID  Class_ID(0x4a4b4145, 0x2f61c03b)
#define JKAEXPORT_FP_ID     Interface_ID(0x4a4b4146, 0x5d27e914)

namespace jkaexp {

int OeffneFenster(const std::string& startName = std::string(), const std::wstring& startBasis = std::wstring());
// Fuer Skripte und den Selbsttest: Figur ueber den Namen ihres obersten Knotens.
bool SkriptExport(const std::wstring& wurzel, const std::string& name, const std::wstring& basis, float skala, int drehen,
                  const std::string& klasse);
const std::wstring& LetzterBericht();

} // namespace jkaexp

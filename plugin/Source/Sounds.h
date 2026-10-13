/* SeriousPortal - Portal's own sounds, if the player's Portal gave them (tools/portal_assets.py puts them in the game's
 * Sounds\SeriousPortal). Without them the portals are silent. */
#ifndef SERIOUSPORTAL_SOUNDS_H
#define SERIOUSPORTAL_SOUNDS_H

#include "StdH.h"

namespace sounds {

// "shoot_blue", "shoot_orange", "open", "close", "invalid", "enter"
void Play(const char *strName);

}; // namespace

#endif

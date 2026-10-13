/* SeriousPortal - Portal's portal gun in Serious Sam Classic.
 *
 * Two portals, blue (0) and orange (1), shot onto Sam's walls, floors and ceilings along the view. Whatever goes into
 * one - the player, monsters, rockets and grenades - comes out of the other, its speed and direction turned with it
 * ("speedy thing goes in, speedy thing comes out"). Sam's world isn't changed: the wall behind a portal stays solid,
 * so going in happens as something touches it moving towards it (or stands on a floor portal). */
#ifndef SERIOUSPORTAL_PORTALS_H
#define SERIOUSPORTAL_PORTALS_H

#include "StdH.h"

namespace portals {

// the oval's half sizes, metres (Portal's portals are 64 x 112 units, about 1.2 x 2.1 m; Sam is a bit taller)
static const FLOAT HALF_W = 0.65f;
static const FLOAT HALF_H = 1.15f;

struct Portal {
  BOOL bOpen;
  FLOAT3D vCenter;     // on the surface
  FLOAT3D vN, vU, vR;  // out of the surface; the oval's up and right (R = U x N)
  FLOAT tmOpened;      // when (for the opening animation)
};

const Portal &Get(INDEX i);
BOOL Linked(void);

// Shoot portal i along the player's view; FALSE if nothing to hold it (the sky, a door) or no room next to the other.
BOOL Shoot(INDEX i, CEntity *penPlayer);
// The same from the player's eyes along a given direction (test hook).
BOOL ShootAlong(INDEX i, CEntity *penPlayer, const FLOAT3D &vDir);
// Close portal i (-1: both).
void Close(INDEX i);

// Through portal i and out of the other: a direction (or velocity), a point.
FLOAT3D MapVector(INDEX i, const FLOAT3D &v);
FLOAT3D MapPoint(INDEX i, const FLOAT3D &p);
// A rotation matrix (an entity's or a camera's axes) turned the same way.
FLOATmatrix3D MapRotation(INDEX i, const FLOATmatrix3D &m);

// Every simulation tick: what touches a portal comes out of the other.
void Step(CEntity *penPlayer);
void Reset(void);
CTString Status(void);

}; // namespace

#endif

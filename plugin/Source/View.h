/* SeriousPortal - drawing the portals.
 *
 * Each portal is an oval on its surface, ringed in its colour. Through it you see what's beyond the other one: before
 * Sam draws his frame, the world is drawn once more from where the camera would be had it gone through (the engine's
 * own renderer, with the other portal's surface as its warp clip plane), at half size, and copied into a texture; the
 * oval then shows that picture where it falls on the screen (projective texturing). */
#ifndef SERIOUSPORTAL_VIEW_H
#define SERIOUSPORTAL_VIEW_H

#include "StdH.h"

namespace view {

// 1: see through the portals (two more renders of the world a frame while both are open), 0: plain ovals.
extern INDEX sp_iSeeThrough;
// dev: 1 draws the portals over everything (no depth test)
extern INDEX sp_iDebugNoDepth;

// Before Sam draws: the views through the portals.
void PreDraw(CDrawPort *pdp, CEntity *penPlayer);
// The game's view was drawn (the main one): the portals on top, with depth.
void OnMainView(CEntity *penViewer, CAnyProjection3D &apr, CDrawPort *pdp);
// A view through a portal is being drawn (render hooks leave it alone).
BOOL Rendering(void);
// How many views through the portals were drawn so far.
ULONG ViewsDrawn(void);

}; // namespace

#endif

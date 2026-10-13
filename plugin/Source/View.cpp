/* SeriousPortal - drawing the portals. See View.h. */
#include "StdH.h"
#include "View.h"
#include "Portals.h"
#include "Gl.h"

namespace view {

INDEX sp_iSeeThrough = 1;
INDEX sp_iDebugNoDepth = 0;

static BOOL _bRendering = FALSE;
// the game's last main view: its projection as the game made it
static CPerspectiveProjection3D _prMain;
static BOOL _bHaveMain = FALSE;
// the pictures through the portals this frame (through portal i: beyond the other one)
static BOOL _abPicture[2] = { FALSE, FALSE };
static ULONG _ctViews = 0;

BOOL Rendering(void) { return _bRendering; };
ULONG ViewsDrawn(void) { return _ctViews; };

void PreDraw(CDrawPort *pdp, CEntity *penPlayer) {
  _abPicture[0] = _abPicture[1] = FALSE;
  if (!sp_iSeeThrough || !portals::Linked() || !_bHaveMain || pdp == NULL || pdp->dp_Raster == NULL || penPlayer == NULL) return;
  CWorld *pwo = IWorld::GetWorld();
  if (pwo == NULL || !GetGameAPI()->IsGameOn() || !gl::Ready()) return;

  const PIX w = pdp->GetWidth() / 2, h = pdp->GetHeight() / 2;
  if (w < 16 || h < 16) return;
  const CPlacement3D &plEye = _prMain.pr_ViewerPlacement;
  FLOATmatrix3D mEye;
  MakeRotationMatrix(mEye, plEye.pl_OrientationAngle);

  for (INDEX i = 0; i < 2; i++) {
    const portals::Portal &a = portals::Get(i), &b = portals::Get(1 - i);
    // only a portal the camera is in front of, and not too far
    const FLOAT3D vToEye = plEye.pl_PositionVector - a.vCenter;
    if ((vToEye % a.vN) <= 0.0f || vToEye.Length() > 300.0f) continue;

    // where the camera would be, gone through portal i and out of the other
    CPlacement3D plVirtual;
    plVirtual.pl_PositionVector = portals::MapPoint(i, plEye.pl_PositionVector);
    DecomposeRotationMatrixNoSnap(plVirtual.pl_OrientationAngle, portals::MapRotation(i, mEye));

    CPerspectiveProjection3D pr = _prMain;
    pr.ViewerPlacementL() = plVirtual;
    pr.ScreenBBoxL() = FLOATaabbox2D(FLOAT2D(0, 0), FLOAT2D((FLOAT)w, (FLOAT)h));
    // nothing on the near side of the other portal's surface: that's the wall it's on, and what's behind it
    pr.WarpPlaneL() = FLOATplane3D(-b.vN, b.vCenter + b.vN * 0.05f); // (just off it: the surface itself goes too)
    CAnyProjection3D apr;
    apr = pr;

    CDrawPort dpSub(pdp, PIXaabbox2D(PIX2D(0, 0), PIX2D(w, h)));
    _bRendering = TRUE;
    RenderView(*pwo, *penPlayer, apr, dpSub);
    _bRendering = FALSE;

    // the picture, from the framebuffer (GL counts rows from the bottom)
    gl::CopyToTexture(i, dpSub.dp_MinI, pdp->dp_Raster->ra_Height - (dpSub.dp_MinJ + h), w, h);
    _abPicture[i] = TRUE;
    _ctViews++;
  }
};

// 4x4 matrices, row-major: out = a * b
static void Mul(const float a[16], const float b[16], float out[16]) {
  for (INDEX r = 0; r < 4; r++) {
    for (INDEX c = 0; c < 4; c++) {
      float f = 0;
      for (INDEX k = 0; k < 4; k++) f += a[r * 4 + k] * b[k * 4 + c];
      out[r * 4 + c] = f;
    }
  }
};

static const INDEX SEGMENTS = 48;

// One portal: the picture beyond (or a swirl of its colour), then its ring and glow.
static void DrawPortal(INDEX i, const float afPV[16], FLOAT tmNow) {
  const portals::Portal &po = portals::Get(i);
  if (!po.bOpen) return;
  // it opens out from a point in a quarter of a second
  const FLOAT fOpen = Clamp((tmNow - po.tmOpened) / 0.25f, 0.05f, 1.0f);
  const FLOAT fW = portals::HALF_W * fOpen, fH = portals::HALF_H * fOpen;
  const FLOAT3D vC = po.vCenter + po.vN * 0.02f;
  static const FLOAT afCol[2][3] = { { 0.15f, 0.55f, 1.0f }, { 1.0f, 0.55f, 0.05f } };
  const FLOAT *col = afCol[i];

  // the oval at scale f: point s of SEGMENTS
  #define OVAL(s, f) (vC + po.vR * (cosf((s) * 2.0f * PI / SEGMENTS) * fW * (f)) + po.vU * (sinf((s) * 2.0f * PI / SEGMENTS) * fH * (f)))

  // inside: the centre, then the rim
  float afXYZ[(SEGMENTS + 2) * 3], afSTQ[(SEGMENTS + 2) * 3], afRGBA[(SEGMENTS + 2) * 4];
  const BOOL bPicture = _abPicture[i] && portals::Linked();
  for (INDEX k = 0; k < SEGMENTS + 2; k++) {
    const FLOAT3D v = k == 0 ? vC : OVAL(k - 1, 1.0f);
    afXYZ[k * 3 + 0] = v(1); afXYZ[k * 3 + 1] = v(2); afXYZ[k * 3 + 2] = v(3);
    // where it lands on the screen, as projective texture coordinates
    float c[4];
    for (INDEX r = 0; r < 4; r++) c[r] = afPV[r * 4 + 0] * v(1) + afPV[r * 4 + 1] * v(2) + afPV[r * 4 + 2] * v(3) + afPV[r * 4 + 3];
    afSTQ[k * 3 + 0] = (c[0] + c[3]) * 0.5f;
    afSTQ[k * 3 + 1] = (c[1] + c[3]) * 0.5f;
    afSTQ[k * 3 + 2] = c[3];
    const FLOAT fSwirl = k == 0 ? 0.25f : 0.45f + 0.25f * sinf(tmNow * 3.0f + k * 0.5f);
    afRGBA[k * 4 + 0] = col[0] * fSwirl; afRGBA[k * 4 + 1] = col[1] * fSwirl; afRGBA[k * 4 + 2] = col[2] * fSwirl; afRGBA[k * 4 + 3] = 0.92f;
  }
  gl::Fan(bPicture ? i : -1, afXYZ, afSTQ, afRGBA, SEGMENTS + 2);

  // the ring (bright), a glow outside it and a softer one inside
  static const FLOAT afRings[3][4] = { { 1.0f, 1.07f, 1.0f, 1.0f }, { 1.07f, 1.18f, 0.45f, 0.0f }, { 0.93f, 1.0f, 0.0f, 0.55f } };
  float afSX[(SEGMENTS + 1) * 2 * 3], afSC[(SEGMENTS + 1) * 2 * 4];
  for (INDEX r = 0; r < 3; r++) {
    for (INDEX s = 0; s <= SEGMENTS; s++) {
      const FLOAT fPulse = 0.85f + 0.15f * sinf(tmNow * 4.0f + s * 0.3f);
      const FLOAT3D vIn = OVAL(s, afRings[r][0]), vOut = OVAL(s, afRings[r][1]);
      const INDEX k = s * 2;
      afSX[k * 3 + 0] = vOut(1); afSX[k * 3 + 1] = vOut(2); afSX[k * 3 + 2] = vOut(3);
      afSX[k * 3 + 3] = vIn(1);  afSX[k * 3 + 4] = vIn(2);  afSX[k * 3 + 5] = vIn(3);
      afSC[k * 4 + 0] = col[0] * fPulse; afSC[k * 4 + 1] = col[1] * fPulse; afSC[k * 4 + 2] = col[2] * fPulse;
      afSC[k * 4 + 3] = r == 0 ? 1.0f : (r == 1 ? afRings[r][3] : afRings[r][2]);
      afSC[k * 4 + 4] = col[0]; afSC[k * 4 + 5] = col[1]; afSC[k * 4 + 6] = col[2];
      afSC[k * 4 + 7] = r == 0 ? 1.0f : (r == 1 ? afRings[r][2] : afRings[r][3]);
    }
    gl::Strip(afSX, afSC, (SEGMENTS + 1) * 2);
  }
  #undef OVAL
};

void OnMainView(CEntity *penViewer, CAnyProjection3D &apr, CDrawPort *pdp) {
  _prMain = *(CPerspectiveProjection3D *)(CProjection3D *)apr;
  _bHaveMain = TRUE;
  if (!portals::Get(0).bOpen && !portals::Get(1).bOpen) return;
  if (!gl::Ready()) return;

  // the projection the way the world was drawn (Render.cpp): its frustum and depth range (0..0.9)
  CPerspectiveProjection3D ppr = _prMain;
  ppr.ObjectPlacementL() = CPlacement3D(FLOAT3D(0, 0, 0), ANGLE3D(0, 0, 0));
  ppr.ObjectFaceForwardL() = FALSE;
  ppr.ObjectStretchL() = FLOAT3D(1, 1, 1);
  ppr.DepthBufferNearL() = 0.0f;
  ppr.DepthBufferFarL() = 0.9f;
  ppr.Prepare();

  const FLOAT fN = ppr.pr_NearClipDistance;
  const FLOAT fF = ppr.pr_FarClipDistance > 0 ? ppr.pr_FarClipDistance : 1E5f;
  const FLOAT l = ppr.pr_plClipL(3) / ppr.pr_plClipL(1) * fN, r = ppr.pr_plClipR(3) / ppr.pr_plClipR(1) * fN;
  const FLOAT b = ppr.pr_plClipD(3) / ppr.pr_plClipD(2) * fN, t = ppr.pr_plClipU(3) / ppr.pr_plClipU(2) * fN;
  const float afP[16] = {
    2 * fN / (r - l), 0, (r + l) / (r - l), 0,
    0, 2 * fN / (t - b), (t + b) / (t - b), 0,
    0, 0, -(fF + fN) / (fF - fN), -2 * fF * fN / (fF - fN),
    0, 0, -1, 0,
  };
  FLOATmatrix3D m;
  MakeRotationMatrix(m, ppr.pr_ViewerPlacement.pl_OrientationAngle);
  const FLOAT3D &e = ppr.pr_ViewerPlacement.pl_PositionVector;
  const float afV[16] = {
    m(1, 1), m(2, 1), m(3, 1), -(m(1, 1) * e(1) + m(2, 1) * e(2) + m(3, 1) * e(3)),
    m(1, 2), m(2, 2), m(3, 2), -(m(1, 2) * e(1) + m(2, 2) * e(2) + m(3, 2) * e(3)),
    m(1, 3), m(2, 3), m(3, 3), -(m(1, 3) * e(1) + m(2, 3) * e(2) + m(3, 3) * e(3)),
    0, 0, 0, 1,
  };
  float afPV[16], afGL[16];
  Mul(afP, afV, afPV);
  for (INDEX row = 0; row < 4; row++) for (INDEX c = 0; c < 4; c++) afGL[c * 4 + row] = afPV[row * 4 + c];

  gl::Begin(afGL, ppr.pr_fDepthBufferNear, ppr.pr_fDepthBufferFar, sp_iDebugNoDepth == 0);
  const FLOAT tmNow = _pTimer->GetLerpedCurrentTick();
  for (INDEX i = 0; i < 2; i++) DrawPortal(i, afPV, tmNow);
  gl::End();
};

}; // namespace

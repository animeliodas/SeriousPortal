/* SeriousPortal - Portal's portal gun in Serious Sam Classic (TFE 1.05 / TSE 1.07), Classics Patch plugin.
 *
 * Z shoots the blue portal where you look, X the orange one, C closes both. Walk, fall, shoot or throw through one and
 * it comes out of the other (Portals.cpp); you see through them (View.cpp). Portal's own sounds come from the
 * player's Portal (tools/portal_assets.py). Part of SeriousBridge: mechanics of other games in Serious Sam.
 */
#include "StdH.h"
#include "Portals.h"
#include "View.h"
#include "Sounds.h"
#include "Player.h"
#include "Physics.h"
#include "DevHook.h"

#include <stdio.h>

CLASSICSPATCH_DEFINE_PLUGIN(k_EPluginFlagGame, MakeVersion(0, 1, 0),
  "SeriousBridge", "SeriousPortal", "Portal's portal gun in Serious Sam: Z blue, X orange, C closes both.");

// keys (Windows virtual-key codes), persistent: free in Sam's default controls
INDEX sp_iKeyBlue = 'Z';
INDEX sp_iKeyOrange = 'X';
INDEX sp_iKeyClose = 'C';

static BOOL _abKeyWas[3] = { FALSE, FALSE, FALSE };
static BOOL _bViewDone = FALSE;

// (the dev hook - console commands from %TEMP%\SeriousPortal\cmd.txt for tests, on with SERIOUSBRIDGE_DEV=1 - and the
// player are the SeriousBridge core's: DevHook.h, Player.h)
static CEntity *LocalPlayer(void) { return player::Local(); };

// Sam's window in front and the game being played (no menu, console, chat or NETRICSA over it).
static BOOL Playing(void) {
  HWND hwnd = GetForegroundWindow();
  DWORD dwPid = 0;
  GetWindowThreadProcessId(hwnd, &dwPid);
  if (dwPid != GetCurrentProcessId()) return FALSE;
  CGameAPI *pGame = GetGameAPI();
  return pGame->IsGameOn() && !pGame->IsMenuOn() && pGame->GetConState() == 0 && pGame->GetCompState() == 0;
};

static void PollKeys(void) {
  const BOOL bPlaying = Playing();
  const INDEX aiKeys[3] = { sp_iKeyBlue, sp_iKeyOrange, sp_iKeyClose };
  for (INDEX k = 0; k < 3; k++) {
    const BOOL bDown = bPlaying && aiKeys[k] > 0 && aiKeys[k] < 256 && (GetAsyncKeyState(aiKeys[k]) & 0x8000) != 0;
    if (bDown && !_abKeyWas[k]) {
      if (k < 2) portals::Shoot(k, LocalPlayer()); else portals::Close(-1);
    }
    _abKeyWas[k] = bDown;
  }
};

// ------------------------------------------------------------------
// Console
// ------------------------------------------------------------------
static void SP_Shoot(SHELL_FUNC_ARGS) {
  BEGIN_SHELL_FUNC;
  const INDEX i = NEXT_ARG(INDEX);
  CPrintF("[SeriousPortal] shoot %s: %s\n", (i & 1) == 0 ? "blue" : "orange", portals::Shoot(i & 1, LocalPlayer()) ? "placed" : "no surface for it");
};

// Test hooks: shoot portal i from the eyes along a direction ("i dx dy dz"); move the player ("dx dy dz").
static void SP_ShootAt(SHELL_FUNC_ARGS) {
  BEGIN_SHELL_FUNC;
  const CTString &strArgs = *NEXT_ARG(CTString *);
  INDEX i = 0;
  FLOAT x = 0, y = 0, z = 0;
  if (sscanf(strArgs.str_String, "%d %f %f %f", &i, &x, &y, &z) != 4) return;
  CPrintF("[SeriousPortal] shoot %s along (%.2f, %.2f, %.2f): %s\n", (i & 1) == 0 ? "blue" : "orange", x, y, z,
    portals::ShootAlong(i & 1, LocalPlayer(), FLOAT3D(x, y, z)) ? "placed" : "no surface for it");
};

static void SP_Move(SHELL_FUNC_ARGS) {
  BEGIN_SHELL_FUNC;
  const CTString &strArgs = *NEXT_ARG(CTString *);
  FLOAT x = 0, y = 0, z = 0;
  CEntity *pen = LocalPlayer();
  if (pen == NULL || sscanf(strArgs.str_String, "%f %f %f", &x, &y, &z) != 3) return;
  CPlacement3D pl = pen->GetPlacement();
  pl.pl_PositionVector += FLOAT3D(x, y, z);
  physics::PlaceNow(pen, pl, NULL);
  CPrintF("[SeriousPortal] player at (%.2f, %.2f, %.2f)\n", pl.pl_PositionVector(1), pl.pl_PositionVector(2), pl.pl_PositionVector(3));
};

static void SP_Where(SHELL_FUNC_ARGS) {
  BEGIN_SHELL_FUNC;
  CEntity *pen = LocalPlayer();
  if (pen == NULL) return;
  const FLOAT3D v = pen->GetPlacement().pl_PositionVector;
  const FLOAT3D vVel = ((CMovableEntity *)pen)->en_vCurrentTranslationAbsolute;
  CPrintF("[SeriousPortal] where: %.3f %.3f %.3f moving %.2f %.2f %.2f\n", v(1), v(2), v(3), vVel(1), vVel(2), vVel(3));
};

static void SP_Close(SHELL_FUNC_ARGS) {
  BEGIN_SHELL_FUNC;
  portals::Close(-1);
};

static void SP_Status(SHELL_FUNC_ARGS) {
  BEGIN_SHELL_FUNC;
  CPrintF("[SeriousPortal] %s; %u views through them drawn\n", portals::Status().str_String, view::ViewsDrawn());
};

// ------------------------------------------------------------------
// Events
// ------------------------------------------------------------------
static void SP_OnStep(void) {
  if (!GetGameAPI()->IsGameOn()) return;
  portals::Step(LocalPlayer());
};

static void SP_OnFrame(CDrawPort *pdp) {
  devhook::Poll();
  PollKeys();
};

static void SP_OnPreDraw(CDrawPort *pdp) {
  _bViewDone = FALSE;
  view::PreDraw(pdp, LocalPlayer());
};

static void SP_OnRenderView(CWorld &wo, CEntity *penViewer, CAnyProjection3D &apr, CDrawPort *pdp) {
  if (view::Rendering() || _bViewDone || pdp == NULL || !apr.IsPerspective()) return;
  if (!GetGameAPI()->IsGameOn() || &wo != IWorld::GetWorld()) return;
  // the game view, not a small picture-in-picture
  CRaster *pra = pdp->dp_Raster;
  if (pra != NULL && pdp->GetWidth() * pdp->GetHeight() * 2 < pra->ra_Width * pra->ra_Height) return;
  _bViewDone = TRUE;
  view::OnMainView(penViewer, apr, pdp);
};

static void SP_OnWorldLoad(CWorld *pwo, const CTFileName &fnmWorld) {
  portals::Reset(); // portals belong to the level they were shot in
};

static void SP_OnGameStop(void) {
  portals::Reset();
};

CLASSICSPATCH_PLUGIN_STARTUP(HIniConfig props, PluginEvents_t &events) {
  events.m_processing->OnStep = &SP_OnStep;
  events.m_processing->OnFrame = &SP_OnFrame;
  events.m_rendering->OnPreDraw = &SP_OnPreDraw;
  events.m_rendering->OnRenderView = &SP_OnRenderView;
  events.m_world->OnWorldLoad = &SP_OnWorldLoad;
  events.m_game->OnGameStop = &SP_OnGameStop;

  ClassicsPlugins()->RegisterMethod(TRUE, "void", "sp_Shoot", "INDEX", &SP_Shoot);
  ClassicsPlugins()->RegisterMethod(TRUE, "void", "sp_Close", "void", &SP_Close);
  ClassicsPlugins()->RegisterMethod(TRUE, "void", "sp_ShootAt", "CTString", &SP_ShootAt);
  ClassicsPlugins()->RegisterMethod(TRUE, "void", "sp_Move", "CTString", &SP_Move);
  ClassicsPlugins()->RegisterMethod(TRUE, "void", "sp_Where", "void", &SP_Where);
  ClassicsPlugins()->RegisterMethod(TRUE, "void", "sp_Status", "void", &SP_Status);
  _pShell->DeclareSymbol("persistent user INDEX sp_iKeyBlue;", &sp_iKeyBlue);
  _pShell->DeclareSymbol("persistent user INDEX sp_iKeyOrange;", &sp_iKeyOrange);
  _pShell->DeclareSymbol("persistent user INDEX sp_iKeyClose;", &sp_iKeyClose);
  _pShell->DeclareSymbol("persistent user INDEX sp_iSeeThrough;", &view::sp_iSeeThrough);
  _pShell->DeclareSymbol("user INDEX sp_iDebugNoDepth;", &view::sp_iDebugNoDepth);

  devhook::Init("SERIOUSBRIDGE_DEV");
  CPrintF("^c00aaff[SeriousPortal]^r loaded: Z blue portal, X orange, C closes both\n");
};

CLASSICSPATCH_PLUGIN_SHUTDOWN(HIniConfig props) {
  devhook::Shutdown();
};

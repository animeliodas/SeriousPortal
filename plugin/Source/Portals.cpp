/* SeriousPortal - the portals and what goes through them. See Portals.h. */
#include "StdH.h"
#include "Portals.h"
#include "Sounds.h"
#include "Physics.h"

#include <map>

namespace portals {

static Portal _apo[2] = { { FALSE }, { FALSE } };
static ULONG _ctTicks = 0;
static ULONG _ctTeleports = 0, _ctShots = 0;
// per entity: the tick it may go through again (it just came out), and how far it was from each portal last tick
static std::map<ULONG, ULONG> _mapCooldown;
static std::map<ULONG, FLOAT> _mapGap[2];

const Portal &Get(INDEX i) { return _apo[i & 1]; };
BOOL Linked(void) { return _apo[0].bOpen && _apo[1].bOpen; };

void Reset(void) {
  _apo[0].bOpen = _apo[1].bOpen = FALSE;
  _mapCooldown.clear();
  _mapGap[0].clear();
  _mapGap[1].clear();
};

void Close(INDEX i) {
  if (i < 0) {
    if (_apo[0].bOpen || _apo[1].bOpen) sounds::Play("close");
    _apo[0].bOpen = _apo[1].bOpen = FALSE;
    return;
  }
  if (_apo[i & 1].bOpen) sounds::Play("close");
  _apo[i & 1].bOpen = FALSE;
};

// The first brush polygon along a ray (models don't hold portals); the point and the polygon, or NULL.
static CBrushPolygon *CastToWall(CEntity *penFrom, const FLOAT3D &vFrom, const FLOAT3D &vTo, FLOAT3D &vHit, CEntity *&penHit) {
  CCastRay cr(penFrom, vFrom, vTo);
  cr.cr_ttHitModels = CCastRay::TT_NONE;
  cr.cr_bHitTranslucentPortals = TRUE; // (a wall's visible face may be one, a layer over the solid one behind)
  IWorld::GetWorld()->CastRay(cr);
  penHit = cr.cr_penHit;
  vHit = cr.cr_vHit;
  return cr.cr_penHit != NULL ? cr.cr_pbpoBrushPolygon : NULL;
};

// Is there surface (the same plane, facing the same way) under the point?
static BOOL OnSurface(CEntity *penFrom, const FLOAT3D &v, const FLOAT3D &vN) {
  FLOAT3D vHit;
  CEntity *penHit = NULL;
  CBrushPolygon *pbpo = CastToWall(penFrom, v + vN * 0.1f, v - vN * 0.15f, vHit, penHit);
  if (pbpo == NULL || (penHit->en_ulPhysicsFlags & EPF_MOVABLE)) return FALSE;
  FLOAT3D vHitN = (const FLOAT3D &)pbpo->bpo_pbplPlane->bpl_plAbsolute;
  vHitN.Normalize();
  return Abs(vHitN % vN) > 0.9f && Abs((vHit - v) % vN) < 0.08f;
};

static BOOL FitOnSurface(CEntity *penFrom, FLOAT3D &vCenter, const FLOAT3D &vN, const FLOAT3D &vU, const FLOAT3D &vR) {
  // offsets along the surface, nearest first
  static FLOAT afOffsets[169][2];
  static INDEX ctOffsets = 0;
  if (ctOffsets == 0) {
    for (INDEX a = -6; a <= 6; a++) for (INDEX b = -6; b <= 6; b++) {
      afOffsets[ctOffsets][0] = a * 0.2f;
      afOffsets[ctOffsets][1] = b * 0.2f;
      ctOffsets++;
    }
    for (INDEX x = 0; x < ctOffsets; x++) for (INDEX y = x + 1; y < ctOffsets; y++) {
      const FLOAT fX = afOffsets[x][0] * afOffsets[x][0] + afOffsets[x][1] * afOffsets[x][1];
      const FLOAT fY = afOffsets[y][0] * afOffsets[y][0] + afOffsets[y][1] * afOffsets[y][1];
      if (fY < fX) {
        Swap(afOffsets[x][0], afOffsets[y][0]);
        Swap(afOffsets[x][1], afOffsets[y][1]);
      }
    }
  }
  // (a wall portal was already fitted between floor and ceiling: it only slides sideways)
  const BOOL bWall = Abs(vN(2)) < 0.7f;
  for (INDEX o = 0; o < ctOffsets; o++) {
    if (bWall && afOffsets[o][1] != 0.0f) continue;
    const FLOAT3D vTry = vCenter + vR * afOffsets[o][0] + vU * afOffsets[o][1];
    BOOL bAll = OnSurface(penFrom, vTry, vN);
    for (INDEX k = 0; k < 8 && bAll; k++) {
      const FLOAT a = k * PI / 4.0f;
      bAll = OnSurface(penFrom, vTry + vR * (cosf(a) * HALF_W * 0.95f) + vU * (sinf(a) * HALF_H * 0.95f), vN);
    }
    if (bAll) {
      vCenter = vTry;
      return TRUE;
    }
  }
  return FALSE;
};

BOOL Shoot(INDEX i, CEntity *penPlayer) {
  if (penPlayer == NULL) return FALSE;
  const CPlacement3D plView = IWorld::GetViewpoint((CPlayerEntity *)penPlayer, TRUE);
  FLOATmatrix3D m;
  MakeRotationMatrix(m, plView.pl_OrientationAngle);
  return ShootAlong(i, penPlayer, FLOAT3D(-m(1, 3), -m(2, 3), -m(3, 3)));
};

BOOL ShootAlong(INDEX i, CEntity *penPlayer, const FLOAT3D &vDirection) {
  i &= 1;
  if (penPlayer == NULL || IWorld::GetWorld() == NULL || vDirection.Length() < 0.001f) return FALSE;
  _ctShots++;
  sounds::Play(i == 0 ? "shoot_blue" : "shoot_orange");

  const FLOAT3D vEye = IWorld::GetViewpoint((CPlayerEntity *)penPlayer, TRUE).pl_PositionVector;
  FLOAT3D vDir = vDirection;
  vDir.Normalize();

  FLOAT3D vHit;
  CEntity *penHit = NULL;
  CBrushPolygon *pbpo = CastToWall(penPlayer, vEye, vEye + vDir * 1000.0f, vHit, penHit);
  // nothing there (the sky), or a door or a lift (it would carry the portal away)
  if (pbpo == NULL || (penHit->en_ulPhysicsFlags & EPF_MOVABLE) || penHit->GetRenderType() != CEntity::RT_BRUSH) {
    sounds::Play("invalid");
    return FALSE;
  }

  FLOAT3D vN = (const FLOAT3D &)pbpo->bpo_pbplPlane->bpl_plAbsolute;
  vN.Normalize();
  if ((vN % vDir) > 0.0f) vN = -vN; // the side facing the shooter

  // up: on a wall the world's up; on a floor or a ceiling, away from the shooter
  FLOAT3D vU;
  if (Abs(vN(2)) < 0.7f) {
    vU = FLOAT3D(0, 1, 0) - vN * vN(2);
  } else {
    vU = vDir - vN * (vDir % vN);
    if (vU.Length() < 0.01f) vU = FLOAT3D(0, 0, -1) - vN * vN(3);
  }
  vU.Normalize();
  FLOAT3D vR = vU * vN;
  vR.Normalize();

  // a wall portal sits on the floor below it and under the ceiling above it, as in Portal
  FLOAT3D vCenter = vHit;
  if (Abs(vN(2)) < 0.7f) {
    FLOAT3D vFloor;
    CEntity *penFloor = NULL;
    const FLOAT3D vOff = vCenter + vN * 0.1f;
    if (CastToWall(penPlayer, vOff, vOff - FLOAT3D(0, HALF_H, 0), vFloor, penFloor) != NULL) {
      vCenter(2) += HALF_H - (vOff(2) - vFloor(2)) + 0.02f;
    } else {
      FLOAT3D vCeiling;
      CEntity *penCeiling = NULL;
      if (CastToWall(penPlayer, vOff, vOff + FLOAT3D(0, HALF_H, 0), vCeiling, penCeiling) != NULL) {
        vCenter(2) -= HALF_H - (vCeiling(2) - vOff(2)) + 0.02f;
      }
    }
  }

  // the whole oval on the surface, as in Portal: moved along it (up to a metre) until its rim is all on it
  if (!FitOnSurface(penPlayer, vCenter, vN, vU, vR)) {
    sounds::Play("invalid");
    return FALSE;
  }

  // not on top of the other one
  const Portal &poOther = _apo[1 - i];
  if (poOther.bOpen && (poOther.vN % vN) > 0.9f && (poOther.vCenter - vCenter).Length() < HALF_W * 2.2f) {
    sounds::Play("invalid");
    return FALSE;
  }

  Portal &po = _apo[i];
  po.bOpen = TRUE;
  po.vCenter = vCenter;
  po.vN = vN;
  po.vU = vU;
  po.vR = vR;
  po.tmOpened = _pTimer->GetLerpedCurrentTick();
  _mapGap[i].clear();
  sounds::Play("open");
  CPrintF("[SeriousPortal] %s portal at (%.2f, %.2f, %.2f), facing (%.2f, %.2f, %.2f)\n", i == 0 ? "blue" : "orange",
    vCenter(1), vCenter(2), vCenter(3), vN(1), vN(2), vN(3));
  return TRUE;
};

FLOAT3D MapVector(INDEX i, const FLOAT3D &v) {
  const Portal &a = _apo[i & 1], &b = _apo[1 - (i & 1)];
  // in through the front of one, out of the front of the other: right and forward turn around, up stays
  return b.vR * -(v % a.vR) + b.vU * (v % a.vU) + b.vN * -(v % a.vN);
};

FLOAT3D MapPoint(INDEX i, const FLOAT3D &p) {
  return _apo[1 - (i & 1)].vCenter + MapVector(i, p - _apo[i & 1].vCenter);
};

FLOATmatrix3D MapRotation(INDEX i, const FLOATmatrix3D &m) {
  FLOATmatrix3D mOut;
  for (INDEX c = 1; c <= 3; c++) {
    const FLOAT3D v = MapVector(i, FLOAT3D(m(1, c), m(2, c), m(3, c)));
    mOut(1, c) = v(1);
    mOut(2, c) = v(2);
    mOut(3, c) = v(3);
  }
  return mOut;
};

// What may go through: the player, monsters, projectiles (rockets, grenades, cannonballs...), items.
static BOOL Traveller(CEntity *pen) {
  if (pen->GetFlags() & ENF_DELETED) return FALSE;
  if (!(pen->en_ulPhysicsFlags & EPF_MOVABLE) || pen->GetRenderType() == CEntity::RT_BRUSH) return FALSE;
  if (pen->IsPredictor()) return FALSE;
  return IsDerivedFromClass(pen, "Player") || IsDerivedFromClass(pen, "Enemy Base") || IsDerivedFromClass(pen, "Projectile")
    || IsDerivedFromClass(pen, "Item");
};

static void Teleport(CEntity *pen, INDEX i, const FLOAT3D &vCenter, const FLOAT3D &vHalf) {
  const Portal &a = _apo[i], &b = _apo[1 - i];
  CMovableEntity *pmen = (CMovableEntity *)pen;
  const FLOAT3D vVelocity = MapVector(i, pmen->en_vCurrentTranslationAbsolute);

  // where it comes out: the same place across the oval (turned), just clear of the other portal's surface
  const FLOAT3D vRel = vCenter - a.vCenter;
  FLOAT r = vRel % a.vR, u = vRel % a.vU;
  const FLOAT fOval = sqrtf((r / HALF_W) * (r / HALF_W) + (u / HALF_H) * (u / HALF_H));
  if (fOval > 0.8f) { r *= 0.8f / fOval; u *= 0.8f / fOval; }
  const FLOAT fExtent = Abs(b.vN(1)) * vHalf(1) + Abs(b.vN(2)) * vHalf(2) + Abs(b.vN(3)) * vHalf(3);
  // (further out than going in counts: 0.3 m against 0.15 m)
  FLOAT3D vOut = b.vCenter + b.vR * -r + b.vU * u + b.vN * (fExtent + 0.3f);
  // out of a wall portal: no lower than the oval's bottom (the floor it sits on)
  if (Abs(b.vN(2)) < 0.7f) {
    const FLOAT fBottom = b.vCenter(2) - HALF_H + 0.05f;
    if (vOut(2) - vHalf(2) < fBottom) vOut(2) = fBottom + vHalf(2);
  }

  CPlacement3D pl = pen->GetPlacement();
  pl.pl_PositionVector += vOut - vCenter;
  FLOATmatrix3D m;
  MakeRotationMatrix(m, pl.pl_OrientationAngle);
  if (IsDerivedFromClass(pen, "Player")) {
    // the player turns only round the vertical (his view's pitch is his own): along where he now faces
    const FLOAT3D vFwd = MapVector(i, FLOAT3D(-m(1, 3), -m(2, 3), -m(3, 3)));
    if (Abs(vFwd(2)) < 0.95f) pl.pl_OrientationAngle(1) = ATan2(-vFwd(1), -vFwd(3));
  } else {
    DecomposeRotationMatrixNoSnap(pl.pl_OrientationAngle, MapRotation(i, m));
  }

  // (the SeriousBridge core's teleport: with the turned velocity)
  if (!physics::PlaceNow(pen, pl, &vVelocity)) return;
  _ctTeleports++;
  if (IsDerivedFromClass(pen, "Player")) {
    sounds::Play("enter");
    CPrintF("[SeriousPortal] through the %s portal: out at (%.2f, %.2f, %.2f), %.1f m/s (%.1f, %.1f, %.1f)\n", i == 0 ? "blue" : "orange",
      pl.pl_PositionVector(1), pl.pl_PositionVector(2), pl.pl_PositionVector(3), vVelocity.Length(), vVelocity(1), vVelocity(2), vVelocity(3));
  } else {
    CPrintF("[SeriousPortal] %s through the %s portal, %.1f m/s\n", (const char *)pen->GetClass()->ec_pdecDLLClass->dec_strName,
      i == 0 ? "blue" : "orange", vVelocity.Length());
  }
};

static BOOL TeleportSEH(CEntity *pen, INDEX i, const FLOAT3D &vCenter, const FLOAT3D &vHalf) {
  __try {
    Teleport(pen, i, vCenter, vHalf);
    return TRUE;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return FALSE;
  }
};

void Step(CEntity *penPlayer) {
  _ctTicks++;
  CWorld *pwo = IWorld::GetWorld();
  if (pwo == NULL || !Linked()) return;

  CDynamicContainer<CEntity> cenGoing;
  CStaticStackArray<INDEX> aiPortal;
  FOREACHINDYNAMICCONTAINER(pwo->wo_cenEntities, CEntity, iten) {
    CEntity *pen = &*iten;
    if (!Traveller(pen)) continue;
    std::map<ULONG, ULONG>::iterator itCool = _mapCooldown.find(pen->en_ulID);
    if (itCool != _mapCooldown.end() && itCool->second > _ctTicks) continue;

    FLOATaabbox3D box;
    pen->GetBoundingBox(box);
    const FLOAT3D vCenter = box.Center();
    const FLOAT3D vHalf = box.Size() * 0.5f;
    const FLOAT3D vVel = ((CMovableEntity *)pen)->en_vCurrentTranslationAbsolute;

    for (INDEX i = 0; i < 2; i++) {
      const Portal &po = _apo[i];
      const FLOAT3D vRel = vCenter - po.vCenter;
      const FLOAT fExtent = Abs(po.vN(1)) * vHalf(1) + Abs(po.vN(2)) * vHalf(2) + Abs(po.vN(3)) * vHalf(3);
      const FLOAT fGap = (vRel % po.vN) - fExtent;
      std::map<ULONG, FLOAT>::iterator itGap = _mapGap[i].find(pen->en_ulID);
      const FLOAT fLastGap = itGap != _mapGap[i].end() ? itGap->second : 1E6f; // (none: not seen coming)
      _mapGap[i][pen->en_ulID] = fGap;
      if (fGap > 0.15f || fGap < -fExtent - 0.5f) continue;

      const FLOAT r = vRel % po.vR, u = vRel % po.vU;
      if ((r / HALF_W) * (r / HALF_W) + (u / HALF_H) * (u / HALF_H) > 1.05f) continue;

      // moving into it, or (a floor portal) standing on it: it falls through
      const BOOL bInto = (vVel % po.vN) < -0.5f || (fLastGap < 1E5f && fLastGap - fGap > 0.02f);
      const BOOL bFloor = po.vN(2) > 0.7f;
      if (!bInto && !bFloor) continue;

      cenGoing.Add(pen);
      aiPortal.Push() = i;
      break;
    }
  }

  // (after the loop: teleporting changes the world's containers)
  for (INDEX iGo = 0; iGo < cenGoing.Count(); iGo++) {
    CEntity *pen = cenGoing.Pointer(iGo);
    const INDEX i = aiPortal[iGo];
    FLOATaabbox3D box;
    pen->GetBoundingBox(box);
    if (!TeleportSEH(pen, i, box.Center(), box.Size() * 0.5f)) {
      CPrintF("[SeriousPortal] moving %s through a portal failed\n", pen->GetClass()->ec_pdecDLLClass->dec_strName);
    }
    _mapCooldown[pen->en_ulID] = _ctTicks + 6; // (coming out, it mustn't count as going into the other)
    _mapGap[0].erase(pen->en_ulID);
    _mapGap[1].erase(pen->en_ulID);
  }

  // forget the gone
  if (_ctTicks % 200 == 0) {
    for (std::map<ULONG, ULONG>::iterator it = _mapCooldown.begin(); it != _mapCooldown.end();) {
      if (it->second + 20 < _ctTicks) _mapCooldown.erase(it++); else ++it;
    }
    if (_mapGap[0].size() > 4096) _mapGap[0].clear();
    if (_mapGap[1].size() > 4096) _mapGap[1].clear();
  }
};

CTString Status(void) {
  CTString str;
  str.PrintF("portals: blue %s, orange %s; %u shots, %u went through", _apo[0].bOpen ? "open" : "closed", _apo[1].bOpen ? "open" : "closed",
    _ctShots, _ctTeleports);
  return str;
};

}; // namespace

/* SeriousPortal plugin precompiled header */
#ifndef SERIOUSPORTAL_STDH_H
#define SERIOUSPORTAL_STDH_H

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif

#include <Core/Core.h>

#include <Engine/Brushes/Brush.h>
#include <Engine/Math/Projection.h>
#include <Engine/Math/Geometry.h>
#include <Engine/Graphics/DrawPort.h>
#include <Engine/World/WorldRayCasting.h>
#include <Engine/Sound/SoundObject.h>

// this port to the SeriousBridge core (core/sam/SBridge.h): its log tag and temp folder (dev hook commands)
#define SB_NAME "SeriousPortal"

#endif

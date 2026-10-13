/* SeriousPortal - Portal's sounds. See Sounds.h. */
#include "StdH.h"
#include "Sounds.h"

#include <string>
#include <map>

namespace sounds {

// one channel per sound, so a shot doesn't cut the opening off
static std::map<std::string, CSoundObject *> _mapChannels;

void Play(const char *strName) {
  const CTFileName fnm = CTString("Sounds\\SeriousPortal\\") + strName + ".wav";
  if (!FileExists(fnm)) return;
  CSoundObject *&pso = _mapChannels[strName];
  if (pso == NULL) pso = new CSoundObject;
  try {
    pso->Play_t(fnm, SOF_NONE);
  } catch (char *strError) {
    CPrintF("[SeriousPortal] sound %s: %s\n", strName, strError);
  }
};

}; // namespace

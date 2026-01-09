#ifndef VIVALDI_PLUS_HOTKEY_H_
#define VIVALDI_PLUS_HOTKEY_H_

#include <windows.h>

namespace bosskey {

// Initialize and register boss key hotkey from config
// This should be called once during application startup
void Initialize();

}  // namespace bosskey

#endif // VIVALDI_PLUS_HOTKEY_H_

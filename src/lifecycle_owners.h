#pragma once
#include <windows.h>
#include <string>
struct LifecycleParticipant { DWORD pid; FILETIME created; };
struct LifecycleSharedState { LifecycleParticipant owners[64]; };
bool LifecycleCreationTime(HANDLE process, FILETIME& created);
size_t CleanLifecycleOwners(LifecycleSharedState& state);
bool HasLifecycleOwner(const LifecycleSharedState& state, const LifecycleParticipant& participant);
bool AddLifecycleOwner(LifecycleSharedState& state, const LifecycleParticipant& participant);
bool OwnsLifecycleProfileWindow();
void AdoptLifecycleWindowOwners(LifecycleSharedState& state, const std::wstring& appDir, DWORD exitingPid);

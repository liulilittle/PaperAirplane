#ifndef NAMESPACESERVICEPROVIDER_H
#define NAMESPACESERVICEPROVIDER_H

#include "Environment.h"

typedef void (WSPAPI* StartNspCompletedEventHandler)(NSP_ROUTINE* sender, NSP_ROUTINE* e);

class NamespaceServiceProvider
{
public:
	NSP_ROUTINE NSP_ROUTINE;
	StartNspCompletedEventHandler StartedNspCompleted;

	int Start(LPGUID lpProviderId, LPNSP_ROUTINE lpnspRoutines);
};
#endif
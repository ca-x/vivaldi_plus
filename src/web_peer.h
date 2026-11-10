#ifndef VIVALDI_PLUS_WEB_PEER_H_
#define VIVALDI_PLUS_WEB_PEER_H_

#include <winsock2.h>

namespace web_http {

bool IsSameUserPeer(SOCKET acceptedSocket);

}

#endif

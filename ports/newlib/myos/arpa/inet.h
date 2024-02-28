#ifndef INET_H
#define INET_H

/*
  needed to port figlet
  https://www.gta.ufrj.br/ensino/eel878/sockets/htonsman.html
  https://www.gta.ufrj.br/ensino/eel878/sockets/
*/
#define htonl(l) ((((l)&0xFF) << 24) | (((l)&0xFF00) << 8) | (((l)&0xFF0000) >> 8) | (((l)&0xFF000000) >> 24))
#define htons(s) ((((s)&0xFF) << 8) | (((s)&0xFF00) >> 8))
#define ntohl(l) htonl((l))
#define ntohs(s) htons((s))

#endif
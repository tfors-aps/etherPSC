#include <stdio.h>
#include <string.h>
#include <osiSock.h>

#define ERROR	-1

int EtherTest() {

    struct sockaddr_in	psAddr, clientAddr;
    int		sock;
    socklen_t	sockAddrSize;
    char	request[32];
    char	response[256];
    int		len, i;

    sock = socket( AF_INET, SOCK_DGRAM, 0 );
    if ( ! sock ) {
	printf("socket failed\n");
        return ( 1 );
    }
    printf( "socket created\n");

    sockAddrSize = sizeof( psAddr );
    memset( &psAddr, 0, sockAddrSize );
    psAddr.sin_family = AF_INET;

    if ( ( psAddr.sin_addr.s_addr = inet_addr("134.79.35.25") ) == ERROR ) {
	printf("inet_addr failed\n");
	return ( 1 );
    }

    psAddr.sin_port = htons( 2000 );

    request[0] = 0xC8;
    request[1] = 0x00;
    request[2] = 0x00;
    request[3] = 0x00;

    if ( sendto( sock, (caddr_t) request, 4, 0,
	(struct sockaddr *) &psAddr, sockAddrSize ) == ERROR ) {
	printf("sendto failed\n");
	perror("sendto");
	close( sock );
	return ( 1 );
    }

    printf("sendto ok\n");

    if ( (len = recvfrom( sock, (caddr_t) response, sizeof(response), 0,
	(struct sockaddr *) &clientAddr, &sockAddrSize ) ) == ERROR ) {
	printf("recvfrom failed\n");
	perror("recvfrom");
	close( sock );
	return( 1 );
    }

    printf("recvfrom ok, len=%d\n", len);

    printf("from %s:%d\n", inet_ntoa(clientAddr.sin_addr), 
	ntohs(clientAddr.sin_port) );

    for ( i = 0; i < len; i++ ) {
	printf("%02x ", response[i]);
    }
    printf("\n\n");

    close( sock );

    return(0);
}

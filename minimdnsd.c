#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <net/if.h>
#include <string.h>
#include <unistd.h>
#include <ifaddrs.h>

// For detecting interfaces going away or coming back.
#include <linux/netlink.h>
#include <linux/rtnetlink.h>

#define MDNS_PORT 5353

struct in_addr localInterface;
struct sockaddr_in groupSock;

int sd;
int sdifaceupdown;

int AddMDNSInterface4( struct in_addr * saddr )
{
	struct ip_mreq mreq;
	mreq.imr_multiaddr.s_addr = inet_addr( "224.0.0.251" );
	memcpy( &mreq.imr_interface, saddr, sizeof( *saddr ) );
	if( setsockopt( sd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&mreq, sizeof(mreq)) == -1)
	{
		char * addr = inet_ntoa( *saddr );
		fprintf( stderr, "WARNING: Could not join membership to %s\n", addr );
		return -1;
	}
	return 0;
}

int IsAddressLocal( struct in_addr * testaddr )
{
	uint32_t check = ntohl( testaddr->s_addr );
	if( ( check & 0xff000000 ) == 0x7f000000 ) return 1; // 127.x.x.x (Link Local, but still want to join)
	if( ( check & 0xff000000 ) == 0x0a000000 ) return 1; // 10.x.x.x
	if( ( check & 0xfff00000 ) == 0xac100000 ) return 1; // 172.[16-31].x.x
	if( ( check & 0xffff0000 ) == 0xc0a80000 ) return 1; // 192.168.x.x
	if( ( check & 0xffff0000 ) == 0xa9fe0000 ) return 1; // 169.254.x.x (RFC5735)
	return 0;
}

int HandleRequestingInterfaces()
{
	static int failcount;
	struct ifaddrs * ifaddr = 0;
	if( getifaddrs( &ifaddr ) == -1 )
	{
		fprintf( stderr, "Error: Could not query devices.\n" );
		return -1;
	}
	else
	{
		failcount = 0;
		for (struct ifaddrs *ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
		{
			struct sockaddr * addr = ifa->ifa_addr;
			int family = addr->sa_family;
			if ( !addr )
			{
				continue;
			}
			if( addr->sa_family == AF_INET )
			{
				char addrbuff[INET_ADDRSTRLEN+10] = { 0 }; // Actually 46 for IPv6, but let's add some buffer.
				struct sockaddr_in * sa4 = (struct sockaddr_in*)addr;
				const char * addrout = inet_ntop( family, &sa4->sin_addr, addrbuff, sizeof( addrbuff ) - 1 );
				int local = IsAddressLocal( &sa4->sin_addr );
				if( !local ) continue;
				printf( "Multicast adding address: %s\n", addrout );
				AddMDNSInterface4( &sa4->sin_addr );
			}
			else if( addr->sa_family == AF_INET6 )
			{
				char addrbuff[INET6_ADDRSTRLEN+10] = { 0 }; // Actually 46 for IPv6, but let's add some buffer.
				struct sockaddr_in6 * sa6 = (struct sockaddr_in6 *)addr;
				const char * addrout = inet_ntop( family, &sa6->sin6_addr, addrbuff, sizeof( addrbuff ) - 1 );
				int local = IN6_IS_ADDR_LINKLOCAL( &sa6->sin6_addr ) || IN6_IS_ADDR_SITELOCAL( &sa6->sin6_addr ) || sa6->sin6_addr.s6_addr[0] == 0xfd;
				if( !local ) continue;
				printf( "LOCAL: %s, but join not written yet\n", addrout );
			}
		}
	}
	return 0;
}

int main (int argc, char *argv[] )
{
	struct sockaddr_in sinsock;
	struct sockaddr_in respsock;

	sd = socket( AF_INET, SOCK_DGRAM, 0 );
	if( sd < 0 )
	{
		perror("Opening datagram socket error");
		exit(1);
	}

	sdifaceupdown = socket( PF_NETLINK, SOCK_RAW, NETLINK_ROUTE );
	if( sdifaceupdown < 0 )
	{
		fprintf( stderr, "WARNING: Couldn't open socket for monitoring address changes.\n");
	}
	else
	{
		// Bind looking for interface changes.
		struct sockaddr_nl addr;
		memset(&addr, 0, sizeof(addr));
		addr.nl_family = AF_NETLINK;
		addr.nl_groups = RTMGRP_IPV4_IFADDR;
		if (bind( sdifaceupdown, (struct sockaddr *)&addr, sizeof(addr)) == -1)
		{
			fprintf( stderr, "WARNING: couldn't bind looking for address changes\n" );
			close( sdifaceupdown );
			sdifaceupdown = -1;
		}
	}

	// Bind the normal MDNS socket, that will get the messages.
	memset( (char *) &sinsock, 0, sizeof( sinsock ) );
	sinsock.sin_family = AF_INET;
	sinsock.sin_addr.s_addr = htonl( INADDR_ANY );
	sinsock.sin_port = htons( MDNS_PORT );

	if( bind( sd, (struct sockaddr *)&sinsock, sizeof(sinsock) ) == -1 )
	{
		fprintf( stderr, "Error: Could not bind to MDNS port\n" );
		exit(1);
	}

	char ** lastiplist = 0;

	// Some things online recommend using IPPROTO_IP, IP_MULTICAST_LOOP
	// But, we can just ignore the replies.

	int r;
	do
	{
		int failcount = 0;
		r = HandleRequestingInterfaces();
		if( r != 0 )
		{
			if( failcount++ > 10 )
			{
				fprintf( stderr, "Too many failures getting interfaces. Aborting\n" );
				return -9;
			}
		}
	} while( r != 0 );


	while( 1 )
	{

/*
		struct if_nameindex *if_nidxs, *intf;
		if_nidxs = if_nameindex();
		if ( if_nidxs != NULL )
		{
			for ( intf = if_nidxs; intf->if_index != 0 || intf->if_name != NULL; intf++ )
			{
				char buff[1024];
				struct ifconf ifc;
				struct ifreq *ifr;
				ifc.ifc_len = sizeof( buff );
				ifc.ifc_buf = buf;
				if( ioctl( socktemp,
				int local = IsAddressLocal( 
				printf( "Joining MDNS on %s\n", intf->if_name);
			}
			if_freenameindex(if_nidxs);
		}
*/
		break;
		sleep( 2 );
	}
	return 0;
}

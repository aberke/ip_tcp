//tcp_utils.h file
#ifndef __TCP_UTILS_H__ 
#define __TCP_UTILS_H__


#include "utils.h"
#include "ip_utils.h" // tcp_packet_data_t and its associated functions defined there

#define TCP_HEADER_MIN_SIZE 20

#define WINDOW_DEFAULT_TIMEOUT 3.0

#define ACCEPT_QUEUE_DEFAULT_SIZE 10

#define DEFAULT_TIMEOUT 12.0
#define DEFAULT_WINDOW_SIZE ((uint16_t)30000)
#define DEFAULT_WINDOW_CHUNK_SIZE 1000
#define RAND_ISN() rand()

/*// a tcp_connection in the listen state queues this triple on its accept_queue when
// it receives a syn.  Nothing further happens until the user calls accept at which point
// this triple is dequeued and a connection is initiated with this information
// the connection should then set its state to listen and go through the LISTEN_to_SYN_RECEIVED transition
struct accept_queue_data{
	uint32_t local_ip;
	uint32_t remote_ip;
	uint16_t remote_port;
	uint32_t last_seq_received;
};*/
typedef struct accept_queue_data* accept_queue_data_t;
accept_queue_data_t accept_queue_data_init(uint32_t local_ip,uint32_t remote_ip,uint16_t remote_port,uint32_t last_seq_received);
void accept_queue_data_destroy(accept_queue_data_t* data);
/* Getting functions for accept_queue_data_t */
uint32_t accept_queue_data_get_local_ip(accept_queue_data_t data);
uint32_t accept_queue_data_get_remote_ip(accept_queue_data_t data);
uint16_t accept_queue_data_get_remote_port(accept_queue_data_t data);
uint32_t accept_queue_data_get_seq(accept_queue_data_t data);


/* tcp_connection_tosend_data_t is what is loaded on and off each tcp_connection's my_to_send queue */
typedef struct tcp_connection_tosend_data* tcp_connection_tosend_data_t;

tcp_connection_tosend_data_t tcp_connection_tosend_data_init(char* to_write, int bytes);
void tcp_connection_tosend_data_destroy(tcp_connection_tosend_data_t to_send);


typedef struct tcp_socket_address{
	uint32_t virt_ip;
	uint16_t virt_port;
} tcp_socket_address_t;


//forward declare memchunk:
struct memchunk;

struct tcphdr* tcp_unwrap_header(void* packet, int length);
struct memchunk* tcp_unwrap_data(void* packet, int length);

/****** For Unwrapping *****/

// defined in terms of SIGNIFICANCE!!
#define FIN_BIT 0
#define SYN_BIT 1
#define RST_BIT 2
#define PSH_BIT 3
#define ACK_BIT 4
#define URG_BIT 5


#if __APPLE__	//(defined __APPLE__ || defined __FAVOR_BSD)// these are the functions that will work for tcphdr defined for mac
	#include <sys/appleapiopts.h>
	#include <sys/_types.h>
	#include <machine/endian.h>
	
	#if !defined(_POSIX_C_SOURCE) || defined(_DARWIN_C_SOURCE)
	typedef	__uint32_t tcp_seq;
	typedef __uint32_t tcp_cc;		/* connection count per rfc1644 */
	
	#define tcp6_seq	tcp_seq	/* for KAME src sync over BSD*'s */
	#define tcp6hdr		tcphdr	/* for KAME src sync over BSD*'s */
	
	/*
	 * TCP header.
	 * Per RFC 793, September, 1981.
	 */
	struct tcphdr {
		unsigned short	th_sport;	/* source port */
		unsigned short	th_dport;	/* destination port */
		tcp_seq	th_seq;			/* sequence number */
		tcp_seq	th_ack;			/* acknowledgement number */
	#if __DARWIN_BYTE_ORDER == __DARWIN_LITTLE_ENDIAN
		unsigned int	th_x2:4,	/* (unused) */
				th_off:4;	/* data offset */
	#endif
	#if __DARWIN_BYTE_ORDER == __DARWIN_BIG_ENDIAN
		unsigned int	th_off:4,	/* data offset */
				th_x2:4;	/* (unused) */
	#endif
		unsigned char	th_flags;
	#define	TH_FIN	0x01
	#define	TH_SYN	0x02
	#define	TH_RST	0x04
	#define	TH_PUSH	0x08
	#define	TH_ACK	0x10
	#define	TH_URG	0x20
	#define	TH_ECE	0x40
	#define	TH_CWR	0x80
	#define	TH_FLAGS	(TH_FIN|TH_SYN|TH_RST|TH_ACK|TH_URG|TH_ECE|TH_CWR)
	
		unsigned short	th_win;		/* window */
		unsigned short	th_sum;		/* checksum */
		unsigned short	th_urp;		/* urgent pointer */
	};
	#endif /* (_POSIX_C_SOURCE && !_DARWIN_C_SOURCE) */

#else // redefine for the two linux versions grrrrr

	#include <features.h>
	#include <sys/types.h>
	#include <sys/socket.h>
	
	#ifdef __FAVOR_BSD
		typedef	u_int32_t tcp_seq;
		/*
		 * TCP header.
		 * Per RFC 793, September, 1981.
		 */
		struct tcphdr
		  {
			u_int16_t th_sport;		/* source port */
			u_int16_t th_dport;		/* destination port */
			tcp_seq th_seq;		/* sequence number */
			tcp_seq th_ack;		/* acknowledgement number */
		#  if __BYTE_ORDER == __LITTLE_ENDIAN
			u_int8_t th_x2:4;		/* (unused) */
			u_int8_t th_off:4;		/* data offset */
		#  endif
		#  if __BYTE_ORDER == __BIG_ENDIAN
			u_int8_t th_off:4;		/* data offset */
			u_int8_t th_x2:4;		/* (unused) */
		#  endif
			u_int8_t th_flags;
		#  define TH_FIN	0x01
		#  define TH_SYN	0x02
		#  define TH_RST	0x04
		#  define TH_PUSH	0x08
		#  define TH_ACK	0x10
		#  define TH_URG	0x20
			u_int16_t th_win;		/* window */
			u_int16_t th_sum;		/* checksum */
			u_int16_t th_urp;		/* urgent pointer */
		};
	# else /* !__FAVOR_BSD */
	
		#define TCP_LINUX_VERSION 1 //our flag for using different getting/setting methods below
	
		struct tcphdr
		  {
			u_int16_t th_sport;		/* source port */	//source
			u_int16_t th_dport;		/* destination port */	//dest
			u_int32_t th_seq;		/* sequence number */	//seq
			u_int32_t th_ack;		/* acknowledgement number */	//seq_ack
		#  if __BYTE_ORDER == __LITTLE_ENDIAN
			u_int16_t res1:4;
			u_int16_t th_off:4;		/* data offset */	//doff:4;
			u_int16_t fin:1;
			u_int16_t syn:1;
			u_int16_t rst:1;
			u_int16_t psh:1;
			u_int16_t ack:1;
			u_int16_t urg:1;
			u_int16_t res2:2;
		#  elif __BYTE_ORDER == __BIG_ENDIAN
			u_int16_t th_off:4;
			u_int16_t res1:4;
			u_int16_t res2:2;
			u_int16_t urg:1;
			u_int16_t ack:1;
			u_int16_t psh:1;
			u_int16_t rst:1;
			u_int16_t syn:1;
			u_int16_t fin:1;
		#  else
		#   error "Adjust your <bits/endian.h> defines"
		#  endif
			u_int16_t th_win;
			u_int16_t th_sum;
			u_int16_t th_urp;
		};
	# endif /* __FAVOR_BSD */
#endif // !__APPLE__


#define tcp_window_size(header) ntohs(((struct tcphdr*)header)->th_win)
#define tcp_ack(header) ntohl(((struct tcphdr*)header)->th_ack)
#define tcp_seqnum(header) ntohl(((struct tcphdr*)header)->th_seq)
#define tcp_dest_port(header) ntohs(((struct tcphdr*)header)->th_dport)
#define tcp_source_port(header) ntohs(((struct tcphdr*)header)->th_sport)
#define tcp_offset_in_bytes(header) ((((struct tcphdr*)header)->th_off)*4) 
#define tcp_checksum(header) (((struct tcphdr*)header)->th_sum)

#ifdef TCP_LINUX_VERSION //instead of bitpacking lets just get/set
	#define tcp_fin_bit(header) (((struct tcphdr*)header)->fin) // is fin set? 
	#define tcp_syn_bit(header) (((struct tcphdr*)header)->syn) // is syn set?
	#define tcp_rst_bit(header) (((struct tcphdr*)header)->rst) // is rst set?
	#define tcp_psh_bit(header) (((struct tcphdr*)header)->psh) // is psh set?  <-- don't need to handle
	#define tcp_ack_bit(header) (((struct tcphdr*)header)->ack) // is ack set?
	#define tcp_urg_bit(header) (((struct tcphdr*)header)->urg) // is urg set?  <-- don't need to handle

#else //using normal mac bitpacking
	#define tcp_fin_bit(header) ((((struct tcphdr*)header)->th_flags & (1 << FIN_BIT)) > 0) // is fin set? 
	#define tcp_syn_bit(header) ((((struct tcphdr*)header)->th_flags & (1 << SYN_BIT)) > 0) // is syn set?
	#define tcp_rst_bit(header) ((((struct tcphdr*)header)->th_flags & (1 << RST_BIT)) > 0) // is rst set?
	#define tcp_psh_bit(header) ((((struct tcphdr*)header)->th_flags & (1 << PSH_BIT)) > 0) // is psh set?  <-- don't need to handle
	#define tcp_ack_bit(header) ((((struct tcphdr*)header)->th_flags & (1 << ACK_BIT)) > 0) // is ack set?
	#define tcp_urg_bit(header) ((((struct tcphdr*)header)->th_flags & (1 << URG_BIT)) > 0) // is urg set?  <-- don't need to handle
#endif

/******** For wrapping *****/
#define tcp_set_window_size(header, size) ((((struct tcphdr*)header)->th_win) = ((uint16_t)htons(size)))
#define tcp_set_ack(header, ack) ((((struct tcphdr*)header)->th_ack) = ((uint32_t)htonl(ack)))
#define tcp_set_seq(header, seq) ((((struct tcphdr*)header)->th_seq) = ((uint32_t)htonl(seq)))
#define tcp_set_offset(header) ((((struct tcphdr*)header)->th_off) = NO_OPTIONS_HEADER_LENGTH)
#define tcp_set_checksum(header, sum) ((((struct tcphdr*)header)->th_sum) = (sum))
#define tcp_set_dest_port(header, port) ((((struct tcphdr*)header)->th_dport) = htons(port))
#define tcp_set_source_port(header, port) ((((struct tcphdr*)header)->th_sport) = htons(port))

#ifdef TCP_LINUX_VERSION //instead of bitpacking lets just get/set
	#define tcp_set_fin_bit(header) (((struct tcphdr*)header)->fin = 1) // set the fin bit to 1
	#define tcp_set_syn_bit(header) (((struct tcphdr*)header)->syn = 1) // set the syn bit to 1
	#define tcp_set_rst_bit(header) (((struct tcphdr*)header)->rst = 1) // set the rst bit to 1
	#define tcp_set_psh_bit(header) (((struct tcphdr*)header)->psh = 1) // set the psh bit to 1
	#define tcp_set_ack_bit(header) (((struct tcphdr*)header)->ack = 1) // set the ack bit to 1
	#define tcp_set_urg_bit(header) (((struct tcphdr*)header)->urg = 1) // set the urg bit to 1

#else //using normal mac bitpacking
	#define tcp_set_fin_bit(header) ((((struct tcphdr*)header)->th_flags) |= (1 << FIN_BIT)) // set the fin bit to 1
	#define tcp_set_syn_bit(header) ((((struct tcphdr*)header)->th_flags) |= (1 << SYN_BIT)) // set the syn bit to 1
	#define tcp_set_rst_bit(header) ((((struct tcphdr*)header)->th_flags) |= (1 << RST_BIT)) // set the rst bit to 1
	#define tcp_set_psh_bit(header) ((((struct tcphdr*)header)->th_flags) |= (1 << PSH_BIT)) // set the psh bit to 1
	#define tcp_set_ack_bit(header) ((((struct tcphdr*)header)->th_flags) |= (1 << ACK_BIT)) // set the ack bit to 1
	#define tcp_set_urg_bit(header) ((((struct tcphdr*)header)->th_flags) |= (1 << URG_BIT)) // set the urg bit to 1
#endif

struct tcphdr* tcp_header_init(int data_size);

// takes in data and wraps data in header with correct addresses.  
// frees parameter data and mallocs new packet  -- sets data to point to new packet
// returns size of new packet that data points to
//int tcp_utils_wrap_packet(void** data, int data_len, tcp_connection_t connection);

// now defined in tcp_connection.c
//int tcp_wrap_packet_send(tcp_connection_t connection, struct tcphdr* header, void* data, int data_len);


uint16_t tcp_utils_calc_checksum(void* packet, uint16_t total_length, uint32_t src_ip, uint32_t dest_ip, uint16_t proto);
void tcp_utils_add_checksum(void* packet, uint16_t  total_length, uint32_t src_ip, uint32_t dest_ip, uint16_t proto);
int tcp_utils_validate_checksum(void* packet, uint16_t total_length, uint32_t src_ip, uint32_t dest_ip, uint16_t proto);


//alex wrote for debugging: prints packet - see tcp_wrap_packet_send
void view_packet(struct tcphdr* header, void* data, int length);

#endif

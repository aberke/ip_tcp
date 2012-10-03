
/*KEEP IN MIND:
"We ask you to design and implement an interface that allows an upper layer to register a handler
for a given protocol number. We’ll leave its speciﬁcs up to you."
*/

/*"When IP packets arrive at their destination, if they aren’t RIP packets, you should
simply print them out in a useful way."*/
void handle_local_packet(link_interface_t li, void* packet, int packet_len){
	//print packet nicely
	// future use will be to hand packet to tcp
}

void handle_selected(link_interface_t li){
	
	//must hand read_packet(link_interface, buffer, buffer_len) a buffer
	char buffer[IP_PACKET_MAX_SIZE];
	memset (buffer, 0, IP_PACKET_MAX_SIZE);
	int bytes_read;
	bytes_read = link_interface_read_packet(li, buffer, IP_PACKET_MAX_SIZE);
	char unwrapped[bytes_read];
	int protocol;
	//protocol = unwrap_ip_packet(buffer, bytes_read, unwrapped);

}
//write wrap_ip_packet //don't have to deal with fragmentation but make sure you don't send more than limit


//write unwrap_ip_packet
/* use:
IP checksum calculation: ipsum.c ipsum.h. Use this function to calculate the checksum in
the IP header for you.
*/
// int is type: RIP vs other  --return -1 if bad packet
// fills unwrapped with unwrapped packet
int unwrap_ip_packet(void* packet, int packet_len, char* unwrapped){
	if(packet_len < sizeof(struct ip)){
		//packet not large enough
		puts("received packet with packet_len < sizeof(struct ip)");
		return -1;
	}

	u_int header_length;
	u_short ip_len, ip_sum;
	u_char protocol;
	struct  in_addr src_ip, dest_ip;
	
	char header[sizeof(struct ip)];
	memcpy(header,  = packet;
	
	struct ip *ip_header = (struct ip *)packet;
	ip_sum = ip_header->ip_sum;
	if(ip_sum != ip_sum(header, sizeof)
	
	ip_len = ip_header->ip_len;
	
	char unwrapped[ip_len];
	unwrapped = buffer[4*(ip_header->ip_hl)];
	
	


	return 0;
}




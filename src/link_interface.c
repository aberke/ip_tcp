//wrapper around link_t structure they give us
// has read_packet function  -- reads from socket
// what it adds to link_t:
	sfd
	doesn't know its ip address
	boolean up_down  -- know whether or not it's down
	
has function send() that takes in void* data  which is a packet constructed by node.c
	-- wraps udp protocol around this ip packet

has function read_packet() 
	returns NULL or ip_packet -- look into having this struct - might be in tools given
	
	
function get_sfd()


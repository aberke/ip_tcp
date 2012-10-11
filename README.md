
where routing_info in form:
uint16_t command;
uint16_t num_entries;
struct {
uint32_t cost;
uint32_t address;
} entries[num_entries];
	

//TODO:

ip_node_t ip_node_init(list_t* links):
	//todo: add error handing for when socket doesn't bind
	



	

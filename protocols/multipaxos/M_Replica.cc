#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <map>

#include "th_assert.h"
#include "M_Replica.h"
#include "M_Message_tags.h"
#include "M_Reply.h"
#include "M_Principal.h"
#include "MD5.h"


#include <sys/mman.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>



#include "M_Certificate.t"
template class M_HighestCounter<M_PrepareResponse>;
template class M_Counter<M_Learn>;

// Global replica object.
M_Replica *M_replica;

#define printDebug(...) \
    do { \
	struct timeval tv; \
	gettimeofday(&tv, NULL); \
	fprintf(stderr, "%u.%06u: ", tv.tv_sec, tv.tv_usec); \
	fprintf(stderr, __VA_ARGS__); \
    } while (0);
#undef printDebug
#define printDebug(...) \
    do { } while (0);

void M_Replica::saveIntoPersistentStorage(M_Accept* m) {
fprintf(persistentStorage, "%*s\n", m->size(), m->contents());
//write(shmfd, (void*)m, sizeof(M_Accept));
fflush(persistentStorage);
}

void M_Replica::saveIntoPersistentStorage(seq_t globaln) {
fprintf(persistentStorage, "%d\n", globaln);
//write(shmfd, (void*)(&globaln), sizeof(seq_t));
fflush(persistentStorage);
}

seq_t getNextSeq();
M_Replica::M_Replica(FILE *config_file, FILE *config_priv, char *host_name, short req_port) :
	M_Node(config_file, config_priv, host_name, req_port), seqno(0)  
{
	// Fail if node is not a replica.
	if (!is_replica(id()))
	{
		th_fail("Node is not a replica");
	}

	M_replica = this;

	replies = new M_Rep_info(num_principals);

	// Read view change, status, and recovery timeouts from replica's portion
	// of "config_file"
	int vt, st, rt;
	fscanf(config_file, "%d\n", &vt);
	fscanf(config_file, "%d\n", &st);
	fscanf(config_file, "%d\n", &rt);

	// Create timers and randomize times to avoid collisions.
	srand48(getpid());


  IamLeader=false;//If I think that I am the leader
  next_uncommited_instance_number_var = 0;
	char FILE_PATH[256];
	sprintf(FILE_PATH, "/tmp/persitantStorage_%d", id());
  persistentStorage = fopen(FILE_PATH, "a+");



	size_t shared_seg_size = 10000;
	char SHMOBJ_PATH[256];
	sprintf(SHMOBJ_PATH, "/persitantStorage_%d", id());
	/* creating the shared memory object    --  shm_open()  */
	shmfd = shm_open(SHMOBJ_PATH, O_CREAT | O_EXCL | O_RDWR, S_IRWXU | S_IRWXG);
	if (shmfd < 0) {
		if (errno != EEXIST)
		{
			perror("In shm_open()");
			exit(1);
		}
		//this time it is OK if it already exists
		shmfd = shm_open(SHMOBJ_PATH, O_CREAT | O_RDWR, S_IRWXU | S_IRWXG);
		if (shmfd < 0)
		{
			perror("In shm_open()");
			exit(1);
		}
	}
	else { //only if it is just created
#ifdef maysam_dbg
		fprintf(stderr, "Created shared memory object %s\n", SHMOBJ_PATH);
#endif
		/* adjusting mapped file size (make room for the whole segment to map) */
		ftruncate(shmfd, shared_seg_size);
	}

	/* requesting the shared segment    --  mmap() */    
	void* pointer = mmap(NULL, shared_seg_size, PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);
	if (pointer == NULL) {
		perror("In mmap()");
		exit(1);
	}


 
  //init
  //TODO: remove it
  //if (id() == 0)
	  //IamLeader = true;


	//join_mcast_group();

	// Disable loopback
	//u_char l = 0;
	//int error = setsockopt(sock, IPPROTO_IP, IP_MULTICAST_LOOP, &l, sizeof(l));
	//if (error < 0)
	//{
		//perror("unable to disable loopback");
		//exit(1);
	//}

   serverListen();
	fprintf(stderr, "Created the task for receiving client requests\n");
}

M_Replica::~M_Replica()
{
	delete replies;
	fclose(persistentStorage);
}

void M_Replica::register_exec(int(*e)(Byz_req *, Byz_rep *, Byz_buffer *, int, bool))
{
	exec_command = e;
}

//#define recv_dbg
//#define maysamdbg
//#define TRACES

void M_Replica::propose(uint64_t index, M_Request *m) {
	if(IamLeader){
		//M_Accept *acc = (M_Accept*) m;
		M_Accept *acc = new M_Accept();
		//acc->convert();//set the tag
		acc->index() = index;
		acc->n() = globaln;
		acc->sender_id() = id();
		acc->client_id() = m->client_id();
		acc->request_id() = m->request_id();
		((M_ForwardedRequest*)m)->n() = globaln;//it is in proposed map
		//process(acc);//msg to self
		M_node->send(acc, M_All_replicas);
	}
	else {
		globaln = getNextSeq();
		M_Prepare prep(index, globaln);
		((M_ForwardedRequest*)m)->n() = globaln;//it is in proposed map
		//process(&prep);//msg to self
		M_node->send(&prep, M_All_replicas);
#ifdef maysamdbg
		fprintf(stderr, "-------- send prepare is done\n");
#endif
	}
} // propose

void M_Replica::handle(M_Request *m, int sock) {
#ifdef maysamdbg
	fprintf(stderr, "===Replica: M_Request: id=%d primary=%d rid=%llu\n", id(), primary(), m->request_id());
#endif
   principals[m->sender_id()]->remotefd = sock;
	processRequest(m);
}

void M_Replica::processRequest(M_Request *m) {
	if (m->is_read_only())
	{
	    fprintf(stderr, "M_Replica::handle(): read-only requests are not handled.\n");
	    delete m;
	    return;
	}

	//int cid = m->client_id();
	//TODO: see if the request is repetetive
	//Request_id rid = m->request_id();

#ifdef TRACES
	int cid = m->client_id();
	Request_id rid = m->request_id();
	fprintf(stderr, "M_Replica::handle() (cid = %d, rid=%llu)\n", cid, rid);
#endif

   th_assert(id() == primary(), "why i am not primary?");
	//TODO: forward it to the primary?

	//forward the requrst to all nodes
	m->sender_id() = id();
	uint64_t index;
	//first sending pending proposals
	index = next_uncommited_instance_number();
	std::map<uint64_t, M_Proposal>::iterator i = proposals.find(index);
	th_assert(i == proposals.end(), "try to set already used index");
	//then its turn for this request
	proposals[index] = M_Proposal(m, num_replicas-1);
	propose(index, m);
	return;
}

void M_Replica::handle(M_Prepare *m, int sock) {
//fprintf(stderr,"pre%d", m->sender_id());
//if (m->sender_id() == id()) return;
	principals[m->sender_id()]->remotefd = sock;
	process(m);
}

void M_Replica::process(M_Prepare *m) {
#ifdef maysamdbg
	fprintf(stderr, "===Replica: M_Prepare: id=%d sender=%d\n", id(), m->sender_id());
#endif

	std::map<uint64_t, M_Accept*>::const_iterator ai = accepted.find(m->index());
	if (ai == accepted.end()) {
		std::map<uint64_t, seq_t>::iterator pi = prepared.find(m->index());
		if (pi == prepared.end() || pi->second < m->n()) {
			if (pi == prepared.end()) 
				prepared[m->index()] = m->n();
			else
				pi->second = m->n();
			M_PrepareResponse prepResp(m->index(), m->n());//no preference (index, reponseN)
			globaln =  m->n();
			M_node->send(&prepResp, m->sender_id());
			saveIntoPersistentStorage(globaln);
		}
		else
		{
			M_Abandon aba(pi->second);
			M_node->send(&aba, m->sender_id());
		}
	}
	else
	{
		std::map<uint64_t, seq_t>::iterator pi = prepared.find(m->index());
		if ((pi == prepared.end() || pi->second < m->n()) && ai->second->n() < m->n()) {
			if (pi == prepared.end()) 
				prepared[m->index()] = m->n();
			else
				pi->second = m->n();
			//M_PrepareResponse *pr = (M_PrepareResponse*) ai->second;
			M_PrepareResponse *pr = new M_PrepareResponse();
			//pr->convert();
			pr->index() = m->index();
			pr->responseN() = m->n();
			pr->sender_id() = id();
			globaln = m->n();//multi-paxos
			M_node->send(pr, m->sender_id());
		}
		else
		{
			M_Abandon aba(highest_proposal_number);
			M_node->send(&aba, m->sender_id());
		}
	}
} // deliver Prepare

void M_Replica::handle(M_PrepareResponse *m, int sock) {
//fprintf(stderr,"preres%d", m->sender_id());
//if (m->sender_id() == id()) return;
   principals[m->sender_id()]->remotefd = sock;
	process(m);
}

void M_Replica::process(M_PrepareResponse *m) {
#ifdef maysamdbg
	fprintf(stderr, "===Replica: M_PrepareResponse: id=%d sender=%d rid=%llu\n", id(), m->sender_id(), m->request_id());
#endif

	std::map<uint64_t, M_Learn*>::const_iterator ci = chosenValues.find(m->index());
	if (ci != chosenValues.end())
	{
	   fprintf(stderr, "Value is already chosen for index %llu\n", m->index());
	   return;
	}
	std::map<uint64_t, M_Proposal>::iterator i = proposals.find(m->index());
	th_assert(i != proposals.end(), "no proposal for the prepare-response msg");
	M_Proposal& p = i->second;

	if (m->responseN() < p.proposed->n())
	{
	   fprintf(stderr, "Old N %d < %d\n", m->responseN(), p.proposed->n());
	   return;
	}
	assert (m->responseN() == p.proposed->n());
	bool isComplete = p.responders.is_complete();
	p.responders.add(m);

	if (!isComplete && p.responders.is_complete()) {
		IamLeader = true;
#ifdef maysamdbg
		std::cout << "=== got PrepareResponse: I am the Leader "  << std::endl;
#endif
		M_Accept *acc = (M_Accept*) p.responders.cvalue(); //the highest
		if (acc == NULL)
			acc = (M_Accept*) p.proposed;
		//TODO: initialize the highest
		//acc->convert();//set the tag
		acc->n() = p.proposed->n();
		acc = new M_Accept();
		acc->client_id() = p.proposed->client_id();
		acc->request_id() = p.proposed->request_id();
		acc->index() = m->index();
		acc->n() = m->responseN();
		acc->sender_id() = id();
		//process(acc);//msg to self
		M_node->send(acc, M_All_replicas);
	}
#ifdef maysamdbg
	else fprintf(stderr,"Not enough PrepareResponse yet\n");
#endif
} // deliver PrepareResponse

void M_Replica::handle(M_Accept *m, int sock) {
//fprintf(stderr,"acc%d", m->sender_id());
	process(m);
}

void M_Replica::process(M_Accept *m) {
#ifdef maysamdbg
	fprintf(stderr, "===Replica: M_Accept: id=%d sender=%d rid=%llu\n", id(), m->sender_id(), m->request_id());
#endif
	std::map<uint64_t, M_Learn*>::const_iterator ci = chosenValues.find(m->index());
	if (ci != chosenValues.end())
	   return;

	std::map<uint64_t, seq_t>::iterator pi = prepared.find(m->index());
	if (pi == prepared.end()) {
		if (globaln != m->n()) {
			fprintf(stderr, "accept without prepared!\n");
			return;
		}
		//saveIntoPersistentStorage(globaln);
		saveIntoPersistentStorage(m);
		//else it is multipaxos
		accepted[m->index()] = m;
		//M_Learn *l = (M_Learn*) m;
		M_Learn *l = new M_Learn();
		//l->convert();//set the tag
		l->client_id() = m->client_id();
		l->request_id() = m->request_id();
		l->index() = m->index();
		l->n() = m->n();
		l->sender_id() = id();
		//process(l);//msg to self
		//M_node->send(l, M_All_replicas);
		M_node->send(l, M_All_replicas);
#ifdef maysamdbg
		fprintf(stderr, "sending learn is done 1\n");
#endif
		return;
	}

	const seq_t& n = pi->second;
	if (m->n() >= n) {
		std::map<uint64_t, M_Accept*>::const_iterator ai = accepted.find(m->index());
		assert(ai == accepted.end() || ai->second->n() < m->n());
		accepted[m->index()] = m;
		//prepared.erase(pi);
		//M_Learn *l = (M_Learn*) m;
		M_Learn *l = new M_Learn();
		//l->convert();//set the tag
		l->client_id() = m->client_id();
		l->request_id() = m->request_id();
		l->index() = m->index();
		l->n() = m->n();
		l->sender_id() = id();
		//process(l);//msg to self
		M_node->send(l, M_All_replicas);
#ifdef maysamdbg
		fprintf(stderr, "sending learn is done 2\n");
#endif
	}
	//else TODO
} // deliver M_Accept

void M_Replica::handle(M_Learn *m, int sock) {
//fprintf(stderr,"l%d", m->sender_id());
//if (m->sender_id() == id()) return;
   principals[m->sender_id()]->remotefd = sock;
	process(m);
}

void M_Replica::process(M_Learn *m) {
#ifdef maysamdbg
	fprintf(stderr, "===Replica: M_Learn: id=%d index=%llu rid=%llu sender=%d\n", id(), m->index(), m->request_id(), m->sender_id());
#endif
	std::map<uint64_t, M_Learn*>::const_iterator ci = chosenValues.find(m->index());
	if (ci != chosenValues.end()) {
#ifdef maysamdbg
	   fprintf(stderr, "Value is already chsoen for learn index %llu", m->index());
#endif
		return;
	}

	std::map<uint64_t, M_Counter<M_Learn> >::iterator li = learnValues.find(m->index());
	if (li == learnValues.end())
	{
		li = learnValues.insert(std::pair<uint64_t, M_Counter<M_Learn> >(m->index(), M_Counter<M_Learn>() )).first;
		M_Counter<M_Learn>& c = li->second;
		c.set_complete(num_replicas-1);
		c.clear();
#ifdef TRACES
		fprintf(stderr, "cleaning the bitmap for index %llu adress %d %d\n", m->index());
#endif
	}
	M_Counter<M_Learn>& l = li->second;
	bool isComplete = l.is_complete();
	int cid = m->client_id();
	int index = m->index();
	l.add(m);//TODO: simple add, check delete
	if (!isComplete && l.is_complete()) {
		chosenValues[index] = m;
		update_next_uncommited_instance_number(index);
#ifdef TRACES
		fprintf(stderr, "Replica::execute_prepared: %d Sending full reply to %d\n", id(), cid);
#endif
		if (IamLeader)//there is already an open connection. client do not listen to new connections
		{
			execute_request(m);
			replies->send_reply(cid, 0, id(), true);
		}
	}
#ifdef maysamdbg
	else fprintf(stderr, "Not enough learn\n");
#endif
} // deliver M_Learn

void M_Replica::handle(M_Abandon *m, int sock) {
   //th_assert(0, "why am I here?");
	//TODO: implement this !!!
}

seq_t M_Replica::getNextSeq() {
	static seq_t r = 0;
	r++;
	r = (r << 8) | id();
	return r;
}

void M_Replica::update_next_uncommited_instance_number(uint64_t commitedInstance) {
	uint64_t next = commitedInstance + 1;
	if (next > next_uncommited_instance_number_var)
		next_uncommited_instance_number_var = next;
	//TODO: what if there is a gap between them
}

uint64_t M_Replica::next_uncommited_instance_number() {
	return next_uncommited_instance_number_var++;
	//TODO: What if I am using UDP and some requests fail
}

bool M_Replica::execute_request(M_Learn *req)
{
#ifdef maysamdbg
	fprintf(stderr, "11 ");
#endif
	int cid = req->client_id();
	Request_id rid = req->request_id();

	// Obtain "in" and "out" buffers to call exec_command
	Byz_req inb;
	Byz_rep outb;
	Byz_buffer non_det;
	inb.contents = req->command(inb.size);
	outb.contents = replies->new_reply(cid, outb.size);
#ifdef maysamdbg
	fprintf(stderr, "1size=%d \n", outb.size);
#endif

	// Execute command in a regular request.
	exec_command(&inb, &outb, &non_det, cid, false);
#ifdef maysamdbg
	fprintf(stderr, "3size=%d \n", outb.size);
#endif

	// Finish constructing the reply.
	replies->end_reply(cid, rid, outb.size);

	return true;
}

void M_Replica::join_mcast_group()
{
exit(1);
	//struct ip_mreq req;
	//req.imr_multiaddr.s_addr = group->address()->sin_addr.s_addr;
	//req.imr_interface.s_addr = INADDR_ANY;
	//int error = setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *) &req,
			//sizeof(req));
	//if (error < 0)
	//{
		//perror("Unable to join group");
		//exit(1);
	//}
}

void M_Replica::leave_mcast_group()
{
exit(1);
	//struct ip_mreq req;
	//req.imr_multiaddr.s_addr = group->address()->sin_addr.s_addr;
	//req.imr_interface.s_addr = INADDR_ANY;
	//int error = setsockopt(sock, IPPROTO_IP, IP_DROP_MEMBERSHIP, (char *) &req,
			//sizeof(req));
	//if (error < 0)
	//{
		//perror("Unable to leave group");
		//exit(1);
	//}
}

void M_Replica::processReceivedMessage(M_Message* msg, int sock) 
{
#ifdef maysamdbg
fprintf(stderr, "=M_Replica::processReceivedMessage tag is %d\n", msg->tag());
#endif
//if (id() == 2) return;
//if (id() == 1 && msg->tag() == M_Learn_tag) return;
//if (id() == 1 && msg->tag() == M_Accept_tag) return;
		switch (msg->tag())
		{
		    case M_Request_tag:
			gen_handle<M_Request>(msg, sock);
			break;
		    case M_Accept_tag:
			gen_handle<M_Accept>(msg, sock);
			break;
		    case M_Learn_tag:
			gen_handle<M_Learn>(msg, sock);
			break;
		    case M_Prepare_tag:
			gen_handle<M_Prepare>(msg, sock);
			break;
		    case M_PrepareResponse_tag:
			gen_handle<M_PrepareResponse>(msg, sock);
			break;
		    case M_Abandon_tag:
			gen_handle<M_Abandon>(msg, sock);
			break;

		    default:
fprintf(stderr, "=M_Replica::processReceivedMessage Error: Unknown msg tag\n");
assert(0);
//exit(1);
			// Unknown message type.
			delete msg;
		}
}
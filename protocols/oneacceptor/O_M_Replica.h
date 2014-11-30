#ifndef _O_M_Replica_h
#define _O_M_Replica_h 1

#include <pthread.h>

//#include "M_Cryptography.h"
#include "types.h"
//#include "Digest.h"
#include "libbyz.h"
#include "Set.h"

#include "Request_history.h"

#include "../multipaxos/M_Node.h"
#include "../multipaxos/M_Rep_info.h"
#include "../multipaxos/M_Certificate.h"
#include "../multipaxos/M_Request.h"
#include "../multipaxos/M_Prepare.h"
#include "../multipaxos/M_Abandon.h"
#include "../multipaxos/M_Accept.h"
#include "../multipaxos/M_Learn.h"
#include "../multipaxos/M_PrepareResponse.h"
#include "O_AcceptedProposals.h"

#include <map>

#define ALIGNMENT_BYTES 2
class M_Proposal
{
	public:
		M_Proposal() {};
		M_Proposal(M_Request* msg, int completeSize) : proposed((M_ForwardedRequest*)msg), responders(completeSize) {
			responders.clear();
		}

		M_ForwardedRequest* proposed;
		M_HighestCounter<M_PrepareResponse> responders;
};


class O_M_Replica : public M_Node
{
	public:

		O_M_Replica(FILE *config_file, FILE *config_priv, char *host_name, short port=0);
		// Effects: Create a new server replica using the information in
		// "config_file".

		virtual ~O_M_Replica();
		// Effects: Kill server replica and deallocate associated storage.

		void register_exec(int (*e)(Byz_req *, Byz_rep *, Byz_buffer *, int, bool));
		// Effects: Registers "e" as the exec_command function.

		template <class T> inline void gen_handle(M_Message *m, int sock)
		{
			T *n;
			if (T::convert(m, n))
			{
				handle(n, sock);
			} else
			{
				//Added by Maysam Yabandeh
				//In non-byzantine work, it must always convert
			   assert(0);
				delete m;
			}
		}

	private:
		void processRequest(M_Request *m);
		void handle(M_Request *m, int sock);
		void handle(M_Accept *m, int sock);
		void handle(M_Learn *m, int sock);
		void handle(M_Prepare *m, int sock);
		void handle(M_PrepareResponse *m, int sock);
		void handle(M_Abandon *m, int sock);

		void process(M_Accept *m);
		void process(M_Learn *m);
		void process(M_Prepare *m);
		void process(M_PrepareResponse *m);
		// Effects: Handles the checkpoint message

		Seqno seqno; // Sequence number to attribute to next protocol message,
			     // we'll use this counter to index messages in history 

		int (*exec_command)(Byz_req *, Byz_rep *, Byz_buffer *, int, bool);

		bool execute_request(M_Learn *m);

		void join_mcast_group();
		// Effects: Enables receipt of messages sent to replica group

		void leave_mcast_group();
		// Effects: Disables receipt of messages sent to replica group

		// Last replies sent to each principal.x
		M_Rep_info *replies;

		// Request history
		//Req_history_log *rh;

		//std::vector<M_Request*> bufferedRequests; //to keep out of order received M_Requests
		void processReceivedMessage(M_Message* msg, int sock);

      std::map<uint64_t, M_Proposal> proposals;
      std::map<uint64_t, M_Accept*> accepted;
      std::map<uint64_t, M_Learn*> chosenValues;
      std::map<uint64_t, M_Counter<M_Learn> > learnValues;
		std::map<uint64_t, seq_t> prepared;
		bool IamLeader;//If I think that I am the leader
		seq_t highest_proposal_number;//the smallest possible value for proposal number
		uint64_t next_uncommited_instance_number_var;
		//
		uint64_t lastIndex;
		uint8_t currentAcceptor, currentLeader;

		void update_next_uncommited_instance_number(uint64_t commitedInstance);
		uint64_t next_uncommited_instance_number();
		void propose(uint64_t index, M_Request *m);
		seq_t globaln;
		seq_t getNextSeq();


//Added by Maysam Yabandeh
public:
void lastActiveAcceptor(int& acceptor, uint64_t& index, AcceptedProposals& lastLeaderUncommiteProposals);
void lastLeader(int& leader, uint64_t& index);
bool proposeLeaderChange(uint64_t index);

};

// Pointer to global replica object.
extern O_M_Replica *O_M_replica;

#endif //_O_M_Replica_h

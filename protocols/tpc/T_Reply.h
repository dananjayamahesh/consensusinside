#ifndef _T_Reply_h
#define _T_Reply_h 1

#include "types.h"
#include "libbyz.h"
#include "T_Message.h"
#include "Digest.h"

class T_Principal;
class T_Rep_info;

//
// T_Reply messages have the following format.
//
struct T_Reply_rep : public T_Message_rep
{
		View v; // current view
		Request_id rid; // unique request identifier
		//Digest rh_digest; // digest of reply.
		int replica; // id of replica sending the reply
		int reply_size; // if negative, reply is not full.
		int cid;
		// Followed by a reply that is "reply_size" bytes long and
		// a MAC authenticating the reply to the client. The MAC is computed
		// only over the Reply_rep. Replies can be empty or full. Full replies
		// contain the actual reply and have reply_size >= 0. Empty replies
		// do not contain the actual reply and have reply_size < 0.
};

class T_Reply : public T_Message
{
		//
		// T_Reply messages
		//
	public:
		T_Reply();

		T_Reply(T_Reply_rep *r);

		T_Reply(View view, Request_id req, int replica, 
		      T_Principal *p, int cid);

		virtual ~T_Reply();

		View view() const;
		// Effects: Fetches the view from the message

		Request_id request_id() const;
		// Effects: Fetches the request identifier from the message.

		int cid() const;
		// Effects: Fetches the identifier of the client who issued the request this message is in response to.

		//Digest &request_history_digest() const;
		// Effects : Fetches the request-history digest from the message

		int& reply_size();
		// Return the reply size

		bool full() const;
		// Effects: Returns true iff "this" is a full reply.

		int id() const;
		// Effects: Fetches the reply identifier from the message.

		void set_id(int id);
		// Effects: Sets the reply identifier to id.

		bool verify();
		// Effects: Verifies if the message is authenticated by rep().replica.

		bool match(T_Reply *r);
		// Effects: Returns true if the replies match.

		static bool convert(T_Message *m1, T_Reply *&m2);
		// Effects: If "m1" has the right size and tag of a "T_Reply", casts
		// "m1" to a "T_Reply" pointer, returns the pointer in "m2" and
		// returns true. Otherwise, it returns false. Convert also trims any
		// surplus storage from "m1" when the conversion is successfull.

		char *reply(int &len);

		char *store_reply(int &max_len);
		   // Effects: Returns a pointer to the location within the message
		   // where the reply should be stored and sets "max_len" to the number of
		   // bytes available to store the reply. The caller can copy any reply
		   // with length less than "max_len" into the returned buffer.
		
	private:
		T_Reply_rep &rep() const;
		// Effects: Casts "msg" to a T_Reply_rep&

		friend class T_Rep_info;
		//Added by Maysam Yabandeh
		//TODO: Clean this dirty imprivization
		//friend class T_Replica;
};

inline T_Reply_rep& T_Reply::rep() const
{
	th_assert(ALIGNED(msg), "Improperly aligned pointer");
	return *((T_Reply_rep*)msg);
}

inline View T_Reply::view() const
{
	return rep().v;
}

inline Request_id T_Reply::request_id() const
{
	return rep().rid;
}

inline int T_Reply::id() const
{
	return rep().replica;
}
inline void T_Reply::set_id(int id)
{
	rep().replica = id;
}
inline int T_Reply::cid() const
{
	return rep().cid;
}

//inline Digest& T_Reply::request_history_digest() const
//{
	//return rep().rh_digest;
//}

inline int& T_Reply::reply_size()
{
	return rep().reply_size;
}

inline bool T_Reply::full() const
{
	return rep().reply_size >= 0;
}

inline bool T_Reply::match(T_Reply *r)
{
	if ((r->reply_size() < 0) || (reply_size() < 0))
	{
		return true;
	}
	bool toRet = ( //(request_history_digest() == r->request_history_digest()) &&
			(rep().v == r->rep().v) && (rep().cid == r->rep().cid) && (rep().rid == r->rep().rid) && ((r->reply_size() < 0) || (reply_size() < 0) ));
	 //fprintf(stderr, "Reply::match [%u,%u,%u,%u]==[%u,%u,%u,%u] && [%u,%u,%u,%u]==[%u,%u,%u,%u]\n", d1.udigest()[0], d1.udigest()[1], d1.udigest()[2], d1.udigest()[3]
	 //, d2.udigest()[0], d2.udigest()[1], d2.udigest()[2], d2.udigest()[3]
	 //, d3.udigest()[0], d3.udigest()[1], d3.udigest()[2], d3.udigest()[3]
	 //, d4.udigest()[0], d4.udigest()[1], d4.udigest()[2], d4.udigest()[3]);
	if (toRet)
	{
		//      fprintf(stderr, "reply_match returning true\n");
		return true;
	}
	//   fprintf(stderr, "reply_match returning false\n");
	return false;
}

#endif // _Reply_h

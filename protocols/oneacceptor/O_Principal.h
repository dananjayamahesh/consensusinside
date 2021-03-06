#ifndef _O_Principal_h
#define _O_Principal_h 1

#include <string.h>
#include <sys/time.h>
#include "types.h"
//#include "O_Cryptography.h"
#include "Traces.h"

//extern "C"
//{
//#include "umac.h"
//}
class O_Node;
//class rabin_pub;

class O_Principal
{
	public:
		O_Principal(int i, int num_principals, Addr a, char *p);
		//Added by Maysam Yabandeh
		O_Principal(int i, int num_principals, Addr a, char* ip, int port, char *p);
		// Effects: Creates a new Principal object.

		virtual ~O_Principal();
		// Effects: Deallocates all the storage associated with principal.

		int pid() const;
		// Effects: Returns the principal identifier.

		const Addr *address() const;
		// Effects: Returns a pointer to the principal's address.


		unsigned int sig_size() const;
		// Effects: Returns the size of signatures generated by this principal.

//Added by Maysam Yabandeh
      int remotefd;
		int port;
		char ip[100];

	private:
		int id;
		Addr addr;
		//rabin_pub *pkey;
		//int ssize;                // signature size

		// UMAC contexts used to generate MACs for incoming and outgoing messages
		//umac_ctx_t* ctxs;

		//static long long umac_nonce;

};

inline const Addr *O_Principal::address() const
{
	return &addr;
}

inline int O_Principal::pid() const
{
	return id;
}

//inline bool O_Principal::verify_mac(const char *src, unsigned src_len,
		//const char *mac)
//{
	//return verify_mac(src, src_len, mac+O_UNonce_size, mac);
//}

//inline void O_Principal::gen_mac(const char *src, unsigned src_len, char *dst,
		//int dest_pid)
//{
	//++umac_nonce;
	//memcpy(dst, (char*)&umac_nonce, O_UNonce_size);
	//dst += O_UNonce_size;
	//gen_mac(src, src_len, dst, dest_pid, (char*)&umac_nonce);
//}

inline unsigned int O_Principal::sig_size() const { return 0; }

#endif // _O_Principal_h

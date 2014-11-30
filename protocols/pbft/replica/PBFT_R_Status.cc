#include <string.h>

#include "th_assert.h"
#include "PBFT_R_Message_tags.h"
#include "PBFT_R_Status.h"
#include "PBFT_R_Node.h"
#include "PBFT_R_Principal.h"
 
PBFT_R_Status::PBFT_R_Status(View v, Seqno ls, Seqno le, bool hnvi, bool hnvm) : 
  PBFT_R_Message(PBFT_R_Status_tag, Max_message_size) {
  rep().extra = (hnvi) ? 1 : 0;
  rep().extra |= (hnvm) ? 2 : 0;
  rep().v = v;        
  rep().ls = ls;
  rep().le = le;
  rep().id = PBFT_R_node->id();
  rep().brsz = 0;

  if (hnvi) {
    // Initialize bitmaps.
    rep().sz = (ls + max_out - le + 7)/8;
    bzero(prepared(), rep().sz);
    bzero(committed(), rep().sz);
  } else {
    bzero(vcs(), PBFT_R_Status_rep::vcs_size);
    rep().sz = 0;
  }
}


void PBFT_R_Status::authenticate() {
  int old_size = sizeof(PBFT_R_Status_rep);
  if (!has_nv_info()) 
    old_size += PBFT_R_Status_rep::vcs_size+rep().sz*sizeof(PP_info);
  else 
    old_size += rep().sz*2+rep().brsz*sizeof(BPBFT_R_info);

  set_size(old_size+PBFT_R_node->auth_size());
  PBFT_R_node->gen_auth_out(contents(), old_size);
}


bool PBFT_R_Status::verify() {
  if (!PBFT_R_node->is_PBFT_R_replica(id()) || id() == PBFT_R_node->id() || view() < 0)
    return false;

  // Check size and authenticator
  int old_size = sizeof(PBFT_R_Status_rep);
  if (!has_nv_info()) 
    old_size += PBFT_R_Status_rep::vcs_size+rep().sz*sizeof(PP_info);
  else 
    old_size += rep().sz*2+rep().brsz*sizeof(BPBFT_R_info);

  if (size() - old_size < PBFT_R_node->auth_size(id()) || 
      !PBFT_R_node->verify_auth_in(id(), contents(), old_size))
    return false;
  
  // Check if message is self consistent
  int diff = rep().le - rep().ls;
  if (diff < 0 || diff > max_out)
    return false;

  if (!has_nv_info()) {
    if (rep().sz < 0 || rep().sz > max_out)
      return false;
  } else {
    if (rep().sz != (max_out-diff+7)/8)
      return false;
  }

  return true;
}


bool PBFT_R_Status::convert(PBFT_R_Message *m1, PBFT_R_Status  *&m2) {
  if (!m1->has_tag(PBFT_R_Status_tag, sizeof(PBFT_R_Status_rep)))
    return false;

  m1->trim();
  m2 = (PBFT_R_Status*)m1;
  return true;
}


void PBFT_R_Status::mark_vcs(int i) {
  th_assert(!has_nv_info(), "Invalid state");
  th_assert(i >= 0 && i < PBFT_R_Status_rep::vcs_size, "Invalid argument");
  Bits_set(vcs(), i);
}


void PBFT_R_Status::append_pps(View v, Seqno n, BR_map mreqs, bool proof) {
  th_assert(!has_nv_info(), "Invalid state");
  th_assert((char*)(pps()+rep().sz) < contents()+Max_message_size, 
	    "PBFT_R_Message too small");

  PP_info& ppi = pps()[rep().sz];
  ppi.n = n-rep().ls;
  ppi.v = v;
  ppi.breqs = mreqs;
  ppi.proof = (proof) ? 1 : 0;
  rep().sz++;
}

  
PBFT_R_Status::PPS_iter::PPS_iter(PBFT_R_Status* m) {
  th_assert(!m->has_nv_info(), "Invalid state");

  msg = m;
  next = 0;
}

	
bool PBFT_R_Status::PPS_iter::get(View& v, Seqno& n, BR_map& mreqs, bool& proof) {
  if (next < msg->rep().sz) {
    PP_info& ppi = msg->pps()[next];
    v = ppi.v;
    n = ppi.n+msg->rep().ls;
    proof = ppi.proof != 0;
    mreqs = ppi.breqs;
    next++;
    return true;
  }

  return false;
}

 
PBFT_R_Status::BRS_iter::BRS_iter(PBFT_R_Status* m) {
  th_assert(m->has_nv_info(), "Invalid state");
  
  msg = m;
  next = 0;
}


bool PBFT_R_Status::BRS_iter::get(Seqno& n, BR_map& mreqs) {
  if (next < msg->rep().brsz) {
    BPBFT_R_info& bri = msg->breqs()[next];
    n = bri.n;
    mreqs = bri.breqs;
    next++;
    return true;
  }
  
  return false;
}
 
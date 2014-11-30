#include <string.h>

#include "th_assert.h"
#include "PBFT_C_Message_tags.h"
#include "PBFT_C_New_view.h"
#include "PBFT_C_Replica.h"
#include "PBFT_C_Principal.h"


PBFT_C_New_view::PBFT_C_New_view(View v) : PBFT_C_Message(PBFT_C_New_view_tag, Max_message_size) {
  rep().v = v;
  rep().min = -1;
  rep().max = -1;

  // Initialize vc_info
  for (int i=0; i < PBFT_C_node->n(); i++) {
    vc_info()[i].d.zero();
  }
}


void  PBFT_C_New_view::add_view_change(int id, Digest &d) {
  th_assert(PBFT_C_node->is_replica(id), "Not a replica");
  th_assert(vc_info()[id].d == Digest(), "Duplicate");

  VC_info& vci =  vc_info()[id];
  vci.d = d;
}


void PBFT_C_New_view::set_min(Seqno min) {
  th_assert(rep().min == -1, "Invalid state");
  rep().min = min;
}

void PBFT_C_New_view::set_max(Seqno max) {
  th_assert(min() >= 0, "Invalid state");
  rep().max = max;
  th_assert(max >= min() && max-min() <= max_out+1, "Invalid arguments");
}

void PBFT_C_New_view::pick(int id, Seqno n) {
  th_assert(min() >= 0, "Invalid state");
  th_assert(PBFT_C_node->is_replica(id), "Not a replica");
  th_assert(vc_info()[id].d != Digest(), "Invalid argument");
  th_assert(n >= min() && n <= min()+max_out, "Invalid argument");

  picked()[n-min()] = id;
}


void PBFT_C_New_view::re_authenticate(PBFT_C_Principal *p) {
  int old_size = sizeof(PBFT_C_New_view_rep)+sizeof(VC_info)*PBFT_C_node->n()+max()-min();

  // Compute authenticator and update size.
  th_assert(Max_message_size-old_size >= PBFT_C_node->auth_size(), "PBFT_C_Message is too small");
  set_size(old_size+PBFT_C_node->auth_size());
  PBFT_C_node->gen_auth_out(contents(), old_size, contents()+old_size);
  trim();
}


bool PBFT_C_New_view::view_change(int id, Digest& d) {
  if (id < 0  || id >= PBFT_C_node->n())
    return false;

  VC_info& vci = vc_info()[id];
  if (vci.d.is_zero())
    return false;

  d = vci.d;

  return true;
}


bool PBFT_C_New_view::verify() {
  if (view() <= 0 || min() < 0 || max() < 0 || max() < min() || max()-min() > max_out+1)
    return false;

  // Check that each entry in picked is set to the identifier of a replica
  // whose view-change digest is in this.
  for (Seqno i = min(); i < max(); i++) {
    int vci = picked()[i-min()];
    if (!PBFT_C_node->is_replica(vci) || vc_info()[vci].d.is_zero())
      return false;
  }

  int old_size = sizeof(PBFT_C_New_view_rep) + sizeof(VC_info)*PBFT_C_node->n()+max()-min();
  if (Max_message_size-old_size < PBFT_C_node->auth_size(id()))
    return false;

  // Check authenticator
  if (!PBFT_C_node->verify_auth_in(id(), contents(), old_size,  contents()+old_size))
    return false;

  return true;
}


bool PBFT_C_New_view::convert(PBFT_C_Message *m1, PBFT_C_New_view  *&m2) {
  if (!m1->has_tag(PBFT_C_New_view_tag, sizeof(PBFT_C_New_view_rep)))
    return false;

  m1->trim();
  m2 = (PBFT_C_New_view*)m1;
  return true;
}
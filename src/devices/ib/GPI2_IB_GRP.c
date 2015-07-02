/*
Copyright (c) Fraunhofer ITWM - Carsten Lojewski <lojewski@itwm.fhg.de>, 2013-2015

This file is part of GPI-2.

GPI-2 is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License
version 3 as published by the Free Software Foundation.

GPI-2 is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GPI-2. If not, see <http://www.gnu.org/licenses/>.
*/
#include <sys/mman.h>
#include <sys/timeb.h>
#include <unistd.h>
#include "GPI2.h"
#include "GASPI.h"
#include "GPI2_Coll.h"
#include "GPI2_IB.h"
#include "GPI2_SN.h"


/* Group utilities */
int
pgaspi_dev_poll_groups()
{
  int i;
  
  const int pret = ibv_poll_cq (glb_gaspi_ctx_ib.scqGroups, glb_gaspi_ctx.ne_count_grp,glb_gaspi_ctx_ib.wc_grp_send);
  
  if (pret < 0)
    {
      for (i = 0; i < glb_gaspi_ctx.ne_count_grp; i++)
	{
	  if (glb_gaspi_ctx_ib.wc_grp_send[i].status != IBV_WC_SUCCESS)
	    {
	      glb_gaspi_ctx.qp_state_vec[GASPI_COLL_QP][glb_gaspi_ctx_ib.wc_grp_send[i].wr_id] = GASPI_STATE_CORRUPT;
	    }
	}

      gaspi_print_error("Failed request to %lu. Collectives queue might be broken",
			glb_gaspi_ctx_ib.wc_grp_send[i].wr_id);
      return GASPI_ERROR;
    }

  return pret;
}

int
pgaspi_dev_post_group_write(void *local_addr, int length, int dst, void *remote_addr, unsigned char group)
{
  
  struct ibv_sge slist;
  struct ibv_send_wr swr;
  struct ibv_send_wr *bad_wr_send;

  slist.addr = (uintptr_t) local_addr;
  slist.length = length;
  slist.lkey = ((struct ibv_mr *)glb_gaspi_group_ctx[group].rrcd[glb_gaspi_ctx.rank].mr)->lkey;

  swr.sg_list = &slist;
  swr.num_sge = 1;
  swr.opcode = IBV_WR_RDMA_WRITE;
  swr.send_flags = (length == 1) ? (IBV_SEND_SIGNALED | IBV_SEND_INLINE) : IBV_SEND_SIGNALED;
  swr.next = NULL;

  swr.wr.rdma.remote_addr = (uint64_t) remote_addr;
  swr.wr.rdma.rkey = glb_gaspi_group_ctx[group].rrcd[dst].rkey;
  swr.wr_id = dst;
  
  if (ibv_post_send ((struct ibv_qp *) glb_gaspi_ctx_ib.qpGroups[dst], &swr, &bad_wr_send))
    {
      glb_gaspi_ctx.qp_state_vec[GASPI_COLL_QP][dst] = GASPI_STATE_CORRUPT;
      return 1;
    }

  return 0;
}

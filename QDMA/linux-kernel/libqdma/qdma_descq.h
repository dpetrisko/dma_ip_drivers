/*
 * This file is part of the Xilinx DMA IP Core driver for Linux
 *
 * Copyright (c) 2017-2019,  Xilinx, Inc.
 * All rights reserved.
 *
 * This source code is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * The full GNU General Public License is included in this distribution in
 * the file called "COPYING".
 */

#ifndef __QDMA_DESCQ_H__
#define __QDMA_DESCQ_H__
/**
 * @file
 * @brief This file contains the declarations for qdma descq processing
 *
 */
#include <linux/spinlock_types.h>
#include <linux/types.h>
#include "qdma_compat.h"
#include "libqdma_export.h"
#include "qdma_regs.h"
#ifdef ERR_DEBUG
#include "qdma_nl.h"
#endif
#include "qdma_ul_ext.h"


enum q_state_t {
	/** Queue is not taken */
	Q_STATE_DISABLED = 0,
	/** Assigned/taken. Partial config is done */
	Q_STATE_ENABLED,
	/** Resource/context is initialized for the queue and is available for
	 *  data consumption
	 */
	Q_STATE_ONLINE,
};

#define QDMA_FLQ_SIZE 80

/**
 * @struct - qdma_descq
 * @brief	qdma software descriptor book keeping fields
 */
struct qdma_descq {
	/** qdma queue configuration */
	struct qdma_queue_conf conf;
	/** lock to protect access to software descriptor */
	spinlock_t lock;
	/** pointer to dma device */
	struct xlnx_dma_dev *xdev;
	/** number of channels */
	u8 channel;
	/** flag to indicate error on the Q, in halted state */
	u8 err:1;
	/** color bit for the queue */
	u8 color:1;
	/** cpu attached */
	u8 cpu_assigned:1;
	/** state of the proc req */
	u8 proc_req_running;
	/** Indicate q state */
	enum q_state_t q_state;
	/** hw qidx associated for this queue */
	unsigned int qidx_hw;
	/** cpu attached to intr_work */
	unsigned int intr_work_cpu;
	/** @q_hndl: Q handle */
	unsigned long q_hndl;
	/** queue handler */
	struct work_struct work;
	/** interrupt list */
	struct list_head intr_list;
	/** leagcy interrupt list */
	struct list_head legacy_intr_q_list;
	/** interrupt id associated for this queue */
	int intr_id;
	/** work  list for the queue */
	struct list_head work_list;
	/** write back therad list */
	struct qdma_kthread *cmplthp;
	/** completion status thread list for the queue */
	struct list_head cmplthp_list;
	/** pending qork thread list */
	struct list_head pend_list;
	/** wait queue for pending list clear */
	qdma_wait_queue pend_list_wq;
	/** pending list empty count */
	unsigned int pend_list_empty;
	/* flag to indicate wwaiting for transfers to complete before q stop*/
	unsigned int q_stop_wait;
	/** availed count */
	unsigned int avail;
	/** current req count */
	unsigned int pend_req_desc;
	/** current producer index */
	unsigned int pidx;
	/** current consumer index */
	unsigned int cidx;
	/** number of descrtors yet to be processed*/
	unsigned int credit;
	/** desctor to be processed*/
	u8 *desc;
	/** desctor dma address*/
	dma_addr_t desc_bus;
	/** desctor writeback*/
	u8 *desc_cmpl_status;

	/* ST C2H */
	/** programming order of the data in ST c2h mode*/
	unsigned char fl_pg_order;
	/** cmpt entry length*/
	unsigned char cmpt_entry_len;
	/** 2 bits reserved*/
	unsigned char rsvd[2];
	/** qdma free list q*/
	unsigned char flq[QDMA_FLQ_SIZE];
	/**total # of udd outstanding */
	unsigned int udd_cnt;
	/** packet count/number of packets to be processed*/
	unsigned int pkt_cnt;
	/** packet data length */
	unsigned int pkt_dlen;
	/** pidx of the completion entry */
	unsigned int pidx_cmpt;
	/** completion cidx */
	unsigned int cidx_cmpt;
	/** number of packets processed in q */
	unsigned long long total_cmpl_descs;
	/** @mm_cmpt_ring_crtd: CMPT ring created for MM */
	u8 mm_cmpt_ring_crtd;
	/** descriptor writeback, data type depends on the cmpt_entry_len */
	void *desc_cmpt_cur;
	/* descriptor list to be provided for ul extenstion call */
	struct qdma_q_desc_list *desc_list;
	/** pointer to completion entry */
	u8 *desc_cmpt;
	/** descriptor dma bus address*/
	dma_addr_t desc_cmpt_bus;
	/** descriptor writeback dma bus address*/
	u8 *desc_cmpt_cmpl_status;
	/** pidx info to be written to PIDX regiser*/
	struct qdma_q_pidx_reg_info pidx_info;
	/** cmpt cidx info to be written to CMPT CIDX regiser*/
	struct qdma_q_cmpt_cidx_reg_info cmpt_cidx_info;

#ifdef ERR_DEBUG
	/** flag to indicate error inducing */
	u64 induce_err;
	u64 ecc_err;
#endif
#ifdef DEBUGFS
	/** debugfs queue index root */
	struct dentry *dbgfs_qidx_root;
	/** debugfs queue root */
	struct dentry *dbgfs_queue_root;
	/** debugfs cmpt queue root */
	struct dentry *dbgfs_cmpt_queue_root;
#endif
};
#ifdef DEBUG_THREADS
#define lock_descq(descq)	\
	do { \
		pr_debug("locking descq %s ...\n", (descq)->conf.name); \
		spin_lock_bh(&(descq)->lock); \
	} while (0)

#define unlock_descq(descq) \
	do { \
		pr_debug("unlock descq %s ...\n", (descq)->conf.name); \
		spin_unlock_bh(&(descq)->lock); \
	} while (0)
#else
/** macro to lock descq */
#define lock_descq(descq)	spin_lock_bh(&(descq)->lock)
/** macro to un lock descq */
#define unlock_descq(descq)	spin_unlock_bh(&(descq)->lock)
#endif

static inline unsigned int ring_idx_delta(unsigned int new, unsigned int old,
					unsigned int rngsz)
{
	return new >= old ? (new - old) : new + (rngsz - old);
}

static inline unsigned int ring_idx_incr(unsigned int idx, unsigned int cnt,
					unsigned int rngsz)
{
	idx += cnt;
	return idx >= rngsz ? idx - rngsz : idx;
}

static inline unsigned int ring_idx_decr(unsigned int idx, unsigned int cnt,
					unsigned int rngsz)
{
	return idx >= cnt ?  idx - cnt : rngsz - (cnt - idx);
}

/*****************************************************************************/
/**
 * qdma_descq_init() - initialize the sw descq entry
 *
 * @param[in]	descq:		pointer to qdma_descq
 * @param[in]	xdev:		pointer to xdev
 * @param[in]	idx_hw:		hw queue index
 * @param[in]	idx_sw:		sw queue index
 *
 * @return	none
 *****************************************************************************/
void qdma_descq_init(struct qdma_descq *descq, struct xlnx_dma_dev *xdev,
			int idx_hw, int idx_sw);

/*****************************************************************************/
/**
 * qdma_descq_config() - configure the sw descq entry
 *
 * @param[in]	descq:		pointer to qdma_descq
 * @param[in]	qconf:		queue configuration
 * @param[in]	reconfig:	flag to indicate whether to refig the sw descq
 *
 * @return	none
 *****************************************************************************/
void qdma_descq_config(struct qdma_descq *descq, struct qdma_queue_conf *qconf,
		 int reconfig);

/*****************************************************************************/
/**
 * qdma_descq_config_complete() - initialize the descq with default values
 *
 * @param[in]	descq:		pointer to qdma_descq
 *
 * @return	0: success
 * @return	<0: failure
 *****************************************************************************/
int qdma_descq_config_complete(struct qdma_descq *descq);

/*****************************************************************************/
/**
 * qdma_descq_alloc_resource() - allocate the resources for a request
 *
 * @param[in]	descq:		pointer to qdma_descq
 *
 * @return	0: success
 * @return	<0: failure
 *****************************************************************************/
int qdma_descq_alloc_resource(struct qdma_descq *descq);

/*****************************************************************************/
/**
 * qdma_descq_free_resource() - free up the resources assigned to a request
 *
 * @param[in]	descq:		pointer to qdma_descq
 *
 * @return	none
 *****************************************************************************/
void qdma_descq_free_resource(struct qdma_descq *descq);

/*****************************************************************************/
/**
 * qdma_descq_prog_hw() - program the hw descriptors
 *
 * @param[in]	descq:		pointer to qdma_descq
 *
 * @return	0: success
 * @return	<0: failure
 *****************************************************************************/
int qdma_descq_prog_hw(struct qdma_descq *descq);

/*****************************************************************************/
/**
 * qdma_descq_context_cleanup() - clean up the queue context
 *
 * @param[in]	descq:		pointer to qdma_descq
 *
 * @return	0: success
 * @return	<0: failure
 *****************************************************************************/
int qdma_descq_context_cleanup(struct qdma_descq *descq);

/*****************************************************************************/
/**
 * qdma_descq_service_cmpl_update() - process completion data for the request
 *
 * @param[in]	descq:		pointer to qdma_descq
 * @param[in]	budget:		number of descriptors to process
 * @param[in]	c2h_upd_cmpl:	C2H only: if update completion needed
 *
 * @return	none
 *****************************************************************************/
void qdma_descq_service_cmpl_update(struct qdma_descq *descq, int budget,
			bool c2h_upd_cmpl);

/*****************************************************************************/
/**
 * qdma_descq_dump() - dump the queue sw desciptor data
 *
 * @param[in]	descq:		pointer to qdma_descq
 * @param[in]	detail:		indicate whether full details or abstact details
 * @param[in]	buflen:		length of the input buffer
 * @param[out]	buf:		message buffer
 *
 * @return	0: success
 * @return	<0: failure
 *****************************************************************************/
int qdma_descq_dump(struct qdma_descq *descq,
					char *buf, int buflen, int detail);

/*****************************************************************************/
/**
 * qdma_descq_dump_desc() - dump the queue hw descriptors
 *
 * @param[in]	descq:		pointer to qdma_descq
 * @param[in]	start:		start descriptor index
 * @param[in]	end:		end descriptor index
 * @param[in]	buflen:		length of the input buffer
 * @param[out]	buf:		message buffer
 *
 * @return	0: success
 * @return	<0: failure
 *****************************************************************************/
int qdma_descq_dump_desc(struct qdma_descq *descq, int start, int end,
		char *buf, int buflen);

/*****************************************************************************/
/**
 * qdma_descq_dump_state() - dump the queue desciptor state
 *
 * @param[in]	descq:		pointer to qdma_descq
 * @param[in]	buflen:		length of the input buffer
 * @param[out]	buf:		message buffer
 *
 * @return	0: success
 * @return	<0: failure
 *****************************************************************************/
int qdma_descq_dump_state(struct qdma_descq *descq, char *buf, int buflen);

/*****************************************************************************/
/**
 * intr_cidx_update() - update the interrupt cidx
 *
 * @param[in]	descq:		pointer to qdma_descq
 * @param[in]	sw_cidx:	sw cidx
 * @param[in]	ring_index:	ring index
 *
 *****************************************************************************/
void intr_cidx_update(struct qdma_descq *descq, unsigned int sw_cidx,
		      int ring_index);

/*****************************************************************************/
/**
 * incr_cmpl_desc_cnt() - update the interrupt cidx
 *
 * @param[in]   descq:      pointer to qdma_descq
 *
 *****************************************************************************/
void incr_cmpl_desc_cnt(struct qdma_descq *descq, unsigned int cnt);

/**
 * @struct - qdma_sgt_req_cb
 * @brief	qdma_sgt_req_cb fits in qdma_request.opaque
 */
struct qdma_sgt_req_cb {
	/** qdma read/write request list*/
	struct list_head list;
	/** request wait queue */
	qdma_wait_queue wq;
	/** number of descriptors to proccess*/
	unsigned int desc_nr;
	/** offset in the page*/
	unsigned int offset;
	/** offset in the scatter gather list*/
	unsigned int sg_offset;
	/** next sg to be processed */
	void *sg;
	/** scatter gather ebtry index*/
	unsigned int sg_idx;
	/** number of data byte not yet proccessed*/
	unsigned int left;
	/** status of the request*/
	int status;
	/** indicates whether request processing is done or not*/
	u8 done;
	/** indicates whether to unmap the kernel pages*/
	u8 unmap_needed:1;
};

/** macro to get the request call back data */
#define qdma_req_cb_get(req)	(struct qdma_sgt_req_cb *)((req)->opaque)

/*****************************************************************************/
/**
 * qdma_descq_proc_sgt_request() - handler to process the qdma
 *				read/write request
 *
 * @param[in]	descq:	pointer to qdma_descq
 * @param[in]	cb:		scatter gather list call back data
 *
 * @return	size of the request
 *****************************************************************************/
ssize_t qdma_descq_proc_sgt_request(struct qdma_descq *descq);

/*****************************************************************************/
/**
 * qdma_sgt_req_done() - handler to track the progress of the request
 *
 * @param[in]	descq:	pointer to qdma_descq
 * @param[in]	cb:		scatter gather list call back data
 * @param[out]	error:	indicates the error status
 *
 * @return	none
 *****************************************************************************/
void qdma_sgt_req_done(struct qdma_descq *descq, struct qdma_sgt_req_cb *cb,
			int error);

/*****************************************************************************/
/**
 * sgl_map() - handler to map the scatter gather list to kernel pages
 *
 * @param[in]	pdev:	pointer to struct pci_dev
 * @param[in]	sg:		scatter gather list
 * @param[in]	sgcnt:	number of entries in scatter gather list
 * @param[in]	dir:	direction of the request
 *
 * @return	none
 *****************************************************************************/
int sgl_map(struct pci_dev *pdev, struct qdma_sw_sg *sg, unsigned int sgcnt,
		enum dma_data_direction dir);

/*****************************************************************************/
/**
 * sgl_unmap() - handler to unmap the scatter gather list and free
 *				the kernel pages
 *
 * @param[in]	pdev:	pointer to struct pci_dev
 * @param[in]	sg:		scatter gather list
 * @param[in]	sgcnt:	number of entries in scatter gather list
 * @param[in]	dir:	direction of the request
 *
 * @return	none
 *****************************************************************************/
void sgl_unmap(struct pci_dev *pdev, struct qdma_sw_sg *sg, unsigned int sgcnt,
		enum dma_data_direction dir);

/*****************************************************************************/
/**
 * descq_flq_free_resource() - handler to free the pages for the request
 *
 * @param[in]	descq:		pointer to qdma_descq
 *
 * @return	none
 *****************************************************************************/
void descq_flq_free_resource(struct qdma_descq *descq);

int rcv_udd_only(struct qdma_descq *descq, struct qdma_ul_cmpt_info *cmpl);
int parse_cmpl_entry(struct qdma_descq *descq, struct qdma_ul_cmpt_info *cmpl);
void cmpt_next(struct qdma_descq *descq);

/* CIDX/PIDX update macros */
#ifndef __QDMA_VF__
#define queue_pidx_update(xdev, qid, is_c2h, pidx_info) \
	qdma_queue_pidx_update(xdev, QDMA_DEV_PF, qid, is_c2h, pidx_info)
#else
#define queue_pidx_update(xdev, qid, is_c2h, pidx_info) \
	qdma_queue_pidx_update(xdev, QDMA_DEV_VF, qid, is_c2h, pidx_info)
#endif

#ifndef __QDMA_VF__
#define queue_cmpt_cidx_update(xdev, qid, cmpt_cidx_info) \
	qdma_queue_cmpt_cidx_update(xdev, QDMA_DEV_PF, qid, cmpt_cidx_info)
#else
#define queue_cmpt_cidx_update(xdev, qid, cmpt_cidx_info) \
	qdma_queue_cmpt_cidx_update(xdev, QDMA_DEV_VF, qid, cmpt_cidx_info)
#endif

#ifndef __QDMA_VF__
#define queue_cmpt_cidx_read(xdev, qid, cmpt_cidx_info) \
	qdma_queue_cmpt_cidx_read(xdev, QDMA_DEV_PF, qid, cmpt_cidx_info)
#else
#define queue_cmpt_cidx_read(xdev, qid, cmpt_cidx_info) \
	qdma_queue_cmpt_cidx_read(xdev, QDMA_DEV_VF, qid, cmpt_cidx_info)
#endif

#ifndef __QDMA_VF__
#define queue_intr_cidx_update(xdev, qid, intr_cidx_info) \
	qdma_queue_intr_cidx_update(xdev, QDMA_DEV_PF, qid, intr_cidx_info)
#else
#define queue_intr_cidx_update(xdev, qid, intr_cidx_info) \
	qdma_queue_intr_cidx_update(xdev, QDMA_DEV_VF, qid, intr_cidx_info)
#endif


#endif /* ifndef __QDMA_DESCQ_H__ */

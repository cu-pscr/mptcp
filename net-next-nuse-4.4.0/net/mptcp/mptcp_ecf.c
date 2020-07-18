/* MPTCP Scheduler module selector. Highly inspired by tcp_cong.c */

#include <linux/module.h>
#include <net/mptcp.h>
#include <net/tcp.h>


struct sock *sandy_fast=NULL,*sandy_slow=NULL;
static DEFINE_SPINLOCK(mptcp_sched_list_lock);
static LIST_HEAD(mptcp_sched_list);

static unsigned int r_beta __read_mostly = 4; // beta = 1/r_beta = 0.25
module_param(r_beta, int, 0644);
MODULE_PARM_DESC(r_beta, "beta for ECF");
int switching_margin=0;
struct defsched_priv {
	u32	last_rbuf_opti;
};

static struct defsched_priv *defsched_get_priv(const struct tcp_sock *tp)
{
	return (struct defsched_priv *)&tp->mptcp->mptcp_sched[0];
}
bool ecf_is_def_unavailable(struct sock *sk)
{
        const struct tcp_sock *tp = tcp_sk(sk);
        struct inet_sock *inet= inet_sk(sk);
        unsigned long long int ipaddr=0;//sandy lines
        int window=0;

        /* Set of states for which we are allowed to send data */
        if (!mptcp_sk_can_send(sk)){
                //printk("*************** MPTCP is defenetly not avaialbale **************");
                return true;

        }

        /* We do not send data on this subflow unless it is
         * fully established, i.e. the 4th ack has been received.
         */
        if (tp->mptcp->pre_established)
                return true;

        if (tp->pf)
                return true;

        return false;
}


static bool ecf_is_temp_unavailable(struct sock *sk,
                                      const struct sk_buff *skb,
                                      bool zero_wnd_test)
{
        const struct tcp_sock *tp = tcp_sk(sk);
        unsigned int mss_now, space, in_flight;

        if (inet_csk(sk)->icsk_ca_state == TCP_CA_Loss) {
                /* If SACK is disabled, and we got a loss, TCP does not exit
                 * the loss-state until something above high_seq has been
                 * acked. (see tcp_try_undo_recovery)
                 *
                 * high_seq is the snd_nxt at the moment of the RTO. As soon
                 * as we have an RTO, we won't push data on the subflow.
                 * Thus, snd_una can never go beyond high_seq.
                 */
                if (!tcp_is_reno(tp))
                        return true; 
                else if (tp->snd_una != tp->high_seq)
                        return true;
        }

        if (!tp->mptcp->fully_established) {
                /* Make sure that we send in-order data */
                if (skb && tp->mptcp->second_packet &&
                    tp->mptcp->last_end_data_seq != TCP_SKB_CB(skb)->seq)
                        return true;
        }

        /* If TSQ is already throttling us, do not send on this subflow. When
         * TSQ gets cleared the subflow becomes eligible again.
         */
        if (test_bit(TSQ_THROTTLED, &tp->tsq_flags))
                return true;

        in_flight = tcp_packets_in_flight(tp);
        /* Not even a single spot in the cwnd */
        if (in_flight >= tp->snd_cwnd)
                return true;

        /* Now, check if what is queued in the subflow's send-queue
         * already fills the cwnd.
         */
        space = (tp->snd_cwnd - in_flight) * tp->mss_cache;

        if (tp->write_seq - tp->snd_nxt > space)
                return true;

        if (zero_wnd_test && !before(tp->write_seq, tcp_wnd_end(tp)))
                return true;

        mss_now = tcp_current_mss(sk);

        /* Don't send on this subflow if we bypass the allowed send-window at
         * the per-subflow level. Similar to tcp_snd_wnd_test, but manually
         * calculated end_seq (because here at this point end_seq is still at
         * the meta-level).
         */
        if (skb && !zero_wnd_test &&
            after(tp->write_seq + min(skb->len, mss_now), tcp_wnd_end(tp)))
                return true;
                
        return false;
}

/* Is the sub-socket sk available to send the skb? */
bool ecf_mptcp_is_available(struct sock *sk, const struct sk_buff *skb,
			bool zero_wnd_test)
{
	//printk("sandy: %s Check if MPTCP is available\n",__func__);
	return !ecf_is_def_unavailable(sk) &&
	       !ecf_is_temp_unavailable(sk, skb, zero_wnd_test);
}

/* Are we not allowed to reinject this skb on tp? */
static int mptcp_dont_reinject_skb(const struct tcp_sock *tp, const struct sk_buff *skb)
{
	/* If the skb has already been enqueued in this sk, try to find
	 * another one.
	 */
	return skb &&
		/* Has the skb already been enqueued into this subsocket? */
		mptcp_pi_to_flag(tp->mptcp->path_index) & TCP_SKB_CB(skb)->path_mask;
}

bool ecf_subflow_is_backup(const struct tcp_sock *tp)
{
	return tp->mptcp->rcv_low_prio || tp->mptcp->low_prio;
}

bool ecf_subflow_is_active(const struct tcp_sock *tp)
{
	return !tp->mptcp->rcv_low_prio && !tp->mptcp->low_prio;
}

/* Generic function to iterate over used and unused subflows and to select the
 * best one
 */
static struct sock
*get_subflow_from_selectors(struct mptcp_cb *mpcb, struct sk_buff *skb,
			    bool (*selector)(const struct tcp_sock *),
			    bool zero_wnd_test, bool *force)
{
	struct sock *bestsk = NULL;
	u32 min_srtt = 0xffffffff;
	bool found_unused = false;
	bool found_unused_una = false;
	struct sock *sk;
	//printk("sandy: %s getting subflow from the selected ones\n",__func__);
	mptcp_for_each_sk(mpcb, sk) {
		struct tcp_sock *tp = tcp_sk(sk);
		bool unused = false;

		/* First, we choose only the wanted sks */
		if (!(*selector)(tp))
			continue;

		if (!mptcp_dont_reinject_skb(tp, skb))
			unused = true;
		else if (found_unused)
			/* If a unused sk was found previously, we continue -
			 * no need to check used sks anymore.
			 */
			continue;

		if (ecf_is_def_unavailable(sk))
			continue;

		if (ecf_is_temp_unavailable(sk, skb, zero_wnd_test)) {
			if (unused)
				found_unused_una = true;
			continue;
		}

		if (unused) {
			if (!found_unused) {
				/* It's the first time we encounter an unused
				 * sk - thus we reset the bestsk (which might
				 * have been set to a used sk).
				 */
				min_srtt = 0xffffffff;
				bestsk = NULL;
			}
			found_unused = true;
		}
		if (tp->srtt_us < min_srtt) {
			min_srtt = tp->srtt_us;
			bestsk = sk;
		}
	}

	if (bestsk) {
		/* The force variable is used to mark the returned sk as
		 * previously used or not-used.
		 */
		if (found_unused)
			*force = true;
		else
			*force = false;
	} else {
		/* The force variable is used to mark if there are temporally
		 * unavailable not-used sks.
		 */
		if (found_unused_una)
			*force = true;
		else
			*force = false;
	}
	/*if(bestsk!=NULL && inet_sk(bestsk)!=NULL){
		printk("%s: %pI4:%d -> %pI4:%d The path returned is\n",
						    __func__ , 
						    &((struct inet_sock *) bestsk )->inet_saddr,
						    ntohs(((struct inet_sock *) bestsk )->inet_sport),
						    &((struct inet_sock *) bestsk )->inet_daddr,
						    ntohs(((struct inet_sock *) bestsk )->inet_dport));

	}*/
	return bestsk;
}

/* This is the scheduler. This function decides on which flow to send
 * a given MSS. If all subflows are found to be busy, NULL is returned
 * The flow is selected based on the shortest RTT.
 * If all paths have full cong windows, we simply return NULL.
 *
 * Additionally, this function is aware of the backup-subflows.
 */
struct sock *ecf_get_available_subflow(struct sock *meta_sk, struct sk_buff *skb,
				   bool zero_wnd_test)
{
	struct mptcp_cb *mpcb = tcp_sk(meta_sk)->mpcb;
	struct sock *sk,*sandy_sk;
	u32 sub_sndbuf = 0;
	u32 sub_packets_out = 0;
	struct tcp_sock *tp;
	struct inet_sock *inet=NULL;	
	u32 min_rtt = 0xffffffff;
	unsigned long long int ipaddr=0;
	bool force;
	struct defsched_priv *dsp =defsched_get_priv(tcp_sk(meta_sk));
	if(dsp==NULL){
		printk("********************** Check the scheduler sandy for ECF ************");
	}
	//printk("sandy:%s get the suflow available\n",__func__);
	/* if there is only one subflow, bypass the scheduling function */
	if (mpcb->cnt_subflows == 1) {
		sk = (struct sock *)mpcb->connection_list;
		if (!ecf_mptcp_is_available(sk, skb, zero_wnd_test))
			sk = NULL;
		return sk;
	}

	/* Answer data_fin on same subflow!!! */
	if (meta_sk->sk_shutdown & RCV_SHUTDOWN &&
	    skb && mptcp_is_data_fin(skb)) {
		mptcp_for_each_sk(mpcb, sk) {
			if (tcp_sk(sk)->mptcp->path_index == mpcb->dfin_path_index &&
			    ecf_mptcp_is_available(sk, skb, zero_wnd_test))
				return sk;
		}
	}

	/* Find the best subflow */
	sk = get_subflow_from_selectors(mpcb, skb, &ecf_subflow_is_active,
					zero_wnd_test, &force);
	//Find the fastest path
	sandy_sk=NULL;
	mptcp_for_each_sk(tcp_sk(meta_sk)->mpcb, sandy_sk) {
		tp = tcp_sk(sandy_sk);
		sub_sndbuf += ( (u32) sandy_sk->sk_wmem_queued );
		sub_packets_out += tp->packets_out;
		if(tp->srtt_us < min_rtt && sandy_sk!=meta_sk) {
			sandy_fast = sandy_sk;
			min_rtt = tp->srtt_us;
		}
	}
	//Find the slow path
	sandy_sk=NULL;
	mptcp_for_each_sk(tcp_sk(meta_sk)->mpcb, sandy_sk) {
                if(sandy_fast!=sandy_sk && sandy_sk!=meta_sk) {
                        sandy_slow = sandy_sk;
                }
        }


	if (force){
		/* one unused active sk or one NULL sk when there is at least
		 * one temporally unavailable unused active sk
		 */
		if(sk==sandy_fast){
			if(inet_sk(sk)!=NULL && sk!=NULL){
				printk("%s: %pI4:%d -> %pI4:%d The Fast path \n",
                                                    __func__ ,
                                                    &((struct inet_sock *) sk )->inet_saddr,
                                                    ntohs(((struct inet_sock *) sk )->inet_sport),
                                                    &((struct inet_sock *) sk )->inet_daddr,
                                                    ntohs(((struct inet_sock *) sk )->inet_dport));
			}
			return sk;
		}else if(sk==sandy_slow){
			if(inet_sk(sk)!=NULL && sk!=NULL){
				printk("%s: %pI4:%d -> %pI4:%d The slow path \n",
                                                    __func__ ,
                                                    &((struct inet_sock *) sk )->inet_saddr,
                                                    ntohs(((struct inet_sock *)sk )->inet_sport),
                                                    &((struct inet_sock *)sk )->inet_daddr,
                                                    ntohs(((struct inet_sock *)sk )->inet_dport));
				
			}
			u32 mss=( (u32) tcp_current_mss(sandy_fast) );
			u32 sndbuf_meta = ( (u32) meta_sk->sk_wmem_queued );
			u32 sndbuf_minus = sub_sndbuf +  
							(tcp_sk(meta_sk)->packets_out > sub_packets_out ? tcp_sk(meta_sk)->packets_out - sub_packets_out : 0) * mss;
			u32 sndbuf = sndbuf_meta > sndbuf_minus ? sndbuf_meta - sndbuf_minus : 0; // can be smaller? anyway for safety

			u32 cwnd_f = max( tcp_sk(sandy_fast)->snd_cwnd, tcp_sk(sandy_fast)->snd_cwnd_before_idle_restart );
			u32 srtt_f = tcp_sk(sandy_fast)->srtt_us>> 3;
			u32 rttvar_f = tcp_sk(sandy_fast)->rttvar_us>> 1;

			u32 cwnd_s = max( tcp_sk(sk)->snd_cwnd, tcp_sk(sk)->snd_cwnd_before_idle_restart );
			u32 srtt_s = tcp_sk(sk)->srtt_us >> 3;
			u32 rttvar_s = tcp_sk(sk)->rttvar_us >> 1;
			u32 delta=max(rttvar_f,rttvar_s);
			u64 lhs, rhs;
                        u32 x_f = sndbuf > cwnd_f * mss ? sndbuf : cwnd_f * mss;
                        lhs = srtt_f * (x_f + cwnd_f * mss);
                        rhs =  cwnd_f * mss * (srtt_s + delta);

                        if( r_beta * lhs < r_beta * rhs + switching_margin * rhs ) {
                        	u32 x_s = sndbuf > cwnd_s * mss ? sndbuf : cwnd_s * mss;
                                u64 lhs_s = srtt_s * x_s;
                                u64 rhs_s = cwnd_s * mss * (2 * srtt_f + delta);
                                if( lhs_s < rhs_s ) {
	                                if(tcp_sk(sk)->snd_cwnd > tcp_sk(sk)->snd_cwnd_before_idle_restart) {
        	                                tcp_sk(sk)->snd_cwnd_before_idle_restart = 0;
                                        } 
                                } else {
                                	switching_margin = 1;
					return NULL;
                                }
                        } else {
                                        if(tcp_sk(sk)->snd_cwnd > tcp_sk(sk)->snd_cwnd_before_idle_restart) {
                                                tcp_sk(sk)->snd_cwnd_before_idle_restart = 0;
                                        }
                                        switching_margin = 0;
                        }

                        return sk;
		}
	}

	sk = get_subflow_from_selectors(mpcb, skb, &ecf_subflow_is_backup,
					zero_wnd_test, &force);
	if (!force && skb)
		/* one used backup sk or one NULL sk where there is no one
		 * temporally unavailable unused backup sk
		 *
		 * the skb passed through all the available active and backups
		 * sks, so clean the path mask
		 */
		TCP_SKB_CB(skb)->path_mask = 0;
	return sk;
}

static struct sk_buff *mptcp_rcv_buf_optimization(struct sock *sk, int penal)
{
	struct sock *meta_sk;
	const struct tcp_sock *tp = tcp_sk(sk);
	struct tcp_sock *tp_it;
	struct sk_buff *skb_head;
	struct defsched_priv *dsp = defsched_get_priv(tp);
	if (tp->mpcb->cnt_subflows == 1)
		return NULL;

	meta_sk = mptcp_meta_sk(sk);
	skb_head = tcp_write_queue_head(meta_sk);

	if (!skb_head || skb_head == tcp_send_head(meta_sk))
		return NULL;


	//penal=0;
	/* If penalization is optional (coming from mptcp_next_segment() and
	 * We are not send-buffer-limited we do not penalize. The retransmission
	 * is just an optimization to fix the idle-time due to the delay before
	 * we wake up the application.
	 */
	if (!penal && sk_stream_memory_free(meta_sk))
		goto retrans;

	/* Only penalize again after an RTT has elapsed */
	if (tcp_time_stamp - dsp->last_rbuf_opti < usecs_to_jiffies(tp->srtt_us >> 3))
		goto retrans;

	/* Half the cwnd of the slow flow */
	mptcp_for_each_tp(tp->mpcb, tp_it) {
		if (tp_it != tp &&
		    TCP_SKB_CB(skb_head)->path_mask & mptcp_pi_to_flag(tp_it->mptcp->path_index)) {
			if (tp->srtt_us < tp_it->srtt_us && inet_csk((struct sock *)tp_it)->icsk_ca_state == TCP_CA_Open) {
				u32 prior_cwnd = tp_it->snd_cwnd;
				
				printk("sandy:%s penalization is enabled\n",__func__);
				tp_it->snd_cwnd = max(tp_it->snd_cwnd >> 1U, 1U);

				/* If in slow start, do not reduce the ssthresh */
				if (prior_cwnd >= tp_it->snd_ssthresh)
					tp_it->snd_ssthresh = max(tp_it->snd_ssthresh >> 1U, 2U);

				dsp->last_rbuf_opti = tcp_time_stamp;
			}
			break;
		}
	}

retrans:

	/* Segment not yet injected into this path? Take it!!! */
	if (!(TCP_SKB_CB(skb_head)->path_mask & mptcp_pi_to_flag(tp->mptcp->path_index))) {
		bool do_retrans = false;
		mptcp_for_each_tp(tp->mpcb, tp_it) {
			if (tp_it != tp &&
			    TCP_SKB_CB(skb_head)->path_mask & mptcp_pi_to_flag(tp_it->mptcp->path_index)) {
				if (tp_it->snd_cwnd <= 4) {
					do_retrans = true;
					break;
				}

				if (4 * tp->srtt_us >= tp_it->srtt_us) {
					do_retrans = false;
					break;
				} else {
					do_retrans = true;
				}
			}
		}

		if (do_retrans && ecf_mptcp_is_available(sk, skb_head, false))
			return skb_head;
	}
	return NULL;
}

/* Returns the next segment to be sent from the mptcp meta-queue.
 * (chooses the reinject queue if any segment is waiting in it, otherwise,
 * chooses the normal write queue).
 * Sets *@reinject to 1 if the returned segment comes from the
 * reinject queue. Sets it to 0 if it is the regular send-head of the meta-sk,
 * and sets it to -1 if it is a meta-level retransmission to optimize the
 * receive-buffer.
 */
static struct sk_buff *__mptcp_next_segment(struct sock *meta_sk, int *reinject)
{
	const struct mptcp_cb *mpcb = tcp_sk(meta_sk)->mpcb;
	struct sk_buff *skb = NULL;

	*reinject = 0;

	/* If we are in fallback-mode, just take from the meta-send-queue */
	if (mpcb->infinite_mapping_snd || mpcb->send_infinite_mapping)
		return tcp_send_head(meta_sk);

	skb = skb_peek(&mpcb->reinject_queue);

	if (skb) {
		*reinject = 1;
	} else {
		skb = tcp_send_head(meta_sk);

		if (!skb && meta_sk->sk_socket &&
		    test_bit(SOCK_NOSPACE, &meta_sk->sk_socket->flags) &&
		    sk_stream_wspace(meta_sk) < sk_stream_min_wspace(meta_sk)) {
			struct sock *subsk = ecf_get_available_subflow(meta_sk, NULL,
								   false);
			if (!subsk)
				return NULL;

			skb = mptcp_rcv_buf_optimization(subsk, 0);
			if (skb)
				*reinject = -1;
		}
	}
	return skb;
}

static struct sk_buff *mptcp_next_segment(struct sock *meta_sk,
					  int *reinject,
					  struct sock **subsk,
					  unsigned int *limit)
{
	//printk("sandy:%s next segment\n",__func__);
	struct sk_buff *skb = __mptcp_next_segment(meta_sk, reinject);
	unsigned int mss_now;
	struct tcp_sock *subtp;
	u16 gso_max_segs;
	u32 max_len, max_segs, window, needed;

	/* As we set it, we have to reset it as well. */
	*limit = 0;

	if (!skb)
		return NULL;

	*subsk = ecf_get_available_subflow(meta_sk, skb, false);
	if (!*subsk)
		return NULL;

	subtp = tcp_sk(*subsk);
	mss_now = tcp_current_mss(*subsk);

	if (!*reinject && unlikely(!tcp_snd_wnd_test(tcp_sk(meta_sk), skb, mss_now))) {
		skb = mptcp_rcv_buf_optimization(*subsk, 1);
		if (skb)
			*reinject = -1;
		else
			return NULL;
	}

	/* No splitting required, as we will only send one single segment */
	if (skb->len <= mss_now)
		return skb;

	/* The following is similar to tcp_mss_split_point, but
	 * we do not care about nagle, because we will anyways
	 * use TCP_NAGLE_PUSH, which overrides this.
	 *
	 * So, we first limit according to the cwnd/gso-size and then according
	 * to the subflow's window.
	 */

	gso_max_segs = (*subsk)->sk_gso_max_segs;
	if (!gso_max_segs) /* No gso supported on the subflow's NIC */
		gso_max_segs = 1;
	max_segs = min_t(unsigned int, tcp_cwnd_test(subtp, skb), gso_max_segs);
	if (!max_segs)
		return NULL;

	max_len = mss_now * max_segs;
	window = tcp_wnd_end(subtp) - subtp->write_seq;

	needed = min(skb->len, window);
	if (max_len <= skb->len)
		/* Take max_win, which is actually the cwnd/gso-size */
		*limit = max_len;
	else
		/* Or, take the window */
		*limit = needed;

	return skb;
}


static void defsched_init(struct sock *sk)
{
        struct defsched_priv *dsp = defsched_get_priv(tcp_sk(sk));
	dsp->last_rbuf_opti = tcp_time_stamp;
	switching_margin = 0;
}

static struct mptcp_sched_ops mptcp_sched_rr = {
        .get_subflow = ecf_get_available_subflow,
        .next_segment = mptcp_next_segment,
        .name = "ecf",
	.init = defsched_init,
        .owner = THIS_MODULE,
};

static int __init ecf_register(void)
{
        BUILD_BUG_ON(sizeof(struct defsched_priv) > MPTCP_SCHED_SIZE);

        if (mptcp_register_scheduler(&mptcp_sched_rr))
                return -1;

        return 0;
}

static void ecf_unregister(void)
{
        mptcp_unregister_scheduler(&mptcp_sched_rr);
}

module_init(ecf_register);
module_exit(ecf_unregister);

MODULE_AUTHOR("sandy");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ECF MPTCP");
MODULE_VERSION("0.92");


/*
 * Copyright (c) 2013 Juniper Networks, Inc. All rights reserved.
 */

#include <net/if.h>

#include <uve/agent_uve_stats.h>
#include <vrouter/stats_collector/agent_stats_sandesh_context.h>
#include <vrouter/stats_collector/agent_stats_collector.h>
#include <cmn/agent_stats.h>
#include <oper/vrf.h>
#include <vrouter_types.h>

AgentStatsSandeshContext::AgentStatsSandeshContext(Agent *agent)
    : agent_(agent), marker_id_(-1) {
    AgentUveStats *uve = static_cast<AgentUveStats *>(agent_->uve());
    stats_ = uve->stats_manager();
}

AgentStatsSandeshContext::~AgentStatsSandeshContext() {
}

bool AgentStatsSandeshContext::MoreData() const {
    return (response_code_ & VR_MESSAGE_DUMP_INCOMPLETE);
}

int AgentStatsSandeshContext::VrResponseMsgHandler(vr_response *r) {
    int code = r->get_resp_code();

    set_response_code(code);
    if (code > 0) {
       /* Positive value indicates the number of records returned in the
        * response from Kernel. Kernel response includes vr_response along
        * with actual response.
        */
        return 0;
    }

    if (code < 0) {
        LOG(ERROR, "Error in reading Statistics from vrouter: " <<
            KSyncEntry::VrouterErrorToString(-code));
        return -code;
    }

    return 0;
}

void AgentStatsSandeshContext::IfMsgHandler(vr_interface_req *req) {
    set_marker_id(req->get_vifr_idx());
    const Interface *intf = InterfaceTable::GetInstance()->FindInterface
                                                    (req->get_vifr_idx());
    if (intf == NULL) {
        return;
     }

    StatsManager::InterfaceStats *stats =stats_->GetInterfaceStats(intf);

    if (!stats) {
        return;
    }
    if (intf->type() == Interface::VM_INTERFACE) {
        agent_->stats()->incr_in_pkts(req->get_vifr_ipackets() -
                                      stats->in_pkts);
        agent_->stats()->incr_in_bytes(req->get_vifr_ibytes() -
                                       stats->in_bytes);
        agent_->stats()->incr_out_pkts(req->get_vifr_opackets() -
                                       stats->out_pkts);
        agent_->stats()->incr_out_bytes(req->get_vifr_obytes() -
                                        stats->out_bytes);
    }

    stats->UpdateStats(req->get_vifr_ibytes(), req->get_vifr_ipackets(),
                       req->get_vifr_obytes(), req->get_vifr_opackets());
    stats->speed = req->get_vifr_speed();
    stats->duplexity = req->get_vifr_duplex();
}

void AgentStatsSandeshContext::VrfStatsMsgHandler(vr_vrf_stats_req *req) {
    set_marker_id(req->get_vsr_vrf());
    bool vrf_present = true;
    const VrfEntry *vrf = agent_->vrf_table()->
                          FindVrfFromId(req->get_vsr_vrf());
    if (vrf == NULL) {
        if (req->get_vsr_vrf() != stats_->GetNamelessVrfId()) {
            vrf_present = false;
        } else {
            return;
        }
     }

    StatsManager::VrfStats *stats = stats_->GetVrfStats(req->get_vsr_vrf());
    if (!stats) {
        LOG(DEBUG, "Vrf not present in stats tree <" << req->get_vsr_vrf()
            << ">");
        return;
    }
    if (!vrf_present) {
        stats->prev_discards = req->get_vsr_discards();
        stats->prev_resolves = req->get_vsr_resolves();
        stats->prev_receives = req->get_vsr_receives();
        stats->prev_udp_tunnels = req->get_vsr_udp_tunnels();
        stats->prev_udp_mpls_tunnels = req->get_vsr_udp_mpls_tunnels();
        stats->prev_gre_mpls_tunnels = req->get_vsr_gre_mpls_tunnels();
        stats->prev_ecmp_composites = req->get_vsr_ecmp_composites();
        stats->prev_l2_mcast_composites = req->get_vsr_l2_mcast_composites();
        stats->prev_fabric_composites = req->get_vsr_fabric_composites();
        stats->prev_encaps = req->get_vsr_encaps();
        stats->prev_l2_encaps = req->get_vsr_l2_encaps();

        stats->prev_gros = req->get_vsr_gros();
        stats->prev_diags = req->get_vsr_diags();
        stats->prev_encap_composites = req->get_vsr_encap_composites();
        stats->prev_evpn_composites = req->get_vsr_evpn_composites();
        stats->prev_vrf_translates = req->get_vsr_vrf_translates();
        stats->prev_vxlan_tunnels = req->get_vsr_vxlan_tunnels();
        stats->prev_arp_virtual_proxy = req->get_vsr_arp_virtual_proxy();
        stats->prev_arp_virtual_stitch = req->get_vsr_arp_virtual_stitch();
        stats->prev_arp_virtual_flood = req->get_vsr_arp_virtual_flood();
        stats->prev_arp_physical_stitch = req->get_vsr_arp_physical_stitch();
        stats->prev_arp_tor_proxy = req->get_vsr_arp_tor_proxy();
        stats->prev_arp_physical_flood = req->get_vsr_arp_physical_flood();
        stats->prev_l2_receives = req->get_vsr_l2_receives();
        stats->prev_uuc_floods = req->get_vsr_uuc_floods();
    } else {
        stats->discards = req->get_vsr_discards() - stats->prev_discards;
        stats->resolves = req->get_vsr_resolves() - stats->prev_resolves;
        stats->receives = req->get_vsr_receives() - stats->prev_receives;
        stats->udp_tunnels = req->get_vsr_udp_tunnels() -
                             stats->prev_udp_tunnels;
        stats->gre_mpls_tunnels = req->get_vsr_gre_mpls_tunnels() -
                                  stats->prev_gre_mpls_tunnels;
        stats->udp_mpls_tunnels = req->get_vsr_udp_mpls_tunnels() -
                                  stats->prev_udp_mpls_tunnels;
        stats->encaps = req->get_vsr_encaps() - stats->prev_encaps;
        stats->l2_encaps = req->get_vsr_l2_encaps() - stats->prev_l2_encaps;
        stats->ecmp_composites = req->get_vsr_ecmp_composites() -
                                 stats->prev_ecmp_composites;
        stats->l2_mcast_composites = req->get_vsr_l2_mcast_composites() -
                                     stats->prev_l2_mcast_composites;
        stats->fabric_composites = req->get_vsr_fabric_composites() - 
                                   stats->prev_fabric_composites;

        stats->gros = req->get_vsr_gros() - stats->prev_gros;
        stats->diags = req->get_vsr_diags() - stats->prev_diags;
        stats->encap_composites = req->get_vsr_encap_composites() -
                                  stats->prev_encap_composites;
        stats->evpn_composites = req->get_vsr_evpn_composites() -
                                 stats->prev_evpn_composites;
        stats->vrf_translates = req->get_vsr_vrf_translates() -
                                stats->prev_vrf_translates;
        stats->vxlan_tunnels = req->get_vsr_vxlan_tunnels() -
                               stats->prev_vxlan_tunnels;
        stats->arp_virtual_proxy = req->get_vsr_arp_virtual_proxy() -
                                   stats->prev_arp_virtual_proxy;
        stats->arp_virtual_stitch = req->get_vsr_arp_virtual_stitch() -
                                    stats->prev_arp_virtual_stitch;
        stats->arp_virtual_flood = req->get_vsr_arp_virtual_flood() -
                                   stats->prev_arp_virtual_flood;
        stats->arp_physical_stitch = req->get_vsr_arp_physical_stitch() -
                                     stats->prev_arp_physical_stitch;
        stats->arp_tor_proxy = req->get_vsr_arp_tor_proxy() -
                               stats->prev_arp_tor_proxy;
        stats->arp_physical_flood = req->get_vsr_arp_physical_flood() -
                                    stats->prev_arp_physical_flood;
        stats->l2_receives = req->get_vsr_l2_receives() -
                             stats->prev_l2_receives;
        stats->uuc_floods = req->get_vsr_uuc_floods() -
                            stats->prev_uuc_floods;
        /* Update the last read values from Kernel in the following fields.
         * This will be used to update prev_* fields on receiving vrf delete
         * notification
         */
        if (req->get_vsr_vrf() != stats_->GetNamelessVrfId()) {
            stats->k_discards = req->get_vsr_discards();
            stats->k_resolves = req->get_vsr_resolves();
            stats->k_receives = req->get_vsr_receives();
            stats->k_udp_tunnels = req->get_vsr_udp_tunnels();
            stats->k_gre_mpls_tunnels = req->get_vsr_gre_mpls_tunnels();
            stats->k_udp_mpls_tunnels = req->get_vsr_udp_mpls_tunnels();
            stats->k_l2_mcast_composites = req->get_vsr_l2_mcast_composites();
            stats->k_ecmp_composites = req->get_vsr_ecmp_composites();
            stats->k_fabric_composites = req->get_vsr_fabric_composites();
            stats->k_encaps = req->get_vsr_encaps();
            stats->k_l2_encaps = req->get_vsr_l2_encaps();
            stats->k_gros = req->get_vsr_gros();
            stats->k_diags = req->get_vsr_diags();
            stats->k_encap_composites = req->get_vsr_encap_composites();
            stats->k_evpn_composites = req->get_vsr_evpn_composites();
            stats->k_vrf_translates = req->get_vsr_vrf_translates();
            stats->k_vxlan_tunnels = req->get_vsr_vxlan_tunnels();
            stats->k_arp_virtual_proxy = req->get_vsr_arp_virtual_proxy();
            stats->k_arp_virtual_stitch = req->get_vsr_arp_virtual_stitch();
            stats->k_arp_virtual_flood = req->get_vsr_arp_virtual_flood();
            stats->k_arp_physical_stitch = req->get_vsr_arp_physical_stitch();
            stats->k_arp_tor_proxy = req->get_vsr_arp_tor_proxy();
            stats->k_arp_physical_flood = req->get_vsr_arp_physical_flood();
            stats->k_l2_receives = req->get_vsr_l2_receives();
            stats->k_uuc_floods = req->get_vsr_uuc_floods();
        }
    }
}

void AgentStatsSandeshContext::DropStatsMsgHandler(vr_drop_stats_req *req) {
    AgentDropStats ds = GetDropStats(req);
    stats_->set_drop_stats(ds);
}

AgentDropStats AgentStatsSandeshContext::GetDropStats(vr_drop_stats_req *req) {
    AgentDropStats ds;

    ds.set_ds_discard(req->get_vds_discard());
    uint64_t drop_pkts = ds.get_ds_discard();

    ds.set_ds_pull(req->get_vds_pull());
    drop_pkts += ds.get_ds_pull();

    ds.set_ds_invalid_if(req->get_vds_invalid_if());
    drop_pkts += ds.get_ds_invalid_if();

    ds.set_ds_garp_from_vm(req->get_vds_garp_from_vm());
    drop_pkts += ds.get_ds_garp_from_vm();

    ds.set_ds_invalid_arp(req->get_vds_invalid_arp());
    drop_pkts += ds.get_ds_invalid_arp();

    ds.set_ds_trap_no_if(req->get_vds_trap_no_if());
    drop_pkts += ds.get_ds_trap_no_if();

    ds.set_ds_nowhere_to_go(req->get_vds_nowhere_to_go());
    drop_pkts += ds.get_ds_nowhere_to_go();

    ds.set_ds_flow_queue_limit_exceeded(req->get_vds_flow_queue_limit_exceeded());
    drop_pkts += ds.get_ds_flow_queue_limit_exceeded();

    ds.set_ds_flow_no_memory(req->get_vds_flow_no_memory());
    drop_pkts += ds.get_ds_flow_no_memory();

    ds.set_ds_flow_invalid_protocol(req->get_vds_flow_invalid_protocol());
    drop_pkts += ds.get_ds_flow_invalid_protocol();

    ds.set_ds_flow_nat_no_rflow(req->get_vds_flow_nat_no_rflow());
    drop_pkts += ds.get_ds_flow_nat_no_rflow();

    ds.set_ds_flow_action_drop(req->get_vds_flow_action_drop());
    drop_pkts += ds.get_ds_flow_action_drop();

    ds.set_ds_flow_action_invalid(req->get_vds_flow_action_invalid());
    drop_pkts += ds.get_ds_flow_action_invalid();

    ds.set_ds_flow_unusable(req->get_vds_flow_unusable());
    drop_pkts += ds.get_ds_flow_unusable();

    ds.set_ds_flow_table_full(req->get_vds_flow_table_full());
    drop_pkts += ds.get_ds_flow_table_full();

    ds.set_ds_interface_tx_discard(req->get_vds_interface_tx_discard());
    drop_pkts += ds.get_ds_interface_tx_discard();

    ds.set_ds_interface_drop(req->get_vds_interface_drop());
    drop_pkts += ds.get_ds_interface_drop();

    ds.set_ds_duplicated(req->get_vds_duplicated());
    drop_pkts += ds.get_ds_duplicated();

    ds.set_ds_push(req->get_vds_push());
    drop_pkts += ds.get_ds_push();

    ds.set_ds_ttl_exceeded(req->get_vds_ttl_exceeded());
    drop_pkts += ds.get_ds_ttl_exceeded();

    ds.set_ds_invalid_nh(req->get_vds_invalid_nh());
    drop_pkts += ds.get_ds_invalid_nh();

    ds.set_ds_invalid_label(req->get_vds_invalid_label());
    drop_pkts += ds.get_ds_invalid_label();

    ds.set_ds_invalid_protocol(req->get_vds_invalid_protocol());
    drop_pkts += ds.get_ds_invalid_protocol();

    ds.set_ds_interface_rx_discard(req->get_vds_interface_rx_discard());
    drop_pkts += ds.get_ds_interface_rx_discard();

    ds.set_ds_invalid_mcast_source(req->get_vds_invalid_mcast_source());
    drop_pkts += ds.get_ds_invalid_mcast_source();

    ds.set_ds_head_alloc_fail(req->get_vds_head_alloc_fail());
    drop_pkts += ds.get_ds_head_alloc_fail();

    ds.set_ds_head_space_reserve_fail(req->get_vds_head_space_reserve_fail());
    drop_pkts += ds.get_ds_head_space_reserve_fail();

    ds.set_ds_pcow_fail(req->get_vds_pcow_fail());
    drop_pkts += ds.get_ds_pcow_fail();

    ds.set_ds_flood(req->get_vds_flood());
    drop_pkts += ds.get_ds_flood();

    ds.set_ds_mcast_clone_fail(req->get_vds_mcast_clone_fail());
    drop_pkts += ds.get_ds_mcast_clone_fail();

    ds.set_ds_mcast_df_bit(req->get_vds_mcast_df_bit());
    drop_pkts += ds.get_ds_mcast_df_bit();

    ds.set_ds_no_memory(req->get_vds_no_memory());
    drop_pkts += ds.get_ds_no_memory();

    ds.set_ds_rewrite_fail(req->get_vds_rewrite_fail());
    drop_pkts += ds.get_ds_rewrite_fail();

    ds.set_ds_misc(req->get_vds_misc());
    drop_pkts += ds.get_ds_misc();

    ds.set_ds_invalid_packet(req->get_vds_invalid_packet());
    drop_pkts += ds.get_ds_invalid_packet();

    ds.set_ds_cksum_err(req->get_vds_cksum_err());
    drop_pkts += ds.get_ds_cksum_err();

    ds.set_ds_clone_fail(req->get_vds_clone_fail());
    drop_pkts += ds.get_ds_clone_fail();

    ds.set_ds_no_fmd(req->get_vds_no_fmd());
    drop_pkts += ds.get_ds_no_fmd();

    ds.set_ds_invalid_vnid(req->get_vds_invalid_vnid());
    drop_pkts += ds.get_ds_invalid_vnid();

    ds.set_ds_frag_err(req->get_vds_frag_err());
    drop_pkts += ds.get_ds_frag_err();

    ds.set_ds_invalid_source(req->get_vds_invalid_source());
    drop_pkts += ds.get_ds_invalid_source();

    ds.set_ds_arp_no_where_to_go(req->get_vds_arp_no_where_to_go());
    drop_pkts += ds.get_ds_arp_no_where_to_go();

    ds.set_ds_arp_no_route(req->get_vds_arp_no_route());
    drop_pkts += ds.get_ds_arp_no_route();

    ds.set_ds_l2_no_route(req->get_vds_l2_no_route());
    drop_pkts += ds.get_ds_l2_no_route();

    ds.set_ds_fragment_queue_fail(req->get_vds_fragment_queue_fail());
    drop_pkts += ds.get_ds_fragment_queue_fail();

    ds.set_ds_vlan_fwd_tx(req->get_vds_vlan_fwd_tx());
    drop_pkts += ds.get_ds_vlan_fwd_tx();

    ds.set_ds_vlan_fwd_enq(req->get_vds_vlan_fwd_enq());
    drop_pkts += ds.get_ds_vlan_fwd_enq();

    ds.set_ds_drop_new_flow(req->get_vds_drop_new_flow());
    drop_pkts += ds.get_ds_drop_new_flow();

    ds.set_ds_flow_evict(req->get_vds_flow_evict());
    drop_pkts += ds.get_ds_flow_evict();

    ds.set_ds_trap_original(req->get_vds_trap_original());
    drop_pkts += ds.get_ds_trap_original();
    ds.set_ds_drop_pkts(drop_pkts);

    return ds;
} 


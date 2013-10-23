#
# Copyright (c) 2013 Juniper Networks, Inc. All rights reserved.
#
#
# This file is built up from an autogenerated template resource_server.py and
# contains code/hooks at different point during processing a request, specific
# to type of resource. For eg. allocation of mac/ip-addr for a port during its
# creation.

import json
import re

import cfgm_common
from gen.resource_xsd import *
from gen.resource_common import *
from gen.resource_server import *


class FloatingIpServer(FloatingIpServerGen):
    generate_default_instance = False

    @classmethod
    def http_post_collection(cls, tenant_name, obj_dict, db_conn):
        vn_fq_name = obj_dict['fq_name'][:-2]
        req_ip = obj_dict.get("floating_ip_address", None)
        try:
            fip_addr = cls.addr_mgmt.ip_alloc(
                vn_fq_name, asked_ip_addr=req_ip)
        except Exception as e:
            return (False, (503, str(e)))
        obj_dict['floating_ip_address'] = fip_addr
        print 'AddrMgmt: alloc %s FIP for vn=%s, tenant=%s, askip=%s' \
            % (obj_dict['floating_ip_address'], vn_fq_name, tenant_name,
               req_ip)
        return True, ""
    # end http_post_collection

    @classmethod
    def http_delete(cls, id, obj_dict, db_conn):
        vn_fq_name = obj_dict['fq_name'][:-2]
        fip_addr = obj_dict['floating_ip_address']
        print 'AddrMgmt: free FIP %s for vn=%s' % (fip_addr, vn_fq_name)
        cls.addr_mgmt.ip_free(fip_addr, vn_fq_name)
        return True, ""
    # end http_delete

    @classmethod
    def dbe_delete_notification(cls, obj_ids, obj_dict):
        fip_addr = obj_dict['floating_ip_address']
        vn_fq_name = obj_dict['fq_name'][:-2]
        cls.addr_mgmt.ip_free(fip_addr, vn_fq_name)
    # end dbe_delete_notification

# end class FloatingIpServer


class InstanceIpServer(InstanceIpServerGen):
    generate_default_instance = False

    @classmethod
    def http_post_collection(cls, tenant_name, obj_dict, db_conn):
        vn_fq_name = obj_dict['virtual_network_refs'][0]['to']
        if ((vn_fq_name == cfgm_common.IP_FABRIC_VN_FQ_NAME) or
                (vn_fq_name == cfgm_common.LINK_LOCAL_VN_FQ_NAME)):
            # Ignore ip-fabric and link-local address allocations
            return True,  ""

        req_ip = obj_dict.get("instance_ip_address", None)
        try:
            ip_addr = cls.addr_mgmt.ip_alloc(
                vn_fq_name, asked_ip_addr=req_ip)
        except Exception as e:
            return (False, (503, str(e)))
        obj_dict['instance_ip_address'] = ip_addr
        print 'AddrMgmt: alloc %s for vn=%s, tenant=%s, askip=%s' \
            % (obj_dict['instance_ip_address'],
               vn_fq_name, tenant_name, req_ip)
        return True, ""
    # end http_post_collection

    @classmethod
    def http_delete(cls, id, obj_dict, db_conn):
        vn_fq_name = obj_dict['virtual_network_refs'][0]['to']
        if ((vn_fq_name == cfgm_common.IP_FABRIC_VN_FQ_NAME) or
                (vn_fq_name == cfgm_common.LINK_LOCAL_VN_FQ_NAME)):
            # Ignore ip-fabric and link-local address allocations
            return True,  ""

        ip_addr = obj_dict['instance_ip_address']
        print 'AddrMgmt: free IP %s, vn=%s' % (ip_addr, vn_fq_name)
        cls.addr_mgmt.ip_free(ip_addr, vn_fq_name)
        return True, ""
    # end http_delete

    @classmethod
    def dbe_delete_notification(cls, obj_ids, obj_dict):
        ip_addr = obj_dict['instance_ip_address']
        vn_fq_name = obj_dict['virtual_network_refs'][0]['to']
        cls.addr_mgmt.ip_free(ip_addr, vn_fq_name)
    # end dbe_delete_notification

# end class InstanceIpServer


class VirtualMachineInterfaceServer(VirtualMachineInterfaceServerGen):
    generate_default_instance = False

    @classmethod
    def http_post_collection(cls, tenant_name, obj_dict, db_conn):
        mac_addr = cls.addr_mgmt.mac_alloc(obj_dict)
        mac_addrs_obj = MacAddressesType([mac_addr])
        mac_addrs_json = json.dumps(
            mac_addrs_obj,
            default=lambda o: dict((k, v)
                                   for k, v in o.__dict__.iteritems()))
        mac_addrs_dict = json.loads(mac_addrs_json)
        obj_dict['virtual_machine_interface_mac_addresses'] = mac_addrs_dict
        return True, ""
    # end http_post_collection

# end class VirtualMachineInterfaceServer


class VirtualNetworkServer(VirtualNetworkServerGen):

    @classmethod
    def http_post_collection(cls, tenant_name, obj_dict, db_conn):
        cls.addr_mgmt.net_create(obj_dict)
        return True, ""
    # end http_post_collection

    @classmethod
    def http_put(cls, id, obj_dict, db_conn):
        if ((obj_dict['fq_name'] == cfgm_common.IP_FABRIC_VN_FQ_NAME) or
                (obj_dict['fq_name'] == cfgm_common.LINK_LOCAL_VN_FQ_NAME)):
            # Ignore ip-fabric subnet updates
            return True,  ""

        (ok, result) = cls.addr_mgmt.net_check_subnet_overlap(obj_dict)
        if not ok:
            return (ok, (409, result))
        (ok, result) = cls.addr_mgmt.net_check_subnet_delete(obj_dict)
        if not ok:
            return (ok, (409, result))
        cls.addr_mgmt.net_create(obj_dict, id)

        return True, ""
    # end http_put

    @classmethod
    def http_delete(cls, id, obj_dict, db_conn):
        cls.addr_mgmt.net_delete(obj_dict)
        return True, ""
    # end http_delete

    @classmethod
    def ip_alloc(cls, vn_fq_name, subnet_name, count):
        ip_list = [cls.addr_mgmt.ip_alloc(vn_fq_name, subnet_name)
                   for i in range(count)]
        print 'AddrMgmt: reserve %d IP for vn=%s, subnet=%s - %s' \
            % (count, vn_fq_name, subnet_name if subnet_name else '', ip_list)
        return {'ip_addr': ip_list}
    # end ip_alloc

    @classmethod
    def ip_free(cls, vn_fq_name, subnet_name, ip_list):
        print 'AddrMgmt: release IP %s for vn=%s, subnet=%s' \
            % (ip_list, vn_fq_name, subnet_name if subnet_name else '')
        for ip_addr in ip_list:
            cls.addr_mgmt.ip_free(ip_addr, vn_fq_name, subnet_name)
    # end ip_free

    @classmethod
    def subnet_ip_count(cls, obj_dict, subnet_list):
        ip_count_list = []
        for item in subnet_list:
            ip_count_list.append(cls.addr_mgmt.ip_count(obj_dict, item))
        return {'ip_count_list': ip_count_list}
    # end ip_count

# end class VirtualNetworkServer


class VirtualDnsServer(VirtualDnsServerGen):
    generate_default_instance = False

    @classmethod
    def http_post_collection(cls, tenant_name, obj_dict, db_conn):
        return cls.validate_dns_server(obj_dict, db_conn)
    # end http_post_collection

    @classmethod
    def http_put(cls, id, obj_dict, db_conn):
        return cls.validate_dns_server(obj_dict, db_conn)
    # end http_put

    @classmethod
    def http_delete(cls, id, obj_dict, db_conn):
        vdns_name = ":".join(obj_dict['fq_name'])
        if 'parent_uuid' in obj_dict:
            domain_uuid = obj_dict['parent_uuid']
            domain_id = {'uuid': domain_uuid}
            (read_ok, read_result) = db_conn.dbe_read('domain', domain_id)
            if not read_ok:
                return (
                    False,
                    (503, "Internal error : Virtual Dns is not in a domain"))
            virtual_DNSs = read_result.get('virtual_DNSs', None)
            for vdns in virtual_DNSs:
                vdns_uuid = vdns['uuid']
                vdns_id = {'uuid': vdns_uuid}
                (read_ok, read_result) = db_conn.dbe_read('virtual-DNS',
                                                          vdns_id)
                if not read_ok:
                    return (
                        False,
                        (503,
                         "Internal error : Unable to read Virtual Dns data"))
                vdns_data = read_result['virtual_DNS_data']
                if 'next_virtual_DNS' in vdns_data:
                    if vdns_data['next_virtual_DNS'] == vdns_name:
                        return (
                            False,
                            (403,
                             "Virtual DNS server is referred"
                             " by other virtual DNS servers"))
        return True, ""
    # end http_delete

    @classmethod
    def is_valid_dns_name(cls, name):
        if len(name) > 255:
            return False
        if name.endswith("."):  # A single trailing dot is legal
            # strip exactly one dot from the right, if present
            name = name[:-1]
        disallowed = re.compile("[^A-Z\d-]", re.IGNORECASE)
        return all(  # Split by labels and verify individually
            (label and len(label) <= 63  # length is within proper range
             # no bordering hyphens
             and not label.startswith("-") and not label.endswith("-")
             and not disallowed.search(label))  # contains only legal char
            for label in name.split("."))
    # end is_valid_dns_name

    @classmethod
    def is_valid_ipv4_address(cls, address):
        parts = address.split(".")
        if len(parts) != 4:
            return False
        for item in parts:
            try:
                if not 0 <= int(item) <= 255:
                    return False
            except ValueError:
                return False
        return True
    # end is_valid_ipv4_address

    @classmethod
    def validate_dns_server(cls, obj_dict, db_conn):
        vdns_data = obj_dict['virtual_DNS_data']
        if not cls.is_valid_dns_name(vdns_data['domain_name']):
            return (
                False,
                (403, "Domain name does not adhere to DNS name requirements"))

        record_order = ["fixed", "random", "round-robin"]
        if not str(vdns_data['record_order']).lower() in record_order:
            return (False, (403, "Invalid value for record order"))

        ttl = vdns_data['default_ttl_seconds']
        if ttl < 0 or ttl > 2147483647:
            return (False, (403, "Invalid value for TTL"))

        if 'next_virtual_DNS' in vdns_data:
            vdns_next = vdns_data['next_virtual_DNS']
            if not vdns_next or vdns_next is None:
                return True, ""
            next_vdns = vdns_data['next_virtual_DNS'].split(":")
            # check that next vdns exists
            try:
                next_vdns_uuid = db_conn.fq_name_to_uuid(
                    'virtual_DNS', next_vdns)
            except Exception as e:
                if not cls.is_valid_ipv4_address(
                        vdns_data['next_virtual_DNS']):
                    return (
                        False,
                        (403,
                         "Invalid Virtual Forwarder(next virtual dns server)"))
                else:
                    return True, ""
            # check that next virtual dns servers arent referring to each other
            # above check doesnt allow during create, but entry could be
            # modified later
            next_vdns_id = {'uuid': next_vdns_uuid}
            (read_ok, read_result) = db_conn.dbe_read(
                'virtual-DNS', next_vdns_id)
            if read_ok:
                next_vdns_data = read_result['virtual_DNS_data']
                if 'next_virtual_DNS' in next_vdns_data:
                    vdns_name = ":".join(obj_dict['fq_name'])
                    if next_vdns_data['next_virtual_DNS'] == vdns_name:
                        return (
                            False,
                            (403,
                             "Cannot have Virtual Dns servers "
                             "referring to each other"))
        return True, ""
    # end validate_dns_server
# end class VirtualDnsServer


class VirtualDnsRecordServer(VirtualDnsRecordServerGen):
    generate_default_instance = False

    @classmethod
    def http_post_collection(cls, tenant_name, obj_dict, db_conn):
        return cls.validate_dns_record(obj_dict, db_conn)
    # end http_post_collection

    @classmethod
    def http_put(cls, id, obj_dict, db_conn):
        return cls.validate_dns_record(obj_dict, db_conn)
    # end http_put

    @classmethod
    def http_delete(cls, id, obj_dict, db_conn):
        return True, ""
    # end http_delete

    @classmethod
    def validate_dns_record(cls, obj_dict, db_conn):
        rec_data = obj_dict['virtual_DNS_record_data']
        rec_types = ["a", "cname", "ptr", "ns"]
        rec_type = str(rec_data['record_type']).lower()
        if not rec_type in rec_types:
            return (False, (403, "Invalid record type"))
        if str(rec_data['record_class']).lower() != "in":
            return (False, (403, "Invalid record class"))

        rec_name = rec_data['record_name']
        rec_value = rec_data['record_data']

        # check rec_name validity
        if rec_type == "ptr":
            if (not VirtualDnsServer.is_valid_ipv4_address(rec_name) and
                    not "in-addr.arpa" in rec_name.lower()):
                return (
                    False,
                    (403,
                     "PTR Record name has to be IP address"
                     " or reverse.ip.in-addr.arpa"))
        elif not VirtualDnsServer.is_valid_dns_name(rec_name):
            return (
                False,
                (403, "Record name does not adhere to DNS name requirements"))

        # check rec_data validity
        if rec_type == "a":
            if not VirtualDnsServer.is_valid_ipv4_address(rec_value):
                return (False, (403, "Invalid IP address"))
        elif rec_type == "cname" or rec_type == "ptr":
            if not VirtualDnsServer.is_valid_dns_name(rec_value):
                return (
                    False,
                    (403,
                     "Record data does not adhere to DNS name requirements"))
        elif rec_type == "ns":
            try:
                vdns_name = rec_value.split(":")
                vdns_uuid = db_conn.fq_name_to_uuid('virtual_DNS', vdns_name)
            except Exception as e:
                if (not VirtualDnsServer.is_valid_ipv4_address(rec_value) and
                        not VirtualDnsServer.is_valid_dns_name(rec_value)):
                    return (
                        False,
                        (403, "Invalid virtual dns server in record data"))

        ttl = rec_data['record_ttl_seconds']
        if ttl < 0 or ttl > 2147483647:
            return (False, (403, "Invalid value for TTL"))
        return True, ""
    # end validate_dns_record
# end class VirtualDnsRecordServer

#!/usr/bin/env python
#
# Copyright (c) 2017, Intel Corporation
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
#     * Redistributions of source code must retain the above copyright notice,
#       this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#     * Neither the name of Intel Corporation nor the names of its contributors
#       may be used to endorse or promote products derived from this software
#       without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import argparse
import math

def print_cbs_params_for_class_a(args):
    idleslope = args.idleslope_a
    sendslope = idleslope - args.link_speed
    # According to 802.1Q-2014 spec, Annex L, hiCredit and
    # loCredit for SR class A are calculated following the
    # equations L-10 and L-12, respectively.
    hicredit = math.ceil(idleslope * args.frame_non_sr / args.link_speed)
    locredit = math.ceil(sendslope * args.frame_a / args.link_speed)
    
    # added code --- can be removed as per need 
    interface_name = args.interface_name
    qdisc_root_handle = args.qdisc_root_handle
    qdisc_root_handle = str(qdisc_root_handle) + ":1"

    print("sudo tc qdisc add dev %s parent %s handle 7777 cbs idleslope %d sendslope %d hicredit %d locredit %d offload 1" % \
          (interface_name,qdisc_root_handle,idleslope, sendslope, hicredit, locredit))

def print_cbs_params_for_class_b(args):
    idleslope = args.idleslope_b
    sendslope = idleslope - args.link_speed

    # added code and can be removed
    interface_name = args.interface_name
    qdisc_root_handle = args.qdisc_root_handle
    qdisc_root_handle = str(qdisc_root_handle) + ":2"
    

    # Annex L doesn't present a straightforward equation to
    # calculate hiCredit for Class B so we have to derive it
    # based on generic equations presented in that Annex.
    #
    # L-3 is the primary equation to calculate hiCredit. Section
    # L.2 states that the 'maxInterferenceSize' for SR class B
    # is the maximum burst size for SR class A plus the
    # maxInterferenceSize from SR class A (which is equal to the
    # maximum frame from non-SR traffic).
    #
    # The maximum burst size for SR class A equation is shown in
    # L-16. Merging L-16 into L-3 we get the resulting equation
    # which calculates hiCredit B (refer to section L.3 in case
    # you're not familiar with the legend):
    #
    # hiCredit B = Rb * (     Mo         Ma   )
    #                     ---------- + ------
    #                      Ro - Ra       Ro
    #
    hicredit = math.ceil(idleslope * \
               ((args.frame_non_sr / (args.link_speed - args.idleslope_a)) + \
               (args.frame_a / args.link_speed)))

    # loCredit B is calculated following equation L-2.
    locredit = math.ceil(sendslope * args.frame_b / args.link_speed)

    print("sudo tc qdisc add dev %s parent %s handle 8888 cbs idleslope %d sendslope %d hicredit %d locredit %d offload 1" % \
          (interface_name,qdisc_root_handle,idleslope, sendslope, hicredit, locredit))

def main():
    parser = argparse.ArgumentParser()
    
    # By default the link speed is provided as 1gbps
    parser.add_argument('-S', dest='link_speed', default=1000.0, type=float,
                        help='Link speed in mbps')

    # MTU is takes 1500 for the link 
    parser.add_argument('-s', dest='frame_non_sr', default=1500.0, type=float,
                        help='Maximum frame size from non-SR traffic (MTU size'
                        'usually')

    # By default we are adding two classes 
    # Inserting the BW for the class A 
    parser.add_argument('-A', dest='idleslope_a', default=0, type=float,
                        help='Idleslope for SR class A in mbps')
    
    # Inserting the packet size to be sent for the A class
    parser.add_argument('-a', dest='frame_a', default=0, type=float,
                        help='Maximum frame size for SR class A traffic')
    
    # Inserting the BW for the class B
    parser.add_argument('-B', dest='idleslope_b', default=0, type=float,
                        help='Idleslope for SR class B in mbps')

    # Inserting the packet size to be sent for the B class
    parser.add_argument('-b', dest='frame_b', default=0, type=float,
                        help='Maximum frame size for SR class B traffic')

    # Inserting the interface name
    parser.add_argument('-i', dest='interface_name', default='eth0', type=str,
                        help='Interface name')

    # Inserting the packet size to be sent for the B class
    parser.add_argument('-q', dest='qdisc_root_handle', default=100, type=int,
                        help='Handle for root QDISC')
    
    # Parsing the arguments 
    args = parser.parse_args()

    # Taking mbps parameters
    # and converting them to kbps
    args.link_speed*=1000
    args.idleslope_a*=1000
    args.idleslope_b*=1000


    # Validating the arguments 
    if args.idleslope_a > 0:
        print_cbs_params_for_class_a(args)

    if args.idleslope_b > 0:
        print_cbs_params_for_class_b(args)

if __name__ == "__main__":
    main()

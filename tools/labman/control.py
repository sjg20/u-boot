# SPDX-License-Identifier: GPL-2.0+
# Copyright 2020 Google LLC
# Written by Simon Glass <sjg@chromium.org>
#
# Main control for labman
#

import glob
import os
import socket

from lab import Lab

OUR_PATH = os.path.dirname(os.path.realpath(__file__))

# Used to access the lab for testing
test_lab = None

def Labman(args, lab=None):
    lab_fname = args.lab
    remote = args.remote
    if not args.lab:
        labs = glob.glob('*.yaml') + glob.glob(os.path.join(OUR_PATH, '*.yaml'))
        if len(labs) == 1:
            lab_fname = labs[0]
    if lab_fname:
        if not lab:
            global test_lab
            lab = Lab()
            test_lab = lab
        lab.read(lab_fname)
        if not remote and lab._host != socket.gethostname():
            remote = lab._host

    if args.single_threaded:
        lab.set_num_threads(0)

    if remote:
        lab.set_remote(remote)

    lab.setup_state_dir()

    result = 0
    if args.cmd == 'check':
        result = lab.check(args.parts)
    elif args.cmd == 'emit':
        lab.emit(args.output_dir, args.dut, args.ftype)
    elif args.cmd == 'ls':
        lab.show(args.args)
    elif args.cmd == 'prov':
        lab.provision(args.component, args.name, args.serial, args.test,
                      args.device)
    elif args.cmd == 'provtest':
        lab.provision_test(args.component, args.device)
    elif args.cmd == 'start':
        lab.start_daemons()
    elif args.cmd == 'scan':
        lab.scan()

    return result

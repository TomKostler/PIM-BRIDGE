"""
This script configures a gem5 full-system simulation for a single-core
ARM board. It uses an HBM2 memory model and a processor that switches
from a simple Atomic core to a detailed O3 core after the system boots.
The device tree is customized to reserve specific memory regions for a
Processing-In-Memory (PIM) device.
The simulation boots a custom Ubuntu image, automatically loads a PIM driver
module, and uses a userspace program to trigger the driver on the O3 core.


This config has to be located in gem5/configs in order to work.
The Ubuntu Image aswell as a Linux kernel suitable for the module and a 
bootloader have to be in gem5/pim_bridge_connector.

The Ubuntu image must also include the after_boot.sh script and ensure that 
.bashrc calls it after boot. Additionally, the userspace_program.c file must
be present for the simulation to function correctly.
"""


# Needed for device tree manipulation
from m5.util.fdthelper import FdtNode, FdtPropertyWords, FdtProperty


from gem5.isas import ISA
from m5.objects import (
    ArmDefaultRelease,
)
from gem5.utils.requires import requires
from gem5.simulate.simulator import Simulator
from m5.objects import VExpress_GEM5_Foundation
from gem5.components.boards.arm_board import ArmBoard
from gem5.components.memory import DRAMSysHBM2
from gem5.components.processors.cpu_types import CPUTypes
from gem5.simulate.exit_event import ExitEvent

from m5.objects import AddrRange
import m5

requires(isa_required=ISA.ARM)

from gem5.components.cachehierarchies.classic.private_l1_private_l2_cache_hierarchy import (
    PrivateL1PrivateL2CacheHierarchy,
)

import os
from gem5.resources.resource import (
    obtain_resource,
    DiskImageResource,
    KernelResource,
)


from gem5.components.processors.simple_switchable_processor import SimpleSwitchableProcessor

cache_hierarchy = PrivateL1PrivateL2CacheHierarchy(
    l1d_size="16kB", l1i_size="16kB", l2_size="256kB"
)



memory = DRAMSysHBM2()
memory.set_memory_range([AddrRange("0x80000000", size="2GB")])

processor = SimpleSwitchableProcessor(
    starting_core_type=CPUTypes.ATOMIC,
    switch_core_type=CPUTypes.O3,
    isa=ISA.ARM,
    num_cores=1,
)

release = ArmDefaultRelease()
platform = VExpress_GEM5_Foundation()


def add_reserved_memory_node(orig_generateDeviceTree):
    def wrapper(self, state):
        root = orig_generateDeviceTree(self, state)

        reserved = FdtNode("reserved-memory")
        reserved.append(FdtPropertyWords("#address-cells", [2]))
        reserved.append(FdtPropertyWords("#size-cells",    [2]))
        reserved.append(FdtProperty("ranges"))

        pim_config = FdtNode("pim_config@C0000000")
        pim_config.append(FdtPropertyWords("reg", [0x0, 0xC0000000, 0x0, 0x00004000]))
        pim_config.append(FdtProperty("no-map"))
        pim_config.append(FdtPropertyWords("linux,usable-memory", [0]))

        pim_data = FdtNode("pim_data@C0004000")
        pim_data.append(FdtPropertyWords("reg", [0x0, 0xC0004000, 0x0, 0x10000000]))
        pim_data.append(FdtProperty("no-map"))
        pim_data.append(FdtPropertyWords("linux,usable-memory", [0]))

        reserved.append(pim_config)
        reserved.append(pim_data)

        root.append(reserved)
        return root

    return wrapper

ArmBoard.generateDeviceTree = add_reserved_memory_node(ArmBoard.generateDeviceTree)



board = ArmBoard(
    clk_freq="3GHz",
    processor=processor,
    memory=memory,
    cache_hierarchy=cache_hierarchy,
    release=release,
    platform=platform,
)

# HBM2 requires line size of 32 Bytes
board.cache_line_size = 32

# Set the fetch buffer size to 32
processor._switchable_cores.get("switch")[0].core.fetchBufferSize = 32



kernel = KernelResource(local_path="pim_bridge_connector/gem5_kernel_image_5_15_36")
root_disk = DiskImageResource(local_path="pim_bridge_connector/arm-ubuntu-24.04-img")
bootloader = obtain_resource("arm64-bootloader-foundation")


board.set_kernel_disk_workload(
    kernel=kernel,
    disk_image=root_disk,
    bootloader=bootloader,
    kernel_args=[
        "console=ttyAMA0",
        "root=/dev/vda2",
        "rootfstype=ext4",
        "rw",
        "earlyprintk=serial,ttyAMA0",
        "memmap=0x10000000$0xC0004000", # 256MB
    ],
)


# Set the PIM-BRIDGE kernel module that after_boot.sh (automatically executed by the image from gem5)
# should insert into the kernel
board.readfile = os.path.join(os.getcwd(), "pim_bridge_connector/pim_bridge_module.ko")


def exit_event_handler():
    
    for core in processor.get_cores():
        print("PROCESSOR TYPE 1:", core.get_type())
        
    # yield False


    # m5.checkpoint("checkpoint_after_boot_kernel_5_15_36")
    print("Initial boot and checkpoint creation finished. Switching to O3 CPU-Model ...")

    processor.switch()

    print("Nach dem Switch:")
    for core in processor.get_cores():
        print("PROCESSOR TYPE 2:", core.get_type())


    for core in processor.get_cores():
        core.core.fetchBufferSize = 32

    yield False

    yield True



# Needed to check if there is a checkpoint that can be used to start from
checkpoint_dir = "checkpoint_after_boot_kernel_5_15_36"
resume_from_checkpoint = os.path.isdir(checkpoint_dir)


simulator = Simulator(
    board=board,
    checkpoint_path=checkpoint_dir if resume_from_checkpoint else None,
    # checkpoint_path=None,
    on_exit_event={
        ExitEvent.EXIT: exit_event_handler()
    },
)

simulator.run()

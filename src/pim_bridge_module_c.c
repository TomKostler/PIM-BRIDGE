#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

#include <linux/ioport.h>
#include <linux/types.h>
#include <linux/vmalloc.h>

#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include <linux/ktime.h>
#include <linux/mm.h>

#include "../include/bins.h"
#include "../include/pim_data_allocator.h"
#include "../include/pim_memory_region.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tom Kostler");
MODULE_DESCRIPTION("Kernel-Module acting as a driver for PIM-Functionality");

#define MAJOR_NUM 100
#define DEVICE_NAME "pim_device"
#define IOCTL_VADD _IOWR(MAJOR_NUM, 2, struct pim_vectors)
#define IOCTL_VMUL _IOWR(MAJOR_NUM, 3, struct pim_vectors)
#define IOCTL_GEMV _IOWR(MAJOR_NUM, 4, struct pim_gemv)

#define MAX_VECTOR_ELEMENTS (1 << 21)

volatile u32 __iomem *pim_data_virt_addr = NULL;
volatile u8 __iomem *pim_config_virt_addr = NULL;

static int get_gemv_inputs(unsigned long arg, uint16_t **vector_out,
                           uint32_t *vector_len_out, uint16_t **matrix_out,
                           uint32_t *dim1_out, uint32_t *dim2_out) {
    struct pim_gemv descriptor;
    uint16_t *kernel_vector = NULL;
    uint16_t *kernel_matrix = NULL;

    // Get the descriptor
    if (copy_from_user(&descriptor, (struct pim_gemv __user *)arg,
                       sizeof(descriptor))) {
        pr_err("PIM: Failed to copy GEMV descriptor\n");
        return -EFAULT;
    }

    // Check dimensions are non zero and len of the input vector is correct for
    // the multiplication
    if (descriptor.input_vector_len == 0 || descriptor.matrix_dim1 == 0 ||
        descriptor.matrix_dim2 == 0) {
        pr_err("PIM: GEMV dimensions cannot be zero\n");
        return -EINVAL;
    }

    kernel_vector = vmalloc(descriptor.input_vector_len * sizeof(uint16_t));
    kernel_matrix = vmalloc(descriptor.matrix_dim1 *
                            descriptor.input_vector_len * sizeof(uint16_t));

    if (!kernel_vector || !kernel_matrix) {
        vfree(kernel_vector);
        vfree(kernel_matrix);
        return -ENOMEM;
    }

    // Copy the data from User Space to kernel Space
    if (copy_from_user(kernel_vector,
                       (void __user *)descriptor.input_vector_user_addr,
                       descriptor.input_vector_len * sizeof(uint16_t))) {
        goto error_cleanup;
    }
    if (copy_from_user(kernel_matrix,
                       (void __user *)descriptor.matrix_user_addr,
                       descriptor.matrix_dim1 * descriptor.input_vector_len *
                           sizeof(uint16_t))) {
        goto error_cleanup;
    }

    *vector_out = kernel_vector;
    *vector_len_out = descriptor.input_vector_len;
    *matrix_out = kernel_matrix;
    *dim1_out = descriptor.matrix_dim1;
    *dim2_out = descriptor.matrix_dim2;

    return 0;

error_cleanup:
    vfree(kernel_vector);
    vfree(kernel_matrix);
    return -EFAULT;
}

static long pim_device_ioctl(struct file *file, unsigned int cmd,
                             unsigned long arg) {
    uint16_t *kernel_vector_a = NULL;
    uint16_t *kernel_vector_b = NULL;

    uint16_t *kernel_input_vector = NULL;
    uint16_t *kernel_matrix = NULL;
    uint32_t len_input_vector;
    uint32_t matrix_dim1;
    uint32_t matrix_dim2;

    struct pim_vectors vectors_descriptor;
    struct pim_gemv gemv_descriptor;

    size_t vector_size_bytes;

    int ret;

    current_start_free_mem_offset = 0;

    switch (cmd) {
    case IOCTL_VADD:
    case IOCTL_VMUL: {

        if (copy_from_user(&vectors_descriptor,
                           (struct pim_vectors __user *)arg,
                           sizeof(vectors_descriptor))) {
            return -EFAULT;
        }

        vector_size_bytes = vectors_descriptor.len * sizeof(uint16_t);
        if (vectors_descriptor.len == 0 ||
            vectors_descriptor.len > MAX_VECTOR_ELEMENTS) {
            return -EINVAL;
        }

        kernel_vector_a = (uint16_t *)((char *)pim_data_virt_addr +
                                       vectors_descriptor.offset_a);
        kernel_vector_b = (uint16_t *)((char *)pim_data_virt_addr +
                                       vectors_descriptor.offset_b);

        if (cmd == IOCTL_VADD) {
            ret = vadd_from_userspace(kernel_vector_a, kernel_vector_b,
                                      &vectors_descriptor);
        } else {
            ret = vmul_from_userspace(kernel_vector_a, kernel_vector_b,
                                      &vectors_descriptor);
        }

        break;
    }

    case IOCTL_GEMV: {
        if (copy_from_user(&gemv_descriptor, (struct pim_gemv __user *)arg,
                           sizeof(gemv_descriptor))) {
            return -EFAULT;
        }

        ret = get_gemv_inputs(arg, &kernel_input_vector, &len_input_vector,
                              &kernel_matrix, &matrix_dim1, &matrix_dim2);
        if (ret) {
            return ret;
        }

        ret = gemv_from_userspace(gemv_descriptor.result_vector_user_addr,
                                  kernel_input_vector, kernel_matrix,
                                  len_input_vector, matrix_dim1, matrix_dim2);

        vfree(kernel_input_vector);
        vfree(kernel_matrix);

        if (ret) {
            return ret;
        }

        break;
    }

    default:
        return -EINVAL;
    }
    return 0;
}

static int pim_mmap(struct file *filp, struct vm_area_struct *vma) {
    unsigned long size = vma->vm_end - vma->vm_start;
    unsigned long pfn;

    if (size > PIM_DATA_MEMORY_REGION_SIZE) {
        pr_err("PIM: mmap requested size is too large.\n");
        return -EINVAL;
    }

    pfn = PIM_DATA_MEMORY_REGION_BASE >> PAGE_SHIFT;

    vma->vm_flags |= VM_IO | VM_DONTEXPAND | VM_DONTDUMP;

    vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

    if (remap_pfn_range(vma, vma->vm_start, pfn, size, vma->vm_page_prot)) {
        pr_err("PIM: remap_pfn_range failed\n");
        return -EAGAIN;
    }

    pr_info("PIM: Successfully remapped physical memory to userspace\n");
    return 0;
}

static struct file_operations fops = {
    .owner = THIS_MODULE, .unlocked_ioctl = pim_device_ioctl, .mmap = pim_mmap};

static int __init pim_bridge_init(void) {
    int major_number;
    pr_warn("Loading PIM-Bridge kernel module\n");

    major_number = register_chrdev(MAJOR_NUM, DEVICE_NAME, &fops);
    if (major_number < 0) {
        pr_err("Failed to register a major number\n");
        return major_number;
    }
    pr_info("Module loaded. Create a device file with:\n");
    pr_info("mknod /dev/%s c %d 0\n", DEVICE_NAME, MAJOR_NUM);

    pim_config_virt_addr =
        ioremap(PIM_CONFIG_MEMORY_REGION_BASE, PIM_CONFIG_MEMORY_REGION_SIZE);
    pim_data_virt_addr =
        ioremap(PIM_DATA_MEMORY_REGION_BASE, PIM_DATA_MEMORY_REGION_SIZE);

    if (!pim_config_virt_addr) {
        pr_err("ioremap config failed\n");
        unregister_chrdev(MAJOR_NUM, DEVICE_NAME);
        return -ENOMEM;
    }

    if (!pim_data_virt_addr) {
        pr_err("ioremap data failed\n");
        iounmap(pim_config_virt_addr);
        unregister_chrdev(MAJOR_NUM, DEVICE_NAME);
        return -ENOMEM;
    }

    pr_info("Initialized PIM-Region starting at: %px\n", pim_data_virt_addr);

    // This code triggers the PIM-VM without needing a call from the User
    // Library (only works for the CPU Model O3 in gem5-Simulation)
    // current_start_free_mem_offset = 0;
    // vadd_driver_code();
    // current_start_free_mem_offset = 0;
    // vmul_driver_code();
    // current_start_free_mem_offset = 0;
    // gemv_driver_code();

    return 0;
}

static void __exit pim_bridge_exit(void) {
    unregister_chrdev(MAJOR_NUM, DEVICE_NAME);

    if (pim_data_virt_addr)
        iounmap(pim_data_virt_addr);
    if (pim_config_virt_addr)
        iounmap(pim_config_virt_addr);

    pr_info("Unloading PIM-Bridge kernel module\n");
}

module_init(pim_bridge_init);
module_exit(pim_bridge_exit);
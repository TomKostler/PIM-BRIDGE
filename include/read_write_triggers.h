#ifndef READ_WRITE_TRIGGERS_H
#define READ_WRITE_TRIGGERS_H


/**
 * Triggers a PIM operation by issuing a memory-mapped byte write to a specific command address.
 */
int trigger_write(void __iomem *address);


/**
 * Triggers a PIM operation by issuing a memory-mapped byte read to a specific command address.
 */
int trigger_read(void __iomem *address);


#endif
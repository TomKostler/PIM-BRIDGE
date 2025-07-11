#include <linux/errno.h>
#include <linux/kernel.h>

#include "../include/microkernels/kernel_to_string.h"

static int parse_file_to_string(char *buffer, size_t size, const File *file) {
    int written = 0;
    switch (file->type) {
    case BANK:
        written = snprintf(buffer, size, "\"Bank\"");
        break;
    case GRF_A:
        written = snprintf(buffer, size, "{\"GrfA\":{\"index\":%u}}",
                           file->grfa.index);
        break;
    case GRF_B:
        written = snprintf(buffer, size, "{\"GrfB\":{\"index\":%u}}",
                           file->grfb.index);
        break;
    case SRF_M:
        written = snprintf(buffer, size, "{\"SrfM\":{\"index\":%u}}",
                           file->srfm.index);
        break;
    case SRF_A:
        written = snprintf(buffer, size, "{\"SrfA\":{\"index\":%u}}",
                           file->srfa.index);
        break;
    default:
        pr_err("Unknown FileType: %d\n", file->type);
        return -EINVAL;
    }

    if (written < 0 || written >= size) {
        pr_err("Buffer too small (needed: %d, available: %zu)\n", written,
               size);
        return -ENOMEM;
    }
    return written;
}

int parse_instruction_to_string(char *buffer, size_t size,
                                const Instruction *instr) {
    int written = 0;
    int total_written = 0;
    char *ptr = buffer;
    size_t remaining = size;

    switch (instr->type) {
    case NOP:
        written = snprintf(ptr, remaining, "\"NOP\"");
        break;
    case EXIT:
        written = snprintf(ptr, remaining, "\"EXIT\"");
        break;
    case JUMP:
        // {"JUMP":{"offset":%d,"count":%u}}
        written =
            snprintf(ptr, remaining, "{\"JUMP\":{\"offset\":%d,\"count\":%u}}",
                     instr->jump.offset, instr->jump.count);
        break;
    case MOV:
        // {"MOV":{"src":<file>,"dst":<file>}}
        written = snprintf(ptr, remaining, "{\"MOV\":{");
        if (written < 0 || written >= remaining)
            goto buffer_error;
        ptr += written;
        remaining -= written;
        total_written += written;

        written = snprintf(ptr, remaining, "\"src\":");
        if (written < 0 || written >= remaining)
            goto buffer_error;
        ptr += written;
        remaining -= written;
        total_written += written;

        written = parse_file_to_string(ptr, remaining, &instr->mov.src);
        if (written < 0)
            return written;
        if (written >= remaining)
            goto buffer_error;
        ptr += written;
        remaining -= written;
        total_written += written;

        written = snprintf(ptr, remaining, ",\"dst\":");
        if (written < 0 || written >= remaining)
            goto buffer_error;
        ptr += written;
        remaining -= written;
        total_written += written;

        written = parse_file_to_string(ptr, remaining, &instr->mov.dst);
        if (written < 0)
            return written;
        if (written >= remaining)
            goto buffer_error;
        ptr += written;
        remaining -= written;
        total_written += written;

        written = snprintf(ptr, remaining, "}}");
        break;

    case FILL:
        // {"FILL":{"src":<file>,"dst":<file>}}
        written = snprintf(ptr, remaining, "{\"FILL\":{");
        if (written < 0 || written >= remaining)
            goto buffer_error;
        ptr += written;
        remaining -= written;
        total_written += written;

        written = snprintf(ptr, remaining, "\"src\":");
        if (written < 0 || written >= remaining)
            goto buffer_error;
        ptr += written;
        remaining -= written;
        total_written += written;

        written = parse_file_to_string(ptr, remaining, &instr->fill.src);
        if (written < 0)
            return written;
        if (written >= remaining)
            goto buffer_error;
        ptr += written;
        remaining -= written;
        total_written += written;

        written = snprintf(ptr, remaining, ",\"dst\":");
        if (written < 0 || written >= remaining)
            goto buffer_error;
        ptr += written;
        remaining -= written;
        total_written += written;

        written = parse_file_to_string(ptr, remaining, &instr->fill.dst);
        if (written < 0)
            return written;
        if (written >= remaining)
            goto buffer_error;
        ptr += written;
        remaining -= written;
        total_written += written;

        written = snprintf(ptr, remaining, "}}");
        break;

    case ADD:
    case MUL:
        // {"<TYPE>":{"src0":<file>,"src1":<file>,"dst":<file>,"aam":<bool>}}
        written = snprintf(ptr, remaining, "{\"%s\":{",
                           (instr->type == ADD) ? "ADD" : "MUL");
        if (written < 0 || written >= remaining)
            goto buffer_error;
        ptr += written;
        remaining -= written;
        total_written += written;

        // src0
        written = snprintf(ptr, remaining, "\"src0\":");
        if (written < 0 || written >= remaining)
            goto buffer_error;
        ptr += written;
        remaining -= written;
        total_written += written;
        written = parse_file_to_string(ptr, remaining,
                                       (instr->type == ADD) ? &instr->add.src0
                                                            : &instr->mul.src0);
        if (written < 0)
            return written;
        if (written >= remaining)
            goto buffer_error;
        ptr += written;
        remaining -= written;
        total_written += written;

        // src1
        written = snprintf(ptr, remaining, ",\"src1\":");
        if (written < 0 || written >= remaining)
            goto buffer_error;
        ptr += written;
        remaining -= written;
        total_written += written;
        written = parse_file_to_string(ptr, remaining,
                                       (instr->type == ADD) ? &instr->add.src1
                                                            : &instr->mul.src1);
        if (written < 0)
            return written;
        if (written >= remaining)
            goto buffer_error;
        ptr += written;
        remaining -= written;
        total_written += written;

        // dst
        written = snprintf(ptr, remaining, ",\"dst\":");
        if (written < 0 || written >= remaining)
            goto buffer_error;
        ptr += written;
        remaining -= written;
        total_written += written;
        written = parse_file_to_string(ptr, remaining,
                                       (instr->type == ADD) ? &instr->add.dst
                                                            : &instr->mul.dst);
        if (written < 0)
            return written;
        if (written >= remaining)
            goto buffer_error;
        ptr += written;
        remaining -= written;
        total_written += written;

        // aam
        written = snprintf(
            ptr, remaining, ",\"aam\":%s",
            ((instr->type == ADD) ? instr->add.aam : instr->mul.aam) ? "true"
                                                                     : "false");
        if (written < 0 || written >= remaining)
            goto buffer_error;
        ptr += written;
        remaining -= written;
        total_written += written;

        written = snprintf(ptr, remaining, "}}");
        break;

    case MAC:
    case MAD:
        // {"<TYPE>":{"src0":<file>,"src1":<file>,"src2":<file>,"dst":<file>,"aam":<bool>}}
        written = snprintf(ptr, remaining, "{\"%s\":{",
                           (instr->type == MAC) ? "MAC" : "MAD");
        if (written < 0 || written >= remaining)
            goto buffer_error;
        ptr += written;
        remaining -= written;
        total_written += written;

        // src0
        written = snprintf(ptr, remaining, "\"src0\":");
        if (written < 0 || written >= remaining)
            goto buffer_error;
        ptr += written;
        remaining -= written;
        total_written += written;
        written = parse_file_to_string(ptr, remaining,
                                       (instr->type == MAC) ? &instr->mac.src0
                                                            : &instr->mad.src0);
        if (written < 0)
            return written;
        if (written >= remaining)
            goto buffer_error;
        ptr += written;
        remaining -= written;
        total_written += written;
        // src1
        written = snprintf(ptr, remaining, ",\"src1\":");
        if (written < 0 || written >= remaining)
            goto buffer_error;
        ptr += written;
        remaining -= written;
        total_written += written;
        written = parse_file_to_string(ptr, remaining,
                                       (instr->type == MAC) ? &instr->mac.src1
                                                            : &instr->mad.src1);
        if (written < 0)
            return written;
        if (written >= remaining)
            goto buffer_error;
        ptr += written;
        remaining -= written;
        total_written += written;
        // src2
        written = snprintf(ptr, remaining, ",\"src2\":");
        if (written < 0 || written >= remaining)
            goto buffer_error;
        ptr += written;
        remaining -= written;
        total_written += written;
        written = parse_file_to_string(ptr, remaining,
                                       (instr->type == MAC) ? &instr->mac.src2
                                                            : &instr->mad.src2);
        if (written < 0)
            return written;
        if (written >= remaining)
            goto buffer_error;
        ptr += written;
        remaining -= written;
        total_written += written;
        // dst
        written = snprintf(ptr, remaining, ",\"dst\":");
        if (written < 0 || written >= remaining)
            goto buffer_error;
        ptr += written;
        remaining -= written;
        total_written += written;
        written = parse_file_to_string(ptr, remaining,
                                       (instr->type == MAC) ? &instr->mac.dst
                                                            : &instr->mad.dst);
        if (written < 0)
            return written;
        if (written >= remaining)
            goto buffer_error;
        ptr += written;
        remaining -= written;
        total_written += written;
        // aam
        written = snprintf(
            ptr, remaining, ",\"aam\":%s",
            ((instr->type == MAC) ? instr->mac.aam : instr->mad.aam) ? "true"
                                                                     : "false");
        if (written < 0 || written >= remaining)
            goto buffer_error;
        ptr += written;
        remaining -= written;
        total_written += written;

        written = snprintf(ptr, remaining, "}}");
        break;

    default:
        pr_err("Undefined InstructionType: %d\n", instr->type);
        return -EINVAL;
    }

    // Final Check
    if (written < 0 || written >= remaining) {
    buffer_error:
        pr_err("Error or buffer too small %d "
               "(needed: %d, available: %zu)\n",
               instr->type, written, remaining);
        return -ENOMEM;
    }
    total_written += written;

    return total_written;
}

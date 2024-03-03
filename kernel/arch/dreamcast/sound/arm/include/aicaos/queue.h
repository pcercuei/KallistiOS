#ifndef __AICAOS_QUEUE_H
#define __AICAOS_QUEUE_H

struct aica_cmd;
struct aica_header;

void process_cmd_queue(struct aica_header *header);

/* This function is not provided by AICAOS. Instead, the user application should
 * define it to process the commands sent by the SH4. */
void aica_process_command(struct aica_header *header, struct aica_cmd *cmd);

#endif /* __AICAOS_QUEUE_H */

#ifndef __AICAOS_QUEUE_H
#define __AICAOS_QUEUE_H

struct aica_cmd;
struct aica_header;

void aica_init_queue(struct aica_header *header);
void aica_notify_queue(void);

/* This function is not provided by AICAOS. Instead, the user application should
 * define it to process the commands sent by the SH4. */
void aica_process_command(struct aica_header *header, struct aica_cmd *cmd);

/* Answer a given command.
 * This should be called from aica_process_command(). */
void aica_send_response_code(struct aica_header *header, unsigned int code);

/* Add a command to the output queue. */
void aica_add_cmd(const struct aica_cmd *cmd);

#endif /* __AICAOS_QUEUE_H */

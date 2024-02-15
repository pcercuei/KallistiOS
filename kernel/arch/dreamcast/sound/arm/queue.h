
struct aica_header;

void aica_init_queue(struct aica_header *header);
void aica_notify_queue(void);

void aica_printf(const char *fmt, ...);

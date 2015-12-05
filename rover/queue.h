#ifndef _ROVER_QUEUE_H_
#define _ROVER_QUEUE_H_

struct msg {
	char op[4+1];
	char arg[10+1];
};

void queue_init(void);
void queue_write(struct msg *);
void queue_read(struct msg *);

#endif

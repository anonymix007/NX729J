#ifndef __NB_BOOST__
#define __NB_BOOST__

#define BOOST_MAX_LEVEL  5
#define BOOST_MAX_CTRL   8
#define NB_TRACEPOINT_SCHED_WAKEUP "sched_switch"	//sched_wakeup

typedef struct nb_boost{
	u32 pid;
	u8 optype;
	u8 level;
	u8 type;
	bool finish;
	u64 count;
}ST_BOOST;

extern const char optype_str[4][16];

enum{
	PULL_NULL = 0,
	PULL_UP = 1,
	PULL_DOWN = 2,
	PULL_CANCEL = 3,
};
enum{
	TYPE_DEF = 0,
	TYPE_RB_TREE = 1,
	TYPE_WEIGHT = 2,
	TYPE_TICK = 3,
};

#endif

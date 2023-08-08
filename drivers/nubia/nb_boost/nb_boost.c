// SPDX-License-Identifier: GPL-2.0-only
/*
 * kernel/sched/nb_boost.c
 *
 * boost the process by id for nubia product.
 *
 * Copyright(C) 2022, Red Hat, Inc., Ingo Molnar
 */
#include <linux/kobject.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <uapi/linux/sched/types.h>
#include <linux/stringify.h>
#include <linux/ftrace.h>
#include <linux/kprobes.h>
#include <linux/kallsyms.h>
#include <linux/init.h>
#include <linux/syscalls.h>
#include <linux/version.h>
#include <linux/tracepoint.h>
#include <trace/hooks/epoch.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include "nb_boost.h"

#define LOG_TAG "NB_BOOST"
#define NB_DBG(fmt, args...)                                                \
	do {                                                                   \
		printk(KERN_DEBUG "[%s] [%s:%d] " fmt, LOG_TAG, __FUNCTION__,  \
				__LINE__, ##args);                                      \
	} while (0)

#define NB_RUN_DBG(fmt, args...)                                                \
	do {                                                                   \
		if(gnc.dbg_en)printk(KERN_DEBUG "[%s] [%s:%d] " fmt, LOG_TAG, __FUNCTION__,  \
				__LINE__, ##args);                                      \
	} while (0)

typedef struct{
	u8 dbg_en;
	struct mutex nb_mutex;
	bool tp_reg_weight;
	bool tp_reg_rbtree;
	ST_BOOST nb_boost_bak;
	struct tracepoint *nb_tp;
	ST_BOOST nb_boost[BOOST_MAX_CTRL];
	struct kobject  *nb_boost_kobj;
}ST_NB_BOOST;

ST_NB_BOOST gnc;

const char optype_str[4][16] = {
		"NULL",
		"PULL-UP",
		"PULL-DOWN",
		"PULL-CANCEL",
};

/*copy from sched core.c*/
const int sched_prio_to_weight[40] = {
 /* -20 */     88761,     71755,     56483,     46273,     36291,
 /* -15 */     29154,     23254,     18705,     14949,     11916,
 /* -10 */      9548,      7620,      6100,      4904,      3906,
 /*  -5 */      3121,      2501,      1991,      1586,      1277,
 /*   0 */      1024,       820,       655,       526,       423,
 /*   5 */       335,       272,       215,       172,       137,
 /*  10 */       110,        87,        70,        56,        45,
 /*  15 */        36,        29,        23,        18,        15,
};
const u32 sched_prio_to_wmult[40] = {
 /* -20 */     48388,     59856,     76040,     92818,    118348,
 /* -15 */    147320,    184698,    229616,    287308,    360437,
 /* -10 */    449829,    563644,    704093,    875809,   1099582,
 /*  -5 */   1376151,   1717300,   2157191,   2708050,   3363326,
 /*   0 */   4194304,   5237765,   6557202,   8165337,  10153587,
 /*   5 */  12820798,  15790321,  19976592,  24970740,  31350126,
 /*  10 */  39045157,  49367440,  61356676,  76695844,  95443717,
 /*  15 */ 119304647, 148102320, 186737708, 238609294, 286331153,
};
static void nb_boost_by_weight(void *ignore, bool preempt, struct task_struct *prev, struct task_struct *tsk);
static void nb_boost_by_rb_tree(void *ignore, bool preempt, struct task_struct *prev, struct task_struct *tsk);
//static s64 nb_boost_by_tick(struct rq *rq, struct task_struct *tsk, s64 delta);

static int nb_boost_find_node_by_pid(u32 pid)
{
	u8 i = 0;
	
	for(i=0; i<BOOST_MAX_CTRL; i++){
		if(gnc.nb_boost[i].pid == pid)
			return i;
	}
	return -1;
}
static int num_register_tracepoints_for_weight(void)
{
	u8 i = 0;
	u8 count = 0;
	
	for(i=0; i<BOOST_MAX_CTRL; i++){
		if(gnc.nb_boost[i].type == TYPE_WEIGHT)
			count++;
	}
	return count;
}
static int num_register_tracepoints_for_rbtree(void)
{
	u8 i = 0;
	u8 count = 0;
	
	for(i=0; i<BOOST_MAX_CTRL; i++){
		if(gnc.nb_boost[i].type == TYPE_RB_TREE)
			count++;
	}
	return count;
}
static ssize_t nb_boost_pid_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	u8 i = 0;
	u32 rc = 0;
	
	for(i=0; i<BOOST_MAX_CTRL; i++){
		rc += sprintf(buf+rc, "%d:%d\n", i, gnc.nb_boost[i].pid);
	}
	return rc;
}

static ssize_t nb_boost_pid_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	int loc = 0;
	int rc = 0;
	u32 pid = 0;
	u8 i = 0;

	rc = kstrtoint(buf, 10, &pid);
	if (rc < 0)
		return rc;

	mutex_lock(&gnc.nb_mutex);

	loc = nb_boost_find_node_by_pid(pid);	//if a modify request
	if(loc < 0){
		if(gnc.nb_boost_bak.optype == PULL_CANCEL){
			NB_DBG("Can't find pid:%d node for PULL_CANCEL!\n", pid);
			mutex_unlock(&gnc.nb_mutex);
			return -EINVAL;
		}
		loc = nb_boost_find_node_by_pid(0);	//find a empty node
		if(loc < 0){
			NB_DBG("No resource to monitor more process!\n");
			mutex_unlock(&gnc.nb_mutex);
			return -ENOMEM;
		}
	}else{	//for rewrite type, normally deny rewrite type, should set new type after cancel old type
		if(gnc.nb_boost_bak.type!=gnc.nb_boost[loc].type){
			NB_DBG("Deny rewrite type, should set new type after cancel old type!\n");
			mutex_unlock(&gnc.nb_mutex);
			return -ENOMEM;
		}
	}
	gnc.nb_boost_bak.pid = pid;

	if(gnc.nb_boost_bak.optype != PULL_CANCEL){	//no need for pull_cancel
		gnc.nb_boost[loc].type = gnc.nb_boost_bak.type;
		gnc.nb_boost[loc].level = gnc.nb_boost_bak.level;
	}
	gnc.nb_boost[loc].optype = gnc.nb_boost_bak.optype;
	gnc.nb_boost[loc].finish = false;
	gnc.nb_boost[loc].pid = gnc.nb_boost_bak.pid;
	memset(&gnc.nb_boost_bak, 0, sizeof(ST_BOOST));

	if(gnc.nb_boost[loc].type==TYPE_WEIGHT){
		if(gnc.nb_boost[loc].optype!=PULL_CANCEL && gnc.tp_reg_weight==false){	//need register
			rc = tracepoint_probe_register(gnc.nb_tp, nb_boost_by_weight, NULL);
			if(!rc) gnc.tp_reg_weight=true;
			NB_DBG("tracepoint_probe_register for nb_boost_by_weight rc:%d.", rc);
		}
		if(gnc.nb_boost[loc].optype==PULL_CANCEL){	//need unregister && wait for reweight finish
			while(gnc.nb_boost[loc].finish==false && (++i)<10) msleep(100);	//wait for hook end
			rc = num_register_tracepoints_for_weight();
			if(rc < 2){
				rc = tracepoint_probe_unregister(gnc.nb_tp, nb_boost_by_weight, NULL);
				if(!rc) gnc.tp_reg_weight=false;
				NB_DBG("tracepoint_probe_unregister for nb_boost_by_weight rc:%d.", rc);
			}
		}

	}else if(gnc.nb_boost[loc].type==TYPE_RB_TREE){
		if(gnc.nb_boost[loc].optype!=PULL_CANCEL && gnc.tp_reg_rbtree==false){	//need register
			rc = tracepoint_probe_register(gnc.nb_tp, nb_boost_by_rb_tree, NULL);
			if(!rc) gnc.tp_reg_rbtree=true;
			NB_DBG("tracepoint_probe_register for nb_boost_by_rb_tree rc:%d.", rc);
		}
		rc = num_register_tracepoints_for_rbtree();
		if(gnc.nb_boost[loc].optype==PULL_CANCEL &&  rc < 2){	//need unregister
			rc = tracepoint_probe_unregister(gnc.nb_tp, nb_boost_by_rb_tree, NULL);
			if(!rc) gnc.tp_reg_rbtree=false;
			NB_DBG("tracepoint_probe_unregister for nb_boost_by_rb_tree rc:%d.", rc);
		}

	}

	NB_DBG("Requst loc:%d pid:%d optype:%s boost type:%d level:%d.\n", loc, gnc.nb_boost[loc].pid, optype_str[gnc.nb_boost[loc].optype], gnc.nb_boost[loc].type, gnc.nb_boost[loc].level);
	if(gnc.nb_boost[loc].optype==PULL_CANCEL){
		gnc.nb_boost[loc].pid = 0;
		gnc.nb_boost[loc].type = 0;
		gnc.nb_boost[loc].optype = 0;
		gnc.nb_boost[loc].level = 0;
		gnc.nb_boost[loc].count = 0;
		gnc.nb_boost[loc].finish = true;
	}

	mutex_unlock(&gnc.nb_mutex);

	return count;
}
static ssize_t nb_boost_optype_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	u8 i = 0;
	u32 rc = 0;
	
	for(i=0; i<BOOST_MAX_CTRL; i++){
		rc += sprintf(buf+rc, "%d:%d\n", i, gnc.nb_boost[i].optype);
	}
	return rc;
}

static ssize_t nb_boost_optype_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	int rc = 0;
	u32 optype = 0;

	rc = kstrtoint(buf, 10, &optype);
	if (rc < 0)
		return rc;
	
	if(optype > 3){
		NB_DBG("Requst boost optype:%d is invalid!\n", optype);
		return -EFAULT;
	}
	
	mutex_lock(&gnc.nb_mutex);
	gnc.nb_boost_bak.optype = optype;
	mutex_unlock(&gnc.nb_mutex);

	return count;
}
static ssize_t nb_boost_level_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	u8 i = 0;
	u32 rc = 0;
	
	for(i=0; i<BOOST_MAX_CTRL; i++){
		rc += sprintf(buf+rc, "%d:%d\n", i, gnc.nb_boost[i].level);
	}
	return rc;
}

static ssize_t nb_boost_level_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	int rc = 0;
	u32 level = 0;

	rc = kstrtoint(buf, 10, &level);
	if (rc < 0)
		return rc;

	mutex_lock(&gnc.nb_mutex);
	gnc.nb_boost_bak.level = level;
	mutex_unlock(&gnc.nb_mutex);

	return count;
}
static ssize_t nb_boost_type_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	u8 i = 0;
	u32 rc = 0;
	
	for(i=0; i<BOOST_MAX_CTRL; i++){
		rc += sprintf(buf+rc, "%d:%d\n", i, gnc.nb_boost[i].type);
	}
	return rc;
}

static ssize_t nb_boost_type_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	int rc = 0;
	u32 type = 0;

	rc = kstrtoint(buf, 10, &type);
	if (rc < 0)
		return rc;

	mutex_lock(&gnc.nb_mutex);
	gnc.nb_boost_bak.type = type;
	mutex_unlock(&gnc.nb_mutex);

	return count;
}
static ssize_t nb_boost_dbg_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", gnc.dbg_en);
}

static ssize_t nb_boost_dbg_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	int rc = 0;
	u32 dbg = 0;

	rc = kstrtoint(buf, 10, &dbg);
	if (rc < 0)
		return rc;

	gnc.dbg_en = dbg;

	return count;
}

static  struct kobj_attribute boost_pid_attr = __ATTR(pid, 0664, nb_boost_pid_show, nb_boost_pid_store);
static  struct kobj_attribute boost_optype_attr = __ATTR(optype, 0664, nb_boost_optype_show, nb_boost_optype_store);
static  struct kobj_attribute boost_level_attr = __ATTR(level, 0664, nb_boost_level_show, nb_boost_level_store);
static  struct kobj_attribute boost_type_attr = __ATTR(type, 0664, nb_boost_type_show, nb_boost_type_store);
static  struct kobj_attribute boost_dbg_attr = __ATTR(dbg, 0664, nb_boost_dbg_show, nb_boost_dbg_store);

static struct attribute *boost_attrs[] = {
	&boost_pid_attr.attr,
    &boost_optype_attr.attr,
	&boost_level_attr.attr,
	&boost_type_attr.attr,
	&boost_dbg_attr.attr,
    NULL,
};

static struct attribute_group boost_attr_group = {
    .attrs = boost_attrs,
};

static void nb_reweight_task(struct task_struct *p, int prio)
{
	p->se.load.weight = sched_prio_to_weight[prio] << SCHED_FIXEDPOINT_SHIFT;
	p->se.load.inv_weight = sched_prio_to_wmult[prio];

	//NB_DBG("weight:%ld inv_weight:%d!\n", p->se.load.weight, p->se.load.inv_weight);
}

static void restore_tracepoints(struct tracepoint *tp, void *arg)
{
	//check_trace_callback_type_console(probe_console);
	if (strcmp(tp->name, NB_TRACEPOINT_SCHED_WAKEUP)){
		return;
	}
	gnc.nb_tp = tp;
}
static __init int nb_boost_init(void)
{
	int rc = 0;

	memset(&gnc, 0, sizeof(ST_NB_BOOST));
	mutex_init(&gnc.nb_mutex);
	
	for_each_kernel_tracepoint(restore_tracepoints, NULL);
	if(gnc.nb_tp == NULL){
		NB_DBG("Can't find %s tracepoint!\n", NB_TRACEPOINT_SCHED_WAKEUP);
		return -ENODEV;
	}

	gnc.nb_boost_kobj = kobject_create_and_add("nb_boost", kernel_kobj);
	if (!gnc.nb_boost_kobj)
	{
		NB_DBG("nb_boost kobj create error\n");
		return 0;
	}
	rc = sysfs_create_group(gnc.nb_boost_kobj, &boost_attr_group);
	if(rc)
	{
		NB_DBG("failed to create boost group attributes\n");
		return rc;
	}
	NB_DBG("Create boost group attributes success.\n");

	return 0;
}

//static void nb_boost_by_weight(struct rq *rq, struct task_struct *tsk)
static void nb_boost_by_weight(void *ignore, bool preempt, struct task_struct *prev, struct task_struct *tsk)
{
	int loc = 0;
	int prio = 0;
	
	loc = nb_boost_find_node_by_pid(tsk->pid);
	if(loc < 0){
		return;
	}

	if(gnc.nb_boost[loc].pid==0 || gnc.nb_boost[loc].type!=TYPE_WEIGHT || gnc.nb_boost[loc].finish!=false)
		return;

	if(tsk->prio < MAX_RT_PRIO){
		gnc.nb_boost[loc].finish = true;
		NB_RUN_DBG("pid:%d cpu:%d is RT pthread, ignore it!\n", tsk->pid, tsk->cpu);
		return;
	}
	gnc.nb_boost[loc].level = gnc.nb_boost[loc].level>BOOST_MAX_LEVEL?BOOST_MAX_LEVEL:gnc.nb_boost[loc].level;
	if(gnc.nb_boost[loc].optype == PULL_UP){
		prio = (tsk->prio - MAX_RT_PRIO) - ((tsk->prio - MAX_RT_PRIO)/BOOST_MAX_LEVEL) * gnc.nb_boost[loc].level;
		prio = prio<0?0:prio;
	}else if(gnc.nb_boost[loc].optype == PULL_DOWN){
		prio = (tsk->prio - MAX_RT_PRIO) + ((tsk->prio - MAX_RT_PRIO)/BOOST_MAX_LEVEL) * gnc.nb_boost[loc].level;
		prio = prio>(MAX_NICE-MIN_NICE)?(MAX_NICE-MIN_NICE):prio;
	}else if (gnc.nb_boost[loc].optype == PULL_CANCEL){
		prio = tsk->prio - MAX_RT_PRIO;
	}
	nb_reweight_task(tsk, prio);

	NB_RUN_DBG("pid:%d cpu:%d prio from %d to %d ", tsk->pid, tsk->cpu, tsk->prio - MAX_RT_PRIO, prio);
	
	gnc.nb_boost[loc].finish = true;
}

//static void nb_boost_by_rb_tree(struct rq *rq, struct task_struct *tsk)
static void nb_boost_by_rb_tree(void *ignore, bool preempt, struct task_struct *prev, struct task_struct *tsk)
{
	int loc = 0;
	u64 vruntime = tsk->se.vruntime;

	loc = nb_boost_find_node_by_pid(tsk->pid);
	if(loc < 0){
		return;
	}
	if(gnc.nb_boost[loc].pid==0 || gnc.nb_boost[loc].type!=TYPE_RB_TREE || gnc.nb_boost[loc].finish!=false)
		return;

	gnc.nb_boost[loc].count++;
	gnc.nb_boost[loc].level = gnc.nb_boost[loc].level>BOOST_MAX_LEVEL?BOOST_MAX_LEVEL:gnc.nb_boost[loc].level;
	if(gnc.nb_boost[loc].optype == PULL_UP){
		if(!(gnc.nb_boost[loc].count%(BOOST_MAX_LEVEL - gnc.nb_boost[loc].level + 1)))
			vruntime = tsk->se.vruntime/2;
	}else if(gnc.nb_boost[loc].optype == PULL_DOWN){
		if(!(gnc.nb_boost[loc].count%(BOOST_MAX_LEVEL - gnc.nb_boost[loc].level + 1)))
			vruntime = tsk->se.vruntime*2;
	}

	if(vruntime != tsk->se.vruntime)
		NB_RUN_DBG("pid:%d cpu:%d vruntime from %ld to %ld ", tsk->pid, tsk->cpu, tsk->se.vruntime, vruntime);

	tsk->se.vruntime = vruntime;
}
#if 0
static s64 nb_boost_by_tick(struct rq *rq, struct task_struct *tsk, s64 delta)
{
	int loc = 0;
	s64 boost_delta = 0;

	loc = nb_boost_find_node_by_pid(tsk->pid);
	if(loc < 0){
		return;
	}
	if(gnc.nb_boost[loc].pid==0 || gnc.nb_boost[loc].type!=TYPE_TICK || gnc.nb_boost[loc].finish!=false)
		return;

	/*leve between 1-4*/
	if(gnc.nb_boost[loc].optype==PULL_UP){
		boost_delta = delta - delta/5*gnc.nb_boost[loc].level;
		boost_delta = boost_delta <= 0 ? delta:boost_delta;
	}else if(gnc.nb_boost[loc].optype == PULL_DOWN){
		boost_delta = delta + delta/5*gnc.nb_boost[loc].level;
		boost_delta = boost_delta > (delta*2) ? delta:boost_delta;
	}

	NB_RUN_DBG("%s pid:%d cpu:%d  delta:%ld boost_delta:%ld level:%d", __func__, tsk->pid, tsk->cpu, delta, boost_delta, gnc.nb_boost[loc].level);

	return  boost_delta;
}
#endif
static void __exit nb_boost_exit(void)
{
	sysfs_remove_group(gnc.nb_boost_kobj, &boost_attr_group);

	mutex_destroy(&gnc.nb_mutex);

	if(gnc.tp_reg_rbtree)
		tracepoint_probe_unregister(gnc.nb_tp, nb_boost_by_rb_tree, NULL);
	if(gnc.tp_reg_weight)
		tracepoint_probe_unregister(gnc.nb_tp, nb_boost_by_weight, NULL);

}

module_init(nb_boost_init);
module_exit(nb_boost_exit);


MODULE_DESCRIPTION("NUBIA BOOST Driver");
MODULE_LICENSE("GPL");
